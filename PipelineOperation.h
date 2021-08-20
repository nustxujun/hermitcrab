#pragma once 

#include "Pipeline.h"

class PipelineOperation
{
public:
	static RenderGraph::RenderPass renderScene(
		Pipeline::CameraInfo caminfo,
		Pipeline::RenderScene&& render_scene, 
		ResourceHandle::Ptr rendertarget, 
		ResourceHandle::Ptr depthstencil);

	static RenderGraph::RenderPass renderUI( 
		ResourceHandle::Ptr rendertarget);

	static RenderGraph::RenderPass present(
		ResourceHandle::Ptr rendertarget);

};