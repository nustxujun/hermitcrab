#include "AtmosphericScattering.h"

AtmosphericScattering::AtmosphericScattering()
{
	auto renderer = Renderer::getSingleton();
	auto ps = renderer->compileShaderFromFile("shaders/atmospheric_scattering.hlsl","precomputeTransmittanceToAtmosphereTop",SM_PS);
	mQuad = Quad::Ptr(new Quad);
	mQuad->init(ps);
	mAtmosphereParams = mQuad->getPipelineState()->createConstantBuffer(Renderer::Shader::ST_PIXEL, "AtomsphereConstants");

	auto size = renderer->getSize();
	//mTransmittanceToAtmosphereTop = renderer->createTexture(size[0], size[1],1, DXGI_FORMAT_R8G8B8A8_UINT,1);
	//mTransmittanceToAtmosphereTop->createRenderTargetView(NULL);
}

RenderGraph::RenderPass AtmosphericScattering::execute(ResourceHandle::Ptr input, ResourceHandle::Ptr output)
{
	if (mRecompute)
	{	
		mRecompute = true;
		return [this](RenderGraph::Builder& b) {
			return [this](Renderer::CommandList::Ref cmdlist) {
				cmdlist->setRenderTarget(mTransmittanceToAtmosphereTop);
				mQuad->fitToScreen();
				mQuad->draw(cmdlist);
			};
		};
	}
	else
	{
		return {};
	}
}
