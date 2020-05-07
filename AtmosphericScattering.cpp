#include "AtmosphericScattering.h"

AtmosphericScattering::AtmosphericScattering()
{
	auto renderer = Renderer::getSingleton();
	auto ps = renderer->compileShaderFromFile("shaders/atmospheric_scattering.hlsl","precomputeTransmittanceToAtmosphereTop",SM_PS);
	mQuad = Quad::Ptr(new Quad);
	mQuad->init(ps, DXGI_FORMAT_R8G8B8A8_UNORM);
	mAtmosphereParams = mQuad->getPipelineState()->createConstantBuffer(Renderer::Shader::ST_PIXEL, "AtomsphereConstants");

	auto size = renderer->getSize();
	mTransmittanceToAtmosphereTop = renderer->createResourceView(size[0], size[1], DXGI_FORMAT_R8G8B8A8_UNORM,Renderer::VT_RENDERTARGET);
	mTransmittanceToAtmosphereTop->createRenderTargetView(NULL);
}

void AtmosphericScattering::execute(RenderGraph& graph)
{
	if (mRecompute)
	{	
		graph.addPass("prec", [this](RenderGraph::Builder& b) {
			return [this](Renderer::CommandList::Ref cmdlist) {
				cmdlist->setRenderTarget(mTransmittanceToAtmosphereTop);
				mQuad->fitToScreen();
				mQuad->draw(cmdlist);
			};
		});
	}
	else
	{
	}
}
