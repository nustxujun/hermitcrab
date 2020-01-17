#include "RenderGraph.h"



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


void ResourceHandle::addRef()
{
	mRefCount++;
}

void ResourceHandle::release()
{
	if (mRefCount == 0)
	{
		mView.reset();
		return;
	}

	mRefCount--;
}

void ResourceHandle::prepare()
{
	Common::Assert(mRefCount != 0,"invalied resource");

	if (!mView)
		mView = Renderer::getSingleton()->createResourceView(mWidth, mHeight,mFormat,mType, Renderer::Resource::RT_TRANSIENT);
}


RenderGraph::RenderPass & RenderGraph::begin()
{
	mPasses.push_back({});
	return mPasses[mPasses.size() - 1];
}

void RenderGraph::setup(const Visitor& prev , const Visitor& post )
{
	mCurrents.clear();
	std::set<RenderPass* > map;
	auto find = [&map](RenderPass* p)
	{
		return map.find(p) != map.end();
	};

	std::function<void (RenderPass*)> f;
	f = [&list = mCurrents, &f, &find](RenderPass* p) {
		if (find(p))
			return ;
		list.push_back(p);
		p->visitOutputs([&f](RenderPass* p){
			f(p);
		});
	};

	for (auto& p : mPasses)
	{
		f(&p);
	}

	for (auto& p: mCurrents)
	{
		if (prev)
			prev(p);
		p->setup();
		if (post)
			post(p);
	}
}

void RenderGraph::compile(const Visitor& prev , const Visitor& post )
{
	for (auto& p : mCurrents)
	{
		Inputs inputs;
		for (auto& i : p->mInputs)
		{
			inputs.push_back(&i->mResources);
		}
		if (prev)
			prev(p);
		p->compile(inputs);
		if (post)
			post(p);
	}
}

void RenderGraph::execute(const Visitor& prev, const Visitor& post)
{
	std::list<RenderPass*> queue;
	std::function<void(RenderPass*)> f;
	f = [&queue,&f, &prev, & post](RenderPass* p)
	{
		if (p->isPrepared())
		{
			p->prepareResources();
			if(prev) prev(p);
			p->execute();
			if (post) post(p);
			p->release();
			p->visitOutputs([&f](RenderPass* p ){
				f(p);
			});
			p->clear();
		}
		else
		{
			queue.push_back(p);
		}
	};

	for (auto& p : mPasses)
	{
		f(&p);
	}
}

RenderGraph::RenderPass & RenderGraph::RenderPass::operator>>(RenderPass::Ptr pass)
{
	return *this >> *pass;
}

RenderGraph::RenderPass & RenderGraph::RenderPass::operator>>(RenderPass & pass)
{

	pass.addInput(this);
	addOutput(&pass);

	return pass;
}

void RenderGraph::RenderPass::read(ResourceHandle::Ptr res, UINT slot)
{
	Common::Assert((bool)res,"resource is null");
	if (slot >= mShaderResources.size())
		mShaderResources.resize(slot + 1);
	mShaderResources[slot] = res;
	res->addRef();
}

void RenderGraph::RenderPass::write(ResourceHandle::Ptr rt, InitialType type, UINT slot)
{
	mResources.add(rt, slot);
	mInitialTypes[&(*rt)] = type;
}

void RenderGraph::RenderPass::visitOutputs(const std::function<void(RenderPass*)>& visitor)
{
	for(auto& p: mOutputs)
		visitor(p);
}
bool RenderGraph::RenderPass::isPrepared() const
{
	//for (auto& rt: mShaderResources)
	//{
	//	if (!rt->isCompleted())
	//		return false;
	//}
	return true;
}

void RenderGraph::RenderPass::release()
{
	for (auto&i : mShaderResources)
		if (i)
			i->release();
	mShaderResources.clear();
	mResources.clear();
}

void RenderGraph::RenderPass::clear()
{
	mInputs.clear();
	mOutputs.clear();
}

void RenderGraph::RenderPass::addInput(RenderPass * p)
{
	mInputs.push_back(p);
}

void RenderGraph::RenderPass::addOutput(RenderPass * p)
{
	mOutputs.push_back(p);
}

void RenderGraph::RenderPass::prepareResources()
{

	auto cmdlist = Renderer::getSingleton()->getCommandList();
	std::vector<Renderer::ResourceView::Ref> rtvs;
	std::vector<std::function<void(void)>> clears;
	for (size_t i = 0; i < mResources.getNumRenderTargets(); ++i)
	{
		auto rt = mResources.getRenderTarget(i);
		rt->prepare();
		switch (mInitialTypes[&(*rt)])
		{
		case IT_CLEAR: 
			clears.emplace_back([&](){cmdlist->clearRenderTarget(rt->getView(), rt->getClearValue().color); });
			break;
		case IT_DISCARD: 
			clears.emplace_back([&]() {cmdlist->discardResource(rt->getView());});
			break;
		}
		
		cmdlist->transitionTo(rt->getView()->getTexture(),D3D12_RESOURCE_STATE_RENDER_TARGET);
		rtvs.push_back(rt->getView());
	}

	auto ds = mResources.getDepthStencil();
	Renderer::ResourceView::Ref dsv;
	if (ds)
	{
		ds->prepare();
		const auto& clearvalue = ds->getClearValue();
		switch (mInitialTypes[&(*ds)])
		{
		case IT_CLEAR: 
			clears.emplace_back([&]() {cmdlist->clearDepthStencil(ds->getView(), clearvalue.depth, clearvalue.stencil);});
			break;
		case IT_DISCARD: 
			
			clears.emplace_back([&]() {cmdlist->discardResource(ds->getView()); });
			break;
		}

		cmdlist->transitionTo(ds->getView()->getTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		dsv = ds->getView();
	}

	for (auto& srv : mShaderResources)
	{
		cmdlist->transitionTo(srv->getView()->getTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	cmdlist->flushResourceBarrier();
	cmdlist->setRenderTargets(rtvs, dsv);

	for(auto&c: clears)
		c();
}

ResourceHandle::Ptr RenderGraph::Resources::getRenderTarget(size_t index) const
{
	return mRenderTargets[index];
}

ResourceHandle::Ptr RenderGraph::Resources::getDepthStencil() const
{
	return mDepthStencil;
}


void RenderGraph::Resources::add(ResourceHandle::Ptr res, UINT slot)
{
	if (res->getType() == Renderer::VT_RENDERTARGET)
	{
		if (slot >= mRenderTargets.size())
			mRenderTargets.resize(slot + 1);
		mRenderTargets[slot] = res;

	}
	else if (res->getType() == Renderer::VT_DEPTHSTENCIL)
	{
		mDepthStencil = res;
	}

	if (!res->getName().empty())
		mResourceMap[res->getName()] = res;
	res->addRef();

}

void RenderGraph::Resources::clear()
{
	for (auto& i : mRenderTargets)
	{
		if (i)
			i->release();
	}
	mRenderTargets.clear();
	if (mDepthStencil)
		mDepthStencil->release();
	mDepthStencil.reset();
	mResourceMap.clear();
}

bool RenderGraph::Resources::check(const ResourceHandle::Ptr& res)
{
	for (auto& rt : mRenderTargets)
	{
		if (rt->getView() == res->getView())
			return true;
	}
	return mDepthStencil && mDepthStencil->getView() == res->getView();
}

RenderGraph::LambdaRenderPass::LambdaRenderPass(LambdaRenderPass && lrp):
	mSetup(std::move(lrp.mSetup)), mExecute(std::move(lrp.mExecute)), mCompile(std::move(lrp.mCompile))
{
}

void RenderGraph::LambdaRenderPass::operator=(LambdaRenderPass && lrp)
{
	mSetup = std::move(lrp.mSetup);
	mExecute = std::move(lrp.mExecute);
	mCompile = std::move(lrp.mCompile);
}

RenderGraph::LambdaRenderPass::LambdaRenderPass(const SetupFunc& setup, const CompileFunc& compile, const ExecuteFunc& exe):
	mSetup(setup), mExecute(exe),mCompile(compile)
{
}

void RenderGraph::LambdaRenderPass::setup()
{
	if (mSetup)
		mSetup(this);
}

void RenderGraph::LambdaRenderPass::compile(const Inputs & inputs)
{
	if (mCompile)
		mCompile(this, inputs);
}

void RenderGraph::LambdaRenderPass::execute()
{	
	if (mExecute)
		mExecute(mShaderResources);
}

RenderGraph::BeginPass::BeginPass()
{
	setName("begin pass");
	mRenderTarget = ResourceHandle::create(Renderer::VT_RENDERTARGET,0,0,DXGI_FORMAT_UNKNOWN);
	mRenderTarget->setClearValue({0,0,0,0});
	mDepthStencil = ResourceHandle::create(Renderer::VT_DEPTHSTENCIL, 0,0, DXGI_FORMAT_D24_UNORM_S8_UINT);
	mDepthStencil->setClearValue({1.0f, 0});
}

void RenderGraph::BeginPass::compile(const Inputs& inputs)
{
	write(mRenderTarget, IT_CLEAR);
	write(mDepthStencil, IT_CLEAR);
}
