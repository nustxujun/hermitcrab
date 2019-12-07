#include "Pipeline.h"
#include "Renderer.h"
#include "RenderContext.h"



RenderGraph::LambdaRenderPass Pipeline::clearDepth(const Color & color)
{
	return RenderGraph::LambdaRenderPass({},[color](auto* pass, const auto& inputs) {
		//pass->write(inputs[0]->getRenderTarget());
		}, []()
		{
			//Renderer::getSingleton()->getCommandList()->clearRenderTarget(inputs[0], { 0.5f,0.5f, 0.5f,1.0f });
		});
}

RenderGraph::LambdaRenderPass Pipeline::present()
{
	UpValue<ResourceHandle::Ptr> res;
	return RenderGraph::LambdaRenderPass({},[res](auto* pass, const auto& inputs)mutable {
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


		});
}

RenderGraph::LambdaRenderPass Pipeline::drawScene(Camera::Ptr cam, UINT flags , UINT mask)
{
	return RenderGraph::LambdaRenderPass({},[](auto* pass, const auto& inputs) {
		pass->write(inputs[0]->getRenderTarget());
		},[=](){
			RenderContext::getSingleton()->renderScene(cam,flags, mask);
		} );
}


void ForwardPipeline::update()
{
	RenderGraph graph;
	auto renderer = Renderer::getSingleton();

	auto clearPass = clearDepth({ 0.5,0.5,0.5,1 });
	auto presentPass = present();
	auto drawScenePass = drawScene(RenderContext::getSingleton()->getMainCamera());
	graph.begin()>> drawScenePass >> gui()>> presentPass;


	graph.setup();
	graph.compile();
	graph.execute();
}