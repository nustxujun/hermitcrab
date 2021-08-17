#include "PipelineOperation.h"

RenderGraph::RenderPass PipelineOperation::renderScene(Pipeline::CameraInfo caminfo, Pipeline::RenderScene&& render_scene, ResourceHandle::Ptr rendertarget, ResourceHandle::Ptr depthstencil)
{
	return[renderscene = std::move(render_scene), rt = rendertarget, ds = depthstencil, caminfo = std::move(caminfo)](auto& builder)
	{
		builder.write(rt, RenderGraph::Builder::IT_CLEAR);
		builder.write(ds, RenderGraph::Builder::IT_CLEAR);
		return [rs = std::move(renderscene), r = rt, d = ds, c = caminfo](auto cmdlist)->Future<Promise>
		{
			auto renderscene = std::move(rs);
			auto rt = r;
			auto ds = d;
			auto caminfo = c;

			co_await std::suspend_always();

			cmdlist->setRenderTarget(rt->getView(), ds->getView());
			renderscene(cmdlist, caminfo, 0, 0);
			co_return;
		};
	};
}

RenderGraph::RenderPass PipelineOperation::renderUI( ResourceHandle::Ptr rendertarget)
{
	return  [ dst = rendertarget](auto& builder)mutable {
		builder.write(dst, RenderGraph::Builder::IT_NONE);

		auto task = ImGuiPass::getInstance()->execute();
		return[d = dst, task = std::move(task)](auto cmdlist)->Future<Promise>
		{
			auto dst = d;
			Coroutine<Promise> co(std::move(task), cmdlist);
			co_await std::suspend_always();
			cmdlist->setRenderTarget(dst->getView());
			while (!co.done())
			{
				co_await std::suspend_always();
				co.resume();
			}
			co_return;
		};
	};

}