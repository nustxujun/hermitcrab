#include "Pipeline.h"
#include "Renderer.h"
#include "RenderContext.h"
#include <sstream>


RenderGraph::LambdaRenderPass::Ptr Pipeline::present()
{
	Quad::Ptr quad = Quad::Ptr(new Quad());
	auto rs = Renderer::RenderState::Default;
	rs.setRenderTargetFormat({DXGI_FORMAT_R8G8B8A8_UNORM});
	quad->init("shaders/drawtexture.hlsl",rs);
	auto pass = RenderGraph::LambdaRenderPass::Ptr{new  RenderGraph::LambdaRenderPass({},[](auto* pass, const auto& inputs)mutable {
			pass->read(inputs[0]->getRenderTarget());
		}, [quad](auto srvs)mutable
		{

			auto renderer = Renderer::getSingleton();
			auto cmdlist = renderer->getCommandList();
			auto bb = renderer->getBackBuffer();
			cmdlist->setRenderTarget(bb);

			for (UINT i = 0; i < srvs.size(); ++i)
			{
				if (srvs[i])
				{
					quad->setResource(i, srvs[i]->getView()->getTexture()->getShaderResource());
				}
			}
			RenderContext::getSingleton()->renderScreen(quad.get());


			//auto renderer = Renderer::getSingleton();
			//auto device = renderer->getDevice();
			//auto cmdlist = renderer->getCommandList();
			//auto bb = renderer->getBackBuffer();
			//auto src = srvs[0]->getView()->getTexture();
			////cmdlist->copyTexture(bb->getTexture(),0,{0,0,0},src,0,nullptr);
			//cmdlist->transitionTo(bb->getTexture(), D3D12_RESOURCE_STATE_COPY_DEST);
			////cmdlist->transitionTo(src, D3D12_RESOURCE_STATE_RENDER_TARGET);
			//cmdlist->transitionTo(src, D3D12_RESOURCE_STATE_COPY_SOURCE);
			//cmdlist->get()->CopyResource(bb->getTexture()->get(), src->get());
			//cmdlist->transitionTo(bb->getTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			//cmdlist->transitionTo(src, D3D12_RESOURCE_STATE_RENDER_TARGET);

		})};
	pass->setName("present pass");
	return pass;
}

RenderGraph::LambdaRenderPass::Ptr Pipeline::drawScene(Camera::Ptr cam, UINT flags , UINT mask)
{
	auto pass = RenderGraph::LambdaRenderPass::Ptr{new RenderGraph::LambdaRenderPass({},[](auto* pass, const auto& inputs) {
		pass->write(inputs[0]->getRenderTarget());
		pass->write(inputs[0]->getDepthStencil());

		},[=](auto srvs){
			RenderContext::getSingleton()->renderScene(cam,flags, mask);
		} )};

	pass->setName("draw scene pass");
	return pass;
}

RenderGraph::LambdaRenderPass::Ptr Pipeline::postprocess(const std::string& ps, const std::function<void(Renderer::PipelineState::Ref)>& prepare)
{
	std::shared_ptr<Quad> quad = std::shared_ptr<Quad>(new Quad());
	auto rs = Renderer::RenderState::Default;
	quad->init(ps,rs);

	auto pass = RenderGraph::LambdaRenderPass::Ptr{ new  RenderGraph::LambdaRenderPass({},[=](auto* pass, const auto& inputs) {
			pass->read(inputs[0]->getRenderTarget());
			pass->write(ResourceHandle::create(Renderer::VT_RENDERTARGET, 0, 0, DXGI_FORMAT_UNKNOWN));
		}, [quad, prepare](auto srvs)
		{
			auto pso = quad->getPipelineState();
			if (prepare)
				prepare(pso);
			for (UINT i = 0; i < srvs.size();++i)
			{
				if (srvs[i])
					pso->setPSResource(i,srvs[i]->getView()->getTexture()->getShaderResource());
			}
			//pso->setPSResource("frame",srvs[0]->getView()->getHandle());
			RenderContext::getSingleton()->renderScreen(quad.get());
		}) };
	pass->setName(ps);
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
	mColorGrading = postprocess("shaders/colorgrading.hlsl");

	mProfileWindow = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("profile");
	mDebugInfo = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("debuginfo");
	mDebugInfo->drawCallback = [](ImGuiOverlay::ImGuiObject* gui) {
		const auto& debuginfo = Renderer::getSingleton()->getDebugInfo();
		ImGui::Text("drawcall count: %d",debuginfo.drawcallCount);
		ImGui::Text("primitive count: %d", debuginfo.primitiveCount);
		ImGui::Text("resources count: %d", debuginfo.numResources);
		ImGui::Text("transient texture count: %d", debuginfo.numTransientOnUse);
		ImGui::Text("video memroy: %d", debuginfo.videoMemory);

		return true;
	};

	mRenderSettingsWnd = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("rendersettings");
	mRenderSettingsWnd->drawCallback = [&settings = mSettings](auto gui) {
		ImGui::Checkbox("color grading", &settings.colorGrading);

		return true;
	};

	mVisualizationWnd = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("visualization");
	mVisualizationWnd->drawCallback = [&visual = mSettings](auto gui) {
		int v = 0;
		static int selected = 0;
		ImGui::RadioButton("final", &v, 0);
		ImGui::RadioButton("base color", &v, 1);
		ImGui::RadioButton("normal", &v, 2);

		if (v != selected)
		{
			RenderContext::getSingleton()->recompileMaterials((Material::Visualizaion)v);
			selected = v;
		}

		return true;
	};

	auto mainbar = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiMenuBar>(true);
	auto addbutton = [&](const std::string& name, ImGuiOverlay::ImGuiObject* win){
		mainbar->createChild<ImGuiOverlay::ImGuiButton>(name)->callback = [=](auto button) {
			win->visible = !win->visible;
		};
	};

	addbutton("profile", mProfileWindow);
	addbutton("debuginfo", mDebugInfo);
	addbutton("settings", mRenderSettingsWnd);
	addbutton("visualizations", mVisualizationWnd);

}

void DefaultPipeline::update()
{
	RenderGraph graph;
	auto& next = graph.begin() >> mDrawScene;
	if (mSettings.colorGrading)
		next >> mColorGrading >> mGui >> mPresent;
	else 
		next >> mGui >> mPresent;

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
					profilewin->createChild<ImGuiOverlay::ImGuiText>(""),
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
