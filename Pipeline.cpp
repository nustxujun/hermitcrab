#include "Pipeline.h"
#include "Renderer.h"
#include "RenderContext.h"
#include <sstream>
#include "Profile.h"
#include "ResourceViewAllocator.h"

Renderer::RenderTask Pipeline::present(RenderGraph::Builder& b, ResourceHandle::Ptr src)
{
	static Quad::Ptr quad ;
	if (!quad)
	{
		quad = Quad::Ptr(new Quad());
		auto rs = Renderer::RenderState::Default;
		rs.setRenderTargetFormat({DXGI_FORMAT_R8G8B8A8_UNORM});
		quad->init("shaders/drawtexture.hlsl",rs);
	}
	auto tmp = quad;
	b.read(src, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	return  [ src](auto cmdlist)mutable
		{
			auto renderer = Renderer::getSingleton();
			
			auto bb = renderer->getBackBuffer();
			cmdlist->setRenderTarget(bb);

			quad->setResource("tex", src->getView()->getShaderResource());

			quad->fitToScreen();
			RenderContext::getSingleton()->renderScreen(quad.get(), cmdlist);

		};
}

//RenderGraph::LambdaRenderPass::Ptr Pipeline::drawScene(Camera::Ptr cam, UINT flags , UINT mask)
//{
//	//auto pass = RenderGraph::LambdaRenderPass::Ptr{new RenderGraph::LambdaRenderPass({},[](auto* pass, const auto& inputs) {
//	//	pass->write(inputs[0]->getRenderTarget());
//	//	pass->write(inputs[0]->getDepthStencil());
//
//	//	},[=](auto srvs){
//	//		RenderContext::getSingleton()->renderScene(cam,flags, mask);
//	//	} )};
//
//	//pass->setName("draw scene");
//	return {};
//}

//RenderGraph::LambdaRenderPass::Ptr Pipeline::postprocess(const std::string& ps, const std::function<void(Renderer::PipelineState::Ref)>& prepare)
//{
//	//std::shared_ptr<Quad> quad = std::shared_ptr<Quad>(new Quad());
//	//auto rs = Renderer::RenderState::Default;
//	//quad->init(ps,rs);
//
//	//auto pass = RenderGraph::LambdaRenderPass::Ptr{ new  RenderGraph::LambdaRenderPass({},[=](auto* pass, const auto& inputs) {
//	//		pass->read(inputs[0]->getRenderTarget());
//	//		pass->write(ResourceHandle::create(Renderer::VT_RENDERTARGET, 0, 0, DXGI_FORMAT_UNKNOWN));
//	//	}, [quad, prepare](auto srvs)
//	//	{
//	//		auto pso = quad->getPipelineState();
//	//		if (prepare)
//	//			prepare(pso);
//	//		quad->fitToScreen();
//	//		quad->setResource("frame",srvs[0]->getView()->getShaderResource());
//	//		//pso->setPSResource("frame",srvs[0]->getView()->getHandle());
//	//		RenderContext::getSingleton()->renderScreen(quad.get());
//	//	}) };
//	//pass->setName(ps);
//	//return pass;
//	return {};
//}


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
	//mDrawScene = drawScene(RenderContext::getSingleton()->getMainCamera());
	//mColorGrading = postprocess("shaders/colorgrading.hlsl");

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
		ImGui::RadioButton("emissive color", &cur, 6);

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

	mProfileWindow->drawCallback = [profiles = std::move(Profile::Singleton.output())](auto gui) {
		for (auto& profile: profiles)
			ImGui::Text("%s: cpu: %f ms, gpu: %f ms", profile.name.c_str(), profile.cpu, profile.gpu );
		return true;
	};

	Profile::Singleton.reset();
	auto r = Renderer::getSingleton();
	auto s = r->getSize();
	auto rt = ResourceHandle::create(Renderer::VT_RENDERTARGET, s[0], s[1], DXGI_FORMAT_R8G8B8A8_UNORM);
	auto d = ResourceHandle::create(Renderer::VT_DEPTHSTENCIL, s[0], s[1], DXGI_FORMAT_D24_UNORM_S8_UINT);

	rt->setClearValue({0,0,0,0});
	d->setClearValue({1.0f, 0});
	graph.addPass("scene", [this, rt, d](auto& builder) {
		builder.write(rt, RenderGraph::Builder::IT_CLEAR, D3D12_RESOURCE_STATE_RENDER_TARGET);
		builder.write(d, RenderGraph::Builder::IT_CLEAR, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		return [=](auto cmdlist) {
			auto context = RenderContext::getSingleton();
			auto cam = context->getMainCamera();
			cmdlist->setRenderTarget(rt->getView(), d->getView());
			context->renderScene(cmdlist,cam);
		};
	});


	graph.addPass("gui",[this, rt](auto& builder) {
		builder.write(rt, RenderGraph::Builder::IT_NONE, D3D12_RESOURCE_STATE_RENDER_TARGET);

		auto task = mGui.execute();
		return [this, rt, task = std::move(task)](auto cmdlist){
			cmdlist->setRenderTarget(rt->getView());
			task(cmdlist);
		};
	});

	graph.addPass("present", [this, rt](auto& builder) {
		builder.read(rt, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		return present(builder, rt);
	});

	graph.execute();
}
