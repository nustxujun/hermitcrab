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
		ResourceViewAllocator::Singleton.recycle(mView);
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
		mView = ResourceViewAllocator::Singleton.alloc(mWidth, mHeight, 1, mFormat,mType);
	return mView; 
}


void RenderGraph::addPass(const std::string& name, std::function<RenderTask(Builder&)>&& callback)
{
	Builder b;
	auto task = callback(b);
	if (task)
		mTasks.emplace_back(name,[task = std::move(task), b = std::move(b), name](auto cmdlist) mutable
		{
			PROFILE(name, cmdlist);
			{
				PROFILE("prepare", cmdlist);
			b.prepare(cmdlist);
			}
			{
			PROFILE("task", cmdlist);
			task(cmdlist);
			}
		});
}


void RenderGraph::execute()
{
	Renderer::getSingleton()->addRenderTask([tasks = std::move(mTasks)](auto cmdlist) mutable
	{
		PROFILE("cmdlist", cmdlist);
		while(!tasks.empty())
		{
			tasks.begin()->second(cmdlist);
			tasks.pop_front();
		}
	},true);
}

//RenderGraph::BeginPass::BeginPass()
//{
//	setName("begin pass");
//	mRenderTarget = ResourceHandle::create(Renderer::VT_RENDERTARGET,0,0,DXGI_FORMAT_UNKNOWN);
//	mRenderTarget->setClearValue({0,0,0,0});
//	mDepthStencil = ResourceHandle::create(Renderer::VT_DEPTHSTENCIL, 0,0, DXGI_FORMAT_D24_UNORM_S8_UINT);
//	mDepthStencil->setClearValue({1.0f, 0});
//}


void RenderGraph::Builder::read(const ResourceHandle::Ptr& res, D3D12_RESOURCE_STATES state)
{
	mTransitions.push_back({res, state, IT_NONE});
}

void RenderGraph::Builder::write(const ResourceHandle::Ptr& res, InitialType type, D3D12_RESOURCE_STATES state)
{
	mTransitions.push_back({res, state, type});
}

void RenderGraph::Builder::prepare(Renderer::CommandList::Ref cmdlist)
{

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


