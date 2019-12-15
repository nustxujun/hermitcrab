#include "Pipeline.h"
#include "Renderer.h"
#include "RenderContext.h"
#include <sstream>


RenderGraph::LambdaRenderPass::Ptr Pipeline::present()
{
	UpValue<ResourceHandle::Ptr> res;
	auto pass = RenderGraph::LambdaRenderPass::Ptr{new  RenderGraph::LambdaRenderPass({},[res](auto* pass, const auto& inputs)mutable {
			pass->read(inputs[0]->getRenderTarget());
			res = (inputs[0]->getRenderTarget());
		}, [res]()mutable
		{
			auto renderer = Renderer::getSingleton();
			auto device = renderer->getDevice();
			auto cmdlist = renderer->getCommandList();
			auto bb = renderer->getBackBuffer();
			auto src = res.get()->getView()->getTexture();
			//cmdlist->copyTexture(bb->getTexture(),0,{0,0,0},src,0,nullptr);
			cmdlist->transitionTo(bb->getTexture(), D3D12_RESOURCE_STATE_COPY_DEST);
			//cmdlist->transitionTo(src, D3D12_RESOURCE_STATE_RENDER_TARGET);
			cmdlist->transitionTo(src, D3D12_RESOURCE_STATE_COPY_SOURCE);
			cmdlist->get()->CopyResource(bb->getTexture()->get(), src->get());
			cmdlist->transitionTo(bb->getTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			cmdlist->transitionTo(src, D3D12_RESOURCE_STATE_RENDER_TARGET);


		})};
	pass->setName("present pass");
	return pass;
}

RenderGraph::LambdaRenderPass::Ptr Pipeline::drawScene(Camera::Ptr cam, UINT flags , UINT mask)
{
	auto pass = RenderGraph::LambdaRenderPass::Ptr{new RenderGraph::LambdaRenderPass({},[](auto* pass, const auto& inputs) {
		pass->write(inputs[0]->getRenderTarget());
		pass->write(inputs[0]->getDepthStencil());

		},[=](){
			RenderContext::getSingleton()->renderScene(cam,flags, mask);
		} )};

	pass->setName("draw scene pass");
	return pass;
}


//void ForwardPipeline::update()
//{
//	RenderGraph graph;
//	auto renderer = Renderer::getSingleton();
//
//	auto presentPass = present();
//	auto drawScenePass = drawScene(RenderContext::getSingleton()->getMainCamera());
//	graph.begin()>> drawScenePass >> gui()>> presentPass;
//
//
//	graph.setup();
//	graph.compile();
//	graph.execute();
//}

DefaultPipeline::DefaultPipeline()
{
	mPresent = present();
	mDrawScene = drawScene(RenderContext::getSingleton()->getMainCamera());
	mProfileWindow = ImGuiObject::root()->createChild<ImGuiWindow>("profile");

	auto mainbar = ImGuiObject::root()->createChild<ImGuiMenuBar>(true);
	mainbar->createChild<ImGuiButton>("profile")->callback = [profile = mProfileWindow](auto button) {
		profile->visible = !profile->visible;
	};
}

void DefaultPipeline::update()
{
	RenderGraph graph;
	graph.begin() >> mDrawScene >> mGui >> mPresent;

	graph.setup();
	graph.compile();
	graph.execute(
		[&profiles = mProfiles, profilewin = mProfileWindow](RenderGraph::RenderPass* pass) {
			if (pass->getName() == "begin pass")
				return ;
			if (!profiles[pass].first )
			{
				profiles[pass] = {
					Renderer::getSingleton()->createProfile(),
					profilewin->createChild<ImGuiText>(""),
				};
			}
			profiles[pass].first->begin();
		},
		[&profiles = mProfiles](RenderGraph::RenderPass* pass)
		{
			if (!profiles[pass].first)
				return;
			auto& p = profiles[pass].first;
			p->end();
			std::stringstream ss;
			ss.precision(3);
			ss << pass->getName() << ": " << p->getCPUTime()<< "(" << p->getGPUTime() << ")" << "ms";
			profiles[pass].second->text = ss.str();
		}
	);
}
