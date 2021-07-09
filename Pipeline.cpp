#include "Pipeline.h"
#include "Renderer.h"
#include <sstream>
#include "Profile.h"
#include "ResourceViewAllocator.h"


Pipeline::Pipeline()
{
	mGui = ImGuiPass::Ptr(new ImGuiPass);
}

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

				graph.addPass(name,[quad, srvs = std::move(srvs), argvs = std::move(argvs), rt](RenderGraph::Builder& builder)
				{
					for (auto& s : srvs)
						builder.read(s.second);
					builder.write(rt, RenderGraph::Builder::IT_DISCARD);
					return[quad, s = std::move(srvs), a = std::move(argvs), r = rt](Renderer::CommandList * cmdlist)->Future<Promise>
					{
						auto q = quad;
						auto srvs = std::move(s);
						auto argvs = std::move(a);
						auto rt = r;
						co_await std::suspend_always();

						cmdlist->setRenderTargets({rt->getView()->getRenderTarget()});

						for (auto& i : srvs)
							q->setResource(i.first, i.second->getView()->getShaderResource());

						if (argvs)
							argvs(q.get());

						q->fitToScreen();
						q->draw(cmdlist);
						co_return;
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
	mRenderScene = decltype(mRenderScene)(new RenderScene{std::move(rs)});
}

bool Pipeline::is(const std::string& n)
{
	return mSettings.switchers[n];
}

void Pipeline::set(const std::string& n, bool v)
{
	mSettings.switchers[n] = v;
}


void ForwardPipleline::execute(CameraInfo caminfo)
{
	//mDispatcher.invoke([gui = mGui, &cb = mGUICallback]() {
	//	gui->update(cb);
	//});

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

	if (mRenderScene)
		graph.addPass("scene", [renderscene = mRenderScene, rt , ds, caminfo](auto& builder)
		{
			builder.write(rt, RenderGraph::Builder::IT_CLEAR);
			builder.write(ds, RenderGraph::Builder::IT_CLEAR);
			return [rs = renderscene, r = rt, d = ds, c = caminfo](auto cmdlist)->Future<Promise>
			{
				auto renderscene = rs;
				auto rt = r;
				auto ds = d;
				auto caminfo = c;

				co_await std::suspend_always();

				cmdlist->setRenderTarget(rt->getView(), ds->getView());
				(*renderscene)(cmdlist, caminfo, 0, 0);
				co_return;
			};
		});

	
	for (auto& pp: mPostProcess)
		rt = pp(graph, rt, ds);
	mPostProcess.clear();

	graph.addPass("gui", [gui = mGui, dst = rt, &cb = mGUICallback](auto& builder) {
		builder.write(dst, RenderGraph::Builder::IT_NONE);

		auto task = ImGuiPass::execute(gui);
		return[ d = dst, task = std::move(task)](auto cmdlist)->Future<Promise>
		{
			auto dst = d;
			Coroutine<Promise> co(std::move(task), cmdlist);
			co_await std::suspend_always();
			cmdlist->setRenderTarget(dst->getView());
			while (!co.done())
				co.resume();
			co_return;
		};
	});

	graph.addPass("present", [ src = rt](auto& builder) {
		builder.copy(src, {});
		return [ s = src](auto cmdlist) ->Future<Promise> 
		{
			auto src = s;
			co_await std::suspend_always();
			auto renderer = Renderer::getSingleton();
			auto bb = renderer->getBackBuffer();
			cmdlist->transitionBarrier(bb, D3D12_RESOURCE_STATE_COPY_DEST, 0, true);
			cmdlist->copyTexture(bb, 0, { 0,0,0 }, src->getView(), 0, NULL);
			cmdlist->transitionBarrier(bb, D3D12_RESOURCE_STATE_PRESENT, 0, true);
			co_return;
		};
	});


	graph.execute(Renderer::getSingleton()->getRenderQueue());

	mGui->update(mGUICallback);

}


