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

		
			quad->setResource("tex",srvs[0]->getView()->getShaderResource());
		
			quad->fitToScreen();
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
	pass->setName("present");
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

	pass->setName("draw scene");
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
			quad->fitToScreen();
			quad->setResource("frame",srvs[0]->getView()->getShaderResource());
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
		ImGui::Text("adapter: %s", debuginfo.adapter.c_str());
		ImGui::Text("drawcall count: %d",debuginfo.drawcallCount);
		ImGui::Text("primitive count: %d", debuginfo.primitiveCount);
		ImGui::Text("resources count: %d", debuginfo.numResources);

		ImGui::Text("video memroy: %d MB", debuginfo.videoMemory / 1024 / 1024);

		return true;
	};

	mRenderSettingsWnd = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("rendersettings");
	mRenderSettingsWnd->drawCallback = [&settings = mSettings](auto gui) {
		ImGui::Checkbox("color grading", &settings.colorGrading);

		return true;
	};

	mVisualizationWnd = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("visualization");
	mVisualizationWnd->drawCallback = [&visual = mSettings](auto gui) {
		static int cur = 0;
		static int selected = 0;
		ImGui::RadioButton("final", &cur, 0);
		ImGui::RadioButton("vertex color", &cur, 1);
		ImGui::RadioButton("base color", &cur, 2);
		ImGui::RadioButton("roughness", &cur, 3);
		ImGui::RadioButton("metallic", &cur, 4);

		ImGui::RadioButton("normal", &cur, 5);

		if (cur != selected)
		{
			RenderContext::getSingleton()->recompileMaterials((Material::Visualizaion)cur);
			selected = cur;
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
	auto* next = &(graph.begin() >> mDrawScene );
	if (mSettings.colorGrading)
		next = &next->operator>>( mColorGrading); 
	
	next->operator>>(mGui) >> mPresent;



	graph.setup();
	graph.compile();
	graph.execute(
		[&profiles = mProfiles, profilewin = mProfileWindow](RenderGraph::RenderPass* pass) {
			if (pass->getName() == "begin pass" || 
				pass->getName().empty())
				return ;
			if (!profiles[pass->getName()].first )
			{
				profiles[pass->getName()] = {
					Renderer::getSingleton()->createProfile(),
					profilewin->createChild<ImGuiOverlay::ImGuiText>(""),
				};
			}
			profiles[pass->getName()].first->begin();
		},
		[&profiles = mProfiles](RenderGraph::RenderPass* pass)
		{
			if (!profiles[pass->getName()].first)
				return;
			auto& p = profiles[pass->getName()].first;
			p->end();
			std::stringstream ss;
			ss.precision(3);
			ss << pass->getName() << ": " << p->getCPUTime()<< "(" << p->getGPUTime() << ")" << "ms";
			profiles[pass->getName()].second->text = ss.str();
		}
	);
}
