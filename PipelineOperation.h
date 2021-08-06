#pragma once 

#include "Pipeline.h"

class PipelineOperation
{
public:
	Pipeline::RenderPass renderScene(
		Pipeline::RenderScene&& render_scene, 
		ResourceHandle::Ptr rendertarget, 
		ResourceHandle::Ptr depthstencil);

	Pipeline::RenderPass renderUI(
		ResourceHandle::Ptr rendertarget, )
};