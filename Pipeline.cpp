#include "Pipeline.h"
#include "Renderer.h"
#include "RenderContext.h"
#include <sstream>
#include "Profile.h"
#include "ResourceViewAllocator.h"


Pipeline::Pipeline()
{
	mGui = ImGuiPass::Ptr(new ImGuiPass);
}

//Pipeline::RenderPass Pipeline::postprocess(const std::string& ps, DXGI_FORMAT fmt)
//{
//	Quad::Ptr quad = Quad::Ptr(new Quad());
//	auto rs = Renderer::RenderState::Default;
//	rs.setRenderTargetFormat({fmt});
//	quad->init(ps,rs);
//
//	return [quad](auto cmdlist, auto& srvs, auto& rtvs,  auto& argvs)
//	{
//		cmdlist->setRenderTargets(rtvs);
//
//		for (auto& i : srvs)
//			quad->setResource(i.first, i.second);
//
//		if (argvs)
//			argvs(quad.get());
//
//		quad->fitToScreen();
//		RenderContext::getSingleton()->renderScreen(quad.get(), cmdlist);
//	};
//}
//
//ResourceHandle::Ptr Pipeline::addPostprocessPass(RenderGraph& rg, const std::string& name, RenderPass&& f, std::map<std::string, ResourceHandle::Ptr>&& srvs, std::function<void(Quad*)>&& argvs, DXGI_FORMAT targetfmt)
//{
//	auto r = Renderer::getSingleton();
//	auto s = r->getSize();
//	auto rt = ResourceHandle::create(Renderer::VT_RENDERTARGET, s[0], s[1], targetfmt);
//
//	rg.addPass(name, [srvs = std::move(srvs), f = std::move(f), rt, argvs = std::move(argvs)](auto& builder){
//		builder.write(rt, RenderGraph::Builder::IT_NONE);
//		for (auto& t: srvs)
//			builder.read(t.second);
//		return [f = std::move(f), srvs = std::move(srvs), rt, argvs = std::move(argvs)](auto cmdlist) {
//			std::map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE> inputs;
//			for (auto& t: srvs)
//				inputs[t.first] = t.second->getView()->getShaderResource();
//			f(cmdlist, inputs, {rt->getView()->getRenderTarget()}, argvs);
//		};
//	});
//
//	return rt;
//}

void Pipeline::addPostProcessPass(const std::string& name,  DXGI_FORMAT fmt)
{
	Quad::Ptr quad = Quad::Ptr(new Quad());
	auto rs = Renderer::RenderState::Default;
	rs.setRenderTargetFormat({fmt});
	quad->init(name,rs);


	auto pass = [quad, name, fmt](std::map<std::string, ResourceHandle::Ptr>&& srvs, std::function<void(Quad*)>&& argvs)
	{
		auto size = Renderer::getSingleton()->getSize();
		auto rt = ResourceHandle::create(Renderer::VT_RENDERTARGET, size[0], size[1], fmt);
		
		return std::make_pair(rt, [name, quad, srvs = std::move(srvs), argvs = std::move(argvs), rt](RenderGraph& graph, ResourceHandle::Ptr preRT, ResourceHandle::Ptr ds) mutable{
				if (srvs.find("frame") == srvs.end())
					srvs["frame"] = preRT;

				graph.addPass(name,[quad, srvs = std::move(srvs), argvs = std::move(argvs), rt](RenderGraph::Builder& builder){
					for (auto& s : srvs)
						builder.read(s.second);
					builder.write(rt, RenderGraph::Builder::IT_DISCARD);
					return[quad, srvs = std::move(srvs), argvs = std::move(argvs), rt](Renderer::CommandList::Ref cmdlist){
						cmdlist->setRenderTargets({rt->getView()->getRenderTarget()});

						for (auto& i : srvs)
							quad->setResource(i.first, i.second->getView()->getShaderResource());

						if (argvs)
							argvs(quad.get());

						quad->fitToScreen();
						RenderContext::getSingleton()->renderScreen(quad.get(), cmdlist);
					};
				});
				return rt;
			}
		);
	};
	mPasses[name] = std::move(pass);
}

ResourceHandle::Ptr Pipeline::postprocess(const std::string& name, std::map<std::string, ResourceHandle::Ptr>&& srvs, std::function<void(Quad*)>&& argvs)
{
	auto [rt, pp] = mPasses[name](std::move(srvs), std::move(argvs));
	mPostProcess.emplace_back(std::move(pp));
	return rt;
}

void Pipeline::postprocess(PostProcess&& pp)
{
	mPostProcess.emplace_back(std::move(pp));
}

void Pipeline::addRenderScene(RenderScene&& rs)
{
	mRenderScene = std::move(rs);
}

bool Pipeline::is(const std::string& n)
{
	return mSettings.switchers[n];
}

void Pipeline::set(const std::string& n, bool v)
{
	mSettings.switchers[n] = v;
}


void ForwardPipleline::execute()
{
	mDispatcher.invoke([this]() {
		mGui->update();
	});

	RenderGraph graph;

	auto r = Renderer::getSingleton();
	auto s = r->getSize();

	auto format = Renderer::FRAME_BUFFER_FORMAT;
	if (!is("tone"))
		format = Renderer::BACK_BUFFER_FORMAT;

	auto rt = ResourceHandle::create(Renderer::VT_RENDERTARGET, s[0], s[1], format);
	auto ds = ResourceHandle::create(Renderer::VT_DEPTHSTENCIL, s[0], s[1], DXGI_FORMAT_D24_UNORM_S8_UINT);

	rt->setClearValue({ 0,0,0,0 });
	ds->setClearValue({ 1.0f, 0 });
	graph.addPass("scene", [this, rt , ds](auto& builder) {
		builder.write(rt, RenderGraph::Builder::IT_CLEAR);
		builder.write(ds, RenderGraph::Builder::IT_CLEAR);
		return [this, rt, ds](auto cmdlist) {
			if (!mRenderScene)
				return;
			auto cam = RenderContext::getSingleton()->getMainCamera();
			cmdlist->setRenderTarget(rt->getView(), ds->getView());
			mRenderScene(cmdlist, cam, 0, 0);
		};
	});

	
	for (auto& pp: mPostProcess)
		rt = pp(graph, rt, ds);
	mPostProcess.clear();

	graph.addPass("gui", [this, dst = rt](auto& builder) {
		builder.write(dst, RenderGraph::Builder::IT_NONE);

		auto task = mGui->execute();
		return[this, dst, task = std::move(task)](auto cmdlist){
			cmdlist->setRenderTarget(dst->getView());
			task(cmdlist);
		};
	});

	graph.addPass("present", [this, src = rt](auto& builder) {
		builder.copy(src, {});
		return [this, src](auto cmdlist) mutable {
			auto renderer = Renderer::getSingleton();
			auto bb = renderer->getBackBuffer();
			cmdlist->transitionBarrier(bb, D3D12_RESOURCE_STATE_COPY_DEST, 0, true);
			cmdlist->copyTexture(bb, 0, { 0,0,0 }, src->getView(), 0, NULL);
			cmdlist->transitionBarrier(bb, D3D12_RESOURCE_STATE_PRESENT, 0, true);
		};
	});


	graph.execute();
}




//DefaultPipeline::DefaultPipeline()
//{
//
//	mProfileWindow = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("profile");
//	mDebugInfo = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("debuginfo");
//	mDebugInfo->drawCallback = [](ImGuiOverlay::ImGuiObject* gui) {
//		const auto& debuginfo = Renderer::getSingleton()->getDebugInfo();
//		ImGui::Text("adapter: %s", debuginfo.adapter.c_str());
//		ImGui::Text("drawcall count: %d",debuginfo.drawcallCount);
//		ImGui::Text("primitive count: %d", debuginfo.primitiveCount);
//		ImGui::Text("resources count: %d", debuginfo.numResources);
//
//		ImGui::Text("video memroy: %d MB", debuginfo.videoMemory / 1024 / 1024);
//
//		return true;
//	};
//
//	mRenderSettingsWnd = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("rendersettings");
//	mRenderSettingsWnd->drawCallback = [&settings = mSettings](auto gui) {
//		for (auto&s : settings.switchers)
//		ImGui::Checkbox(s.first.c_str(), &s.second);
//
//		return true;
//	};
//
//	mVisualizationWnd = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("visualization");
//	mVisualizationWnd->drawCallback = [&visual = mSettings](auto gui) {
//		static int cur = 0;
//		static int selected = 0;
//		ImGui::RadioButton("final", &cur, 0);
//		ImGui::RadioButton("vertex color", &cur, 1);
//		ImGui::RadioButton("base color", &cur, 2);
//		ImGui::RadioButton("roughness", &cur, 3);
//		ImGui::RadioButton("metallic", &cur, 4);
//
//		ImGui::RadioButton("normal", &cur, 5);
//		ImGui::RadioButton("emissive color", &cur, 6);
//
//		if (cur != selected)
//		{
//			RenderContext::getSingleton()->recompileMaterials((Material::Visualizaion)cur);
//			selected = cur;
//		}
//
//		return true;
//	};
//
//	auto mainbar = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiMenuBar>(true);
//	auto addbutton = [&](const std::string& name, ImGuiOverlay::ImGuiObject* win){
//		mainbar->createChild<ImGuiOverlay::ImGuiButton>(name)->callback = [=](auto button) {
//			win->visible = !win->visible;
//		};
//	};
//
//	addbutton("profile", mProfileWindow);
//	addbutton("debuginfo", mDebugInfo);
//	addbutton("settings", mRenderSettingsWnd);
//	addbutton("visualizations", mVisualizationWnd);
//
//
//	mPasses["present"] = postprocess("shaders/drawtexture.hlsl", DXGI_FORMAT_R8G8B8A8_UNORM);
//	mPasses["tone"] = postprocess("shaders/tonemapping.hlsl", DXGI_FORMAT_R8G8B8A8_UNORM);
//
//
//
//	mSettings.switchers["tone"] = true;
//
//}

//void DefaultPipeline::update()
//{
//	PROFILE("pipeline", {});
//	RenderGraph graph;
//
//	mProfileWindow->drawCallback = [profiles = std::move(ProfileMgr::Singleton.output())](auto gui) {
//		for (auto& profile: profiles)
//			ImGui::Text("%s: cpu: %.3f ms, gpu: %.3f ms", profile.name.c_str(), profile.cpu, profile.gpu );
//		return true;
//	};
//
//	auto r = Renderer::getSingleton();
//	auto s = r->getSize();
//
//	auto format = Renderer::FRAME_BUFFER_FORMAT;
//	if (!is("tone"))
//		format = Renderer::BACK_BUFFER_FORMAT;
//	auto hdr = ResourceHandle::create(Renderer::VT_RENDERTARGET, s[0], s[1], format);
//	auto d = ResourceHandle::create(Renderer::VT_DEPTHSTENCIL, s[0], s[1], DXGI_FORMAT_D24_UNORM_S8_UINT);
//
//	hdr->setClearValue({0,0,0,0});
//	d->setClearValue({1.0f, 0});
//	graph.addPass("scene", [this, rt = hdr, d](auto& builder) {
//		builder.write(rt, RenderGraph::Builder::IT_CLEAR);
//		builder.write(d, RenderGraph::Builder::IT_CLEAR);
//		return [=](auto cmdlist) {
//			auto context = RenderContext::getSingleton();
//			auto cam = context->getMainCamera();
//			cmdlist->setRenderTarget(rt->getView(), d->getView());
//			context->renderScene(cmdlist,cam);
//		};
//	});
//
//	
//	auto rt = hdr;
//
//	auto connectPostprocess = [&](const std::string& name, std::map<std::string, ResourceHandle::Ptr>&& srvs, std::function<void(Quad*)>&& argvs, auto fmt)
//	{
//		if (mSettings.switchers[name])
//		{
//			rt = addPostprocessPass(graph, name, [this, name](auto& ... args) {
//				mPasses[name](args ...);
//				}, std::move(srvs), std::move(argvs), fmt);
//		}
//	};
//
//
//	////if (mSettings.switchers["atmosphere"])
//	//{
//	//	auto out = ResourceHandle::create(Renderer::VT_RENDERTARGET, s[0], s[1], DXGI_FORMAT_R8G8B8A8_UNORM);
//	//	mAtmosphere.execute(graph, hdr);
//	//}
//
//	connectPostprocess("tone", { {"frame", hdr} }, {}, DXGI_FORMAT_R8G8B8A8_UNORM);
//
//
//	//auto barrier = graph.addBarrier("gui barrier");
//	//mDispatcher.invoke([barrier, this, rt](){
//	//	PROFILE("gui logic", {});
//	//	mGui.update();
//	//	barrier->addRenderTask("gui", [rt, this](auto cmdlist) {
//	//		RenderGraph::Builder builder;
//	//		builder.write(rt, RenderGraph::Builder::IT_NONE, D3D12_RESOURCE_STATE_RENDER_TARGET);
//	//		auto task = mGui.execute();
//	//		builder.prepare(cmdlist);
//	//		cmdlist->setRenderTarget(rt->getView());
//	//		task(cmdlist);
//	//	});
//	//	barrier->signal();
//
//	//});
//
//
//	mDispatcher.invoke([this](){
//		mGui.update();
//	});
//
//	graph.addPass("gui",[this, dst = rt](auto& builder) {
//		builder.write(dst, RenderGraph::Builder::IT_NONE);
//
//		auto task = mGui.execute();
//		return [this, dst, task = std::move(task)](auto cmdlist){
//			cmdlist->setRenderTarget(dst->getView());
//			task(cmdlist);
//		};
//	});
//
//	graph.addPass("present", [this, src = rt](auto& builder) {
//		builder.copy(src, {});
//		return [this, src](auto cmdlist) mutable{
//			auto renderer = Renderer::getSingleton();
//			auto bb = renderer->getBackBuffer();
//			cmdlist->transitionBarrier(bb, D3D12_RESOURCE_STATE_COPY_DEST,0, true);
//			cmdlist->copyTexture( bb,0,{0,0,0}, src->getView(), 0, NULL);
//			cmdlist->transitionBarrier(bb, D3D12_RESOURCE_STATE_PRESENT, 0, true);
//		};
//	});
//
//	graph.execute();
//}
