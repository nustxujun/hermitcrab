#pragma once

#include "RenderGraph.h"
#include "RenderContext.h"
#include "ImGuiOverlay.h"
class Pipeline
{
public:
	RenderGraph::LambdaRenderPass::Ptr present();
	RenderGraph::LambdaRenderPass::Ptr drawScene(Camera::Ptr cam,UINT flags = 0, UINT mask = -1);

};

class DefaultPipeline: public Pipeline
{
public:
	DefaultPipeline();

	void update();

private:
	ImGuiPass mGui;
	RenderGraph::LambdaRenderPass::Ptr mPresent;
	RenderGraph::LambdaRenderPass::Ptr mDrawScene;
	
	std::map<RenderGraph::RenderPass*, std::pair<Renderer::Profile::Ref, ImGuiText*>> mProfiles;
	ImGuiObject* mProfileWindow;
};