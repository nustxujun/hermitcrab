#pragma once

#include "RenderGraph.h"
#include "RenderContext.h"
#include "ImGuiOverlay.h"
class Pipeline
{
public:
	Renderer::RenderTask present(RenderGraph::Builder& b, ResourceHandle::Ptr src);
	//RenderGraph::LambdaRenderPass::Ptr drawScene(Camera::Ptr cam,UINT flags = 0, UINT mask = -1);
	//RenderGraph::LambdaRenderPass::Ptr postprocess(const std::string& ps, const std::function<void(Renderer::PipelineState::Ref)>& prepare = {});
};

class DefaultPipeline: public Pipeline
{
public:
	DefaultPipeline();

	virtual void update();

protected:
	ImGuiPass mGui;

	
	std::map<std::string, std::pair<Renderer::Profile::Ref, ImGuiOverlay::ImGuiText*>> mProfiles;
	ImGuiOverlay::ImGuiObject* mProfileWindow;
	ImGuiOverlay::ImGuiObject* mDebugInfo;
	ImGuiOverlay::ImGuiObject* mRenderSettingsWnd;
	ImGuiOverlay::ImGuiObject* mVisualizationWnd;


	struct RenderSettings
	{
		bool colorGrading = true;
	}
	mSettings;

	Quad::Ptr mPresentQuad;

};