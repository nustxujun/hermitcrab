#pragma once

#include "RenderGraph.h"
#include "Quad.h"

class AtmosphericScattering
{
public:
	AtmosphericScattering();

	void execute(RenderGraph& graph);

private:
	Quad::Ptr mQuad;
	Renderer::ConstantBuffer::Ptr mAtmosphereParams;
	Renderer::Resource::Ref mTransmittanceToAtmosphereTop;
	bool mRecompute = true;
};