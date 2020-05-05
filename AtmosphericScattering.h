#pragma once

#include "RenderGraph.h"
#include "Quad.h"

class AtmosphericScattering
{
public:
	AtmosphericScattering();

	RenderGraph::RenderPass execute(ResourceHandle::Ptr input, ResourceHandle::Ptr output);

private:
	Quad::Ptr mQuad;
	Renderer::ConstantBuffer::Ptr mAtmosphereParams;
	Renderer::Resource::Ref mTransmittanceToAtmosphereTop;
	bool mRecompute = true;
};