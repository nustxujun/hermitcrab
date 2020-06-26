#include "RenderGraph.h"
#include "Profile.h"
#include "ResourceViewAllocator.h"


ResourceHandle::Ptr ResourceHandle::create(Renderer::ViewType type, int w, int h, DXGI_FORMAT format)
{
	return create(type, w, h, 1, format);
}

ResourceHandle::Ptr ResourceHandle::create(Renderer::ViewType type, int w, int h, int d, DXGI_FORMAT format)
{
	return std::make_shared<ResourceHandle>(type, w, h, d, format);
}

ResourceHandle::Ptr ResourceHandle::clone(ResourceHandle::Ptr res)
{
	return create(res->getType(),res->mWidth, res->mHeight,res->mDepth, res->mFormat);
}

ResourceHandle::ResourceHandle(Renderer::ViewType t, int w, int h, int d, DXGI_FORMAT format):
	mType(t), mWidth(w), mHeight(h), mDepth(d), mFormat(format)
{
}

ResourceHandle::~ResourceHandle()
{
	std::lock_guard<std::mutex> lock(mViewMutex);
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
	std::lock_guard<std::mutex> lock(mViewMutex);
	if (!mView)
	{
		auto ret = ResourceViewAllocator::Singleton.alloc(mWidth, mHeight, mDepth, mFormat,mType);
		mView = ret.first;
		mHashValue = ret.second;
	}
	return mView; 
}


void RenderGraph::addPass(const std::string& name, RenderPass&& callback)
{
	mPasses.push_back({name, std::move(callback)});
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

void RenderGraph::execute(Renderer::CommandQueue::Ref queue)
{
	CHECK_RENDER_THREAD;

	for (auto& pass : mPasses)
	{
		PROFILE("add pass " + pass.first, {});
		Builder b;
		auto task = pass.second(b);

		if (!b.empty())
		{
			queue->addCommand([b = std::move(b)](auto cmdlist){
				b.prepare(cmdlist);
			},  true);
		}
		if (task)
		{
			
			queue->addCommand([n = std::move(pass.first), t = std::move(task)](auto cmdlist) {
				PROFILE(n, cmdlist);
				t(cmdlist);
			},  false);
		}
	}
}


void RenderGraph::Builder::read(const ResourceHandle::Ptr& res)
{
	mTransitions.push_back({res, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, IT_NONE});
}

void RenderGraph::Builder::write(const ResourceHandle::Ptr& res, InitialType type)
{
	if (res->getType() == Renderer::VT_DEPTHSTENCIL)
		mTransitions.push_back({res, D3D12_RESOURCE_STATE_DEPTH_WRITE , type});
	else
		mTransitions.push_back({ res, D3D12_RESOURCE_STATE_RENDER_TARGET , type });
}

void RenderGraph::Builder::access(const ResourceHandle::Ptr& res, InitialType type)
{
	mTransitions.push_back({res, D3D12_RESOURCE_STATE_UNORDERED_ACCESS , type });
}

void RenderGraph::Builder::copy(const ResourceHandle::Ptr& src, const ResourceHandle::Ptr& dst)
{
	if (src)
		mTransitions.push_back({ src, D3D12_RESOURCE_STATE_COPY_SOURCE, IT_NONE });
	if (dst)
		mTransitions.push_back({ src, D3D12_RESOURCE_STATE_COPY_DEST, IT_DISCARD});

}

void RenderGraph::Builder::prepare(Renderer::CommandList::Ref cmdlist)const
{
	std::vector<ResourceHandle::Ptr> uavBarriers;
	for (auto& t : mTransitions)
	{
		cmdlist->transitionBarrier(t.res->getView(), t.state,0);
		if (t.type == IT_FENCE && t.res->getType() == Renderer::VT_UNORDEREDACCESS)
			uavBarriers.emplace_back(t.res);
	}

	for (auto& t : uavBarriers)
	{
		cmdlist->uavBarrier(t->getView());
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

void RenderGraph::Barrier::addRenderTask(const std::string& name, RenderPass&& callback)
{
	std::lock_guard<std::mutex> lock(mMutex);
	mPasses->push_back({ name, std::move(callback) });

}


RenderGraph::Barrier::Barrier()
{
	mFence = FenceObject::Ptr(new FenceObject());
	mPasses = decltype(mPasses)(new Passes());
}

void RenderGraph::Barrier::execute()
{
	//mFence->wait();

	//auto r = Renderer::getSingleton();
	//std::lock_guard<std::mutex> lock(mMutex);
	//r->addRenderTask([fence = mFence, passes = mPasses](auto cmdlist)mutable {
	//	PROFILE("barrier", {});
	//	{
	//		PROFILE("barrier waitting", {});
	//		fence->wait();
	//	}
	//	while(!tasks->empty())
	//	{
	//		auto& t = tasks->front();
	//		PROFILE(t.first, cmdlist);
	//		t.second(cmdlist);
	//		tasks->pop_front();
	//	}
	//});
}
