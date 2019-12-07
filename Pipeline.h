#pragma once

#include "RenderGraph.h"
#include "RenderContext.h"
#include "ImguiOverlay.h"
class Pipeline
{
public:
	virtual void update() = 0;

	RenderGraph::LambdaRenderPass clearDepth(const Color& color);
	RenderGraph::LambdaRenderPass present();
	RenderGraph::LambdaRenderPass drawScene(Camera::Ptr cam,UINT flags = 0, UINT mask = -1);
	ImguiPass& gui(){return mImgui;}
private:
	ImguiPass mImgui;
};

class ForwardPipeline final : public Pipeline
{
public:
	void update();
};