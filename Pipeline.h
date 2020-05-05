#pragma once

#include "RenderGraph.h"
#include "RenderContext.h"
#include "ImGuiOverlay.h"

#include "AtmosphericScattering.h"
class Pipeline
{
public:
	using RenderPass = std::function<void (Renderer::CommandList::Ref&, const std::map<std::string,D3D12_GPU_DESCRIPTOR_HANDLE>&, const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>&,  const std::function<void(Quad*)>&) >;

	RenderPass postprocess(const std::string& ps, DXGI_FORMAT fmt );
	ResourceHandle::Ptr addPostprocessPass(RenderGraph& rg, const std::string& name, RenderPass&& f, std::map<std::string, ResourceHandle::Ptr>&& srvs, std::function<void(Quad*)>&& argvs, DXGI_FORMAT targetfmt);
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
		std::map<std::string, bool> switchers;
	}
	mSettings;

	std::map<std::string, RenderPass> mPasses;

	AtmosphericScattering mAtmosphere;
};