#pragma once

#include "Common.h"

#include "imgui/imgui.h"
#include "RenderGraph.h"

class ImGuiObject;
class ImGuiPass 
{
public:
	using Ptr = std::shared_ptr<ImGuiPass>;
public:
	static ImGuiPass::Ptr getInstance()
	{
		static Ptr inst = Ptr(new ImGuiPass());
		return inst;
	}

	ImGuiPass();
	~ImGuiPass();

	RenderGraph::RenderTask execute() ;
	static void resize( int width, int height);
	void ready(){mReady.store(true);}
private:
	void initImGui();
	void initRendering();
	void initFonts();

	static void beginFrame();
	static void endFrame();

private:
	Renderer::PipelineStateInstance::Ptr mPipelineState;
	Renderer::Buffer::Ref mVertexBuffer[Renderer::NUM_BACK_BUFFERS];
	Renderer::Buffer::Ref mIndexBuffer[Renderer::NUM_BACK_BUFFERS];
	Renderer::Resource::Ref mFonts;
	int mWidth = 0;
	int mHeight = 0;

	FenceObject mFence;
	std::atomic<bool> mReady = false;
};

