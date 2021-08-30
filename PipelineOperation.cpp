#include "PipelineOperation.h"

RenderGraph::RenderPass PipelineOperation::renderScene(Pipeline::CameraInfo caminfo, Pipeline::RenderScene&& render_scene, ResourceHandle::Ptr rendertarget, ResourceHandle::Ptr depthstencil)
{
	return[renderscene = std::move(render_scene), rt = rendertarget, ds = depthstencil, caminfo = std::move(caminfo)](auto& builder)
	{
		builder.write(rt, RenderGraph::Builder::IT_CLEAR);
		builder.write(ds, RenderGraph::Builder::IT_CLEAR);
		return [rs = renderscene, r = rt, d = ds, c = caminfo](auto cmdlist)->Future<Promise>
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

RenderGraph::RenderPass PipelineOperation::present(ResourceHandle::Ptr rendertarget)
{
	return [src = rendertarget](auto& builder) {
		builder.copy(src, {});

		auto bb = Renderer::getSingleton()->getBackBuffer();
		auto& dstdesc = bb->getDesc();
		auto& srcdesc = src->getView()->getDesc();
		Common::Assert(dstdesc.Width == srcdesc.Width && dstdesc.Height == srcdesc.Height, "rendertarget size is invalid");

		return [s = src](auto cmdlist) ->Future<Promise>
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
	};
}