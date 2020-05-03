#pragma once

#include "RenderGraph.h"
#include "RenderContext.h"
#include "ImGuiOverlay.h"
class Pipeline
{
public:
	using RenderPass = std::function<void (Renderer::CommandList::Ref&, const std::map<std::string,D3D12_GPU_DESCRIPTOR_HANDLE>&, const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>&, D3D12_CPU_DESCRIPTOR_HANDLE)>;

	RenderPass postprocess(const std::string& ps, DXGI_FORMAT fmt );
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
		bool tonemapping = true;
	}
	mSettings;

	std::map<std::string, RenderPass> mPasses;
};