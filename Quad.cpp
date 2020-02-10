#include "Quad.h"

void Quad::init(const std::string & psname, const Renderer::RenderState& settingrs)
{
	auto renderer = Renderer::getSingleton();
	auto vs = renderer->compileShaderFromFile("shaders/quad.hlsl", "vs", SM_VS);
	auto ps = renderer->compileShaderFromFile((psname), "ps", SM_PS);
	std::vector<Renderer::Shader::Ptr> shaders = { vs, ps };
	ps->enable32BitsConstants(true);
	ps->registerStaticSampler({
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			0,0,
			D3D12_COMPARISON_FUNC_NEVER,
			D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
			0,
			D3D12_FLOAT32_MAX,
			0,0,
			D3D12_SHADER_VISIBILITY_PIXEL
		});
	ps->registerStaticSampler({
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			0,0,
			D3D12_COMPARISON_FUNC_NEVER,
			D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
			0,
			D3D12_FLOAT32_MAX,
			1,0,
			D3D12_SHADER_VISIBILITY_PIXEL
		});
	Renderer::RenderState rs = settingrs;

	mPipelineState = renderer->createPipelineState(shaders,rs);


}

Renderer::PipelineState::Ref Quad::getPipelineState() const
{
	return mPipelineState;
}

void Quad::setResource(const std::string& name, D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
	mPipelineState->setResource(Renderer::Shader::ST_PIXEL, name, handle);
}	

void Quad::fitToScreen()
{
	auto renderer = Renderer::getSingleton();
	auto size = renderer->getSize();
	mRect = {0,0,(LONG)size[0], (LONG)size[1]};
}

