#include "RenderGraph.h"
#include "Profile.h"
#include "ResourceViewAllocator.h"


ResourceHandle::Ptr ResourceHandle::create(Renderer::ViewType type, int w, int h, DXGI_FORMAT format)
{
	return std::make_shared<ResourceHandle>(type, w, h, format);
}

ResourceHandle::Ptr ResourceHandle::clone(ResourceHandle::Ptr res)
{
	return create(res->getType(),res->mWidth, res->mHeight,res->mFormat);
}

ResourceHandle::ResourceHandle(Renderer::ViewType t, int w, int h, DXGI_FORMAT format):
	mType(t), mWidth(w), mHeight(h), mFormat(format)
{
}

ResourceHandle::~ResourceHandle()
{
	if (mView)
		ResourceViewAllocator::Singleton.recycle(mView, mHashValue);
	mView = {};
}

Renderer::ViewType ResourceHandle::getType() const
{
	return mType;
}

const std::wstring & ResourceHandle::getName() const
{
	return mName;
}

void ResourceHandle::setName(const std::wstring & n)
{
	mName = n;
}




void ResourceHandle::prepare()
{

}

const Renderer::Resource::Ref& ResourceHandle::getView()
{
	if (!mView)
	{
		auto ret = ResourceViewAllocator::Singleton.alloc(mWidth, mHeight, 1, mFormat,mType);
		mView = ret.first;
		mHashValue = ret.second;
	}
	return mView; 
}


void RenderGraph::addPass(const std::string& name, RenderPass&& callback)
{
	mPasses.push_back({name, std::move(callback)});
	//Builder b;
	//auto task = callback(b);
	//if (task)
	//	mTasks.emplace_back([task = std::move(task), b = std::move(b), name](auto cmdlist) mutable
	//	{
	//		PROFILE(name, cmdlist);
	//			
	//		b.prepare(cmdlist);
	//		task(cmdlist);
	//	});
}


RenderGraph::Barrier::Ptr RenderGraph::addBarrier(const std::string& name)
{
	auto barrier = Barrier::Ptr(new Barrier);
	mPasses.push_back({name, [barrier](auto&b )->RenderTask{
		barrier->execute();
		return {};
	}});
	return barrier;
}

void RenderGraph::execute()
{
	auto r = Renderer::getSingleton();
	for (auto& pass : mPasses)
	{
		Builder b;
		auto task = pass.second(b);
		if (task)
		{
			r->addRenderTask([n = std::move(pass.first), t = std::move(task), b = std::move(b)](auto cmdlist) {
				PROFILE(n, cmdlist);
				b.prepare(cmdlist);
				t(cmdlist);
			});
		}
	}

}


void RenderGraph::Builder::read(const ResourceHandle::Ptr& res, D3D12_RESOURCE_STATES state)
{
	mTransitions.push_back({res, state, IT_NONE});
}

void RenderGraph::Builder::write(const ResourceHandle::Ptr& res, InitialType type, D3D12_RESOURCE_STATES state)
{
	mTransitions.push_back({res, state, type});
}

void RenderGraph::Builder::prepare(Renderer::CommandList::Ref cmdlist)const
{
	Common::Assert(Thread::isMainThread(), "builder must be in main thread");
	for (auto& t : mTransitions)
	{
		cmdlist->transitionBarrier(t.res->getView(), t.state);
	}

	cmdlist->flushResourceBarrier();


	for (auto& t : mTransitions)
	{
		auto res = t.res->getView();
		auto& cv = t.res->getClearValue();
		if (t.type == IT_CLEAR)
		{
			switch (res->getViewType())
			{
			case Renderer::VT_RENDERTARGET: cmdlist->clearRenderTarget(res, cv.color); break;
			case Renderer::VT_DEPTHSTENCIL: cmdlist->clearDepthStencil(res, cv.depth, cv.stencil); break;
			}
		}
		else if (t.type == IT_DISCARD)
		{
			cmdlist->discardResource(res);
		}
	}
}


void RenderGraph::Barrier::signal()
{
	mFence->signal();
}

void RenderGraph::Barrier::addRenderTask(const std::string& name, RenderTask&& callback)
{
	std::lock_guard<std::mutex> lock(mMutex);
	mTasks->push_back({ name, std::move(callback) });
	//Builder b;
	//auto task = callback(b);
	//if (task)
	//{
	//	std::unique_lock<std::mutex> lock(mMutex);
	//	mTasks.emplace_back([task = std::move(task), b = std::move(b), name](auto cmdlist) mutable
	//	{
	//		PROFILE(name, cmdlist);

	//		b.prepare(cmdlist);
	//		task(cmdlist);
	//	});
	//}
}


RenderGraph::Barrier::Barrier()
{
	mFence = FenceObject::Ptr(new FenceObject());
	mTasks = decltype(mTasks)(new Tasks());
}

void RenderGraph::Barrier::execute()
{
	auto r = Renderer::getSingleton();
	std::lock_guard<std::mutex> lock(mMutex);
	r->addRenderTask([fence = mFence, tasks = mTasks](auto cmdlist)mutable {
		PROFILE("barrier", {});
		{
			PROFILE("barrier waitting", {});
			fence->wait();
		}
		while(!tasks->empty())
		{
			auto& t = tasks->front();
			PROFILE(t.first, cmdlist);
			t.second(cmdlist);
			tasks->pop_front();
		}
	});
}
