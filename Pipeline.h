#pragma once

#include "RenderGraph.h"
#include "RenderContext.h"
#include "ImGuiOverlay.h"

class Pipeline
{
public:
	Pipeline();
	using Ptr = std::shared_ptr<Pipeline>;

	using PostProcess = std::function<ResourceHandle::Ptr(RenderGraph&, ResourceHandle::Ptr, ResourceHandle::Ptr)>;
	using RenderPass = std::function<std::pair<ResourceHandle::Ptr, PostProcess> (std::map<std::string,ResourceHandle::Ptr>&&,  std::function<void(Quad*)>&&) >;
	using SetupQuad = std::function<void(Quad&, std::map<std::string, ResourceHandle::Ptr>&&, ResourceHandle::Ptr, ResourceHandle::Ptr)>;
	//RenderPass postprocess(const std::string& ps, DXGI_FORMAT fmt );
	void addPostProcessPass( const std::string& name,  DXGI_FORMAT fmt);
	ResourceHandle::Ptr postprocess(const std::string& name, std::map<std::string, ResourceHandle::Ptr>&& srvs = {}, std::function<void(Quad*)>&& argvs = {});
	void postprocess( PostProcess&& pp);

	using RenderScene = std::function<void(Renderer::CommandList::Ref cmdlist, Camera::Ptr cam, UINT flags , UINT mask )>;
	void addRenderScene(RenderScene&& rs);

	virtual void execute() {};

	bool is(const std::string& n);
	void set(const std::string& n, bool v);
protected:

	struct RenderSettings
	{
		std::map<std::string, bool> switchers;
	}
	mSettings;
	Dispatcher mDispatcher;
	ImGuiPass::Ptr mGui;
	std::shared_ptr<RenderScene> mRenderScene;
	std::map<std::string, RenderPass> mPasses;
	std::vector<PostProcess> mPostProcess;

};

class ForwardPipleline : public Pipeline 
{
public:
	using Pipeline::Pipeline;

	void execute() override;
private:
};

//class DefaultPipeline: public Pipeline
//{
//public:
//	DefaultPipeline();
//
//	virtual void update();
//
//protected:
//	ImGuiPass mGui;
//	
//	std::map<std::string, std::pair<Renderer::Profile::Ref, ImGuiOverlay::ImGuiText*>> mProfiles;
//	ImGuiOverlay::ImGuiObject* mProfileWindow;
//	ImGuiOverlay::ImGuiObject* mDebugInfo;
//	ImGuiOverlay::ImGuiObject* mRenderSettingsWnd;
//	ImGuiOverlay::ImGuiObject* mVisualizationWnd;
//
//
//	struct RenderSettings
//	{
//		std::map<std::string, bool> switchers;
//	}
//	mSettings;
//
//	std::map<std::string, RenderPass> mPasses;
//
//	AtmosphericScattering mAtmosphere;
//	Dispatcher mDispatcher;
//
//};