#pragma once

#include "RenderGraph.h"

class Pipeline
{
public:
	virtual void update() = 0;

	RenderGraph::LambdaRenderPass clearDepth(const Color& color);
	RenderGraph::LambdaRenderPass present();
private:
	
};

class ForwardPipeline final : public Pipeline
{
public:
	void update();
};