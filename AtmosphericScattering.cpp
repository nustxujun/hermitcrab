#include "AtmosphericScattering.h"

AtmosphericScattering::AtmosphericScattering()
{
	auto renderer = Renderer::getSingleton();
	auto ps = renderer->compileShaderFromFile("shaders/atmospheric_scattering.hlsl","precomputeTransmittanceToAtmosphereTop",SM_PS);
	mQuad = Quad::Ptr(new Quad);
	mQuad->init(ps, DXGI_FORMAT_R8G8B8A8_UNORM);
	mAtmosphereConsts = mQuad->getPipelineState()->createConstantBuffer(Renderer::Shader::ST_PIXEL, "AtomsphereConstants");

	auto size = renderer->getSize();
	mTransmittanceToAtmosphereTop = renderer->createResourceView(256, 256, DXGI_FORMAT_R8G8B8A8_UNORM,Renderer::VT_RENDERTARGET);
	mTransmittanceToAtmosphereTop->createRenderTargetView(NULL);

	mSettings = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("atmosphere settings");
	mSettings->drawCallback = [&](auto) {
		
		return true;
	};
}

void AtmosphericScattering::execute(RenderGraph& graph)
{
	if (mRecompute)
	{	
		mRecompute  = false;
		graph.addPass("prec", [this](RenderGraph::Builder& b)mutable
		{
			return [this](Renderer::CommandList::Ref cmdlist) {
				cmdlist->setRenderTarget(mTransmittanceToAtmosphereTop);
				mQuad->setRect({0,0,256,256});
				mQuad->draw(cmdlist);
			};
		});
	}
	else
	{
	}
}
