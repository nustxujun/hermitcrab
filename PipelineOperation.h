#pragma once 

#include "Pipeline.h"

class PipelineOperation
{
public:
	RenderGraph::RenderPass renderScene(
		Pipeline::CameraInfo caminfo,
		Pipeline::RenderScene&& render_scene, 
		ResourceHandle::Ptr rendertarget, 
		ResourceHandle::Ptr depthstencil);

	RenderGraph::RenderPass renderUI(
		std::function<void(void)>&& callback, 
		ResourceHandle::Ptr rendertarget);

};