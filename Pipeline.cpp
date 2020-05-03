#include "Pipeline.h"
#include "Renderer.h"
#include "RenderContext.h"
#include <sstream>
#include "Profile.h"
#include "ResourceViewAllocator.h"


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

Pipeline::RenderPass Pipeline::postprocess(const std::string& ps, DXGI_FORMAT fmt)
{
	Quad::Ptr quad = Quad::Ptr(new Quad());
	auto rs = Renderer::RenderState::Default;
	rs.setRenderTargetFormat({fmt});
	quad->init(ps,rs);

	return [quad](auto cmdlist, auto& srvs, auto& rtvs, auto dsv)
	{
		cmdlist->setRenderTargets(rtvs, dsv);

		for (auto& i : srvs)
			quad->setResource(i.first, i.second);

		quad->fitToScreen();
		RenderContext::getSingleton()->renderScreen(quad.get(), cmdlist);
	};
}


DefaultPipeline::DefaultPipeline()
{

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
		ImGui::Checkbox("tonemapping", &settings.tonemapping);

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


	mPasses["present"] = postprocess("shaders/drawtexture.hlsl", DXGI_FORMAT_R8G8B8A8_UNORM);
	mPasses["tone"] = postprocess("shaders/tonemapping.hlsl", DXGI_FORMAT_R8G8B8A8_UNORM);
}

void DefaultPipeline::update()
{
	PROFILE("pipeline", {});
	RenderGraph graph;

	mProfileWindow->drawCallback = [profiles = std::move(ProfileMgr::Singleton.output())](auto gui) {
		for (auto& profile: profiles)
			ImGui::Text("%s: cpu: %.3f ms, gpu: %.3f ms", profile.name.c_str(), profile.cpu, profile.gpu );
		return true;
	};

	auto r = Renderer::getSingleton();
	auto s = r->getSize();
	auto hdr = ResourceHandle::create(Renderer::VT_RENDERTARGET, s[0], s[1], DXGI_FORMAT_R16G16B16A16_FLOAT);
	auto d = ResourceHandle::create(Renderer::VT_DEPTHSTENCIL, s[0], s[1], DXGI_FORMAT_D24_UNORM_S8_UINT);

	hdr->setClearValue({0,0,0,0});
	d->setClearValue({1.0f, 0});
	graph.addPass("scene", [this, rt = hdr, d](auto& builder) {
		builder.write(rt, RenderGraph::Builder::IT_CLEAR, D3D12_RESOURCE_STATE_RENDER_TARGET);
		builder.write(d, RenderGraph::Builder::IT_CLEAR, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		return [=](auto cmdlist) {
			auto context = RenderContext::getSingleton();
			auto cam = context->getMainCamera();
			cmdlist->setRenderTarget(rt->getView(), d->getView());
			context->renderScene(cmdlist,cam);
		};
	});

	auto rt = hdr;

	if (mSettings.tonemapping)
	{ 
		auto ldr = ResourceHandle::create(Renderer::VT_RENDERTARGET, s[0], s[1], DXGI_FORMAT_R8G8B8A8_UNORM);
		rt = ldr;
		graph.addPass("tone", [this, src = hdr, dst = ldr](auto& builder) {
			builder.write(dst, RenderGraph::Builder::IT_NONE, D3D12_RESOURCE_STATE_RENDER_TARGET);
			builder.read(src, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			return [this, dst, src](auto cmdlist) {
				mPasses["tone"](cmdlist, { {"frame",src->getView()->getShaderResource()} }, { dst->getView()->getRenderTarget() }, {});
			};
		});
	
	}


	graph.addPass("gui",[this, dst = rt](auto& builder) {
		builder.write(dst, RenderGraph::Builder::IT_NONE, D3D12_RESOURCE_STATE_RENDER_TARGET);

		auto task = mGui.execute();
		return [this, dst, task = std::move(task)](auto cmdlist){
			cmdlist->setRenderTarget(dst->getView());
			task(cmdlist);
		};
	});

	graph.addPass("present", [this, src = rt](auto& builder) {
		builder.read(src, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		return [this, src](auto cmdlist) mutable{
			auto renderer = Renderer::getSingleton();
			auto bb = renderer->getBackBuffer();
			mPasses["present"](cmdlist, { {"tex", src->getView()->getShaderResource()} },{bb->getRenderTarget() }, {});
		};
	});

	graph.execute();
}
