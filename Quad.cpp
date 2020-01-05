#include "Quad.h"

void Quad::init(const std::string & psname, const Renderer::RenderState& settingrs)
{
	auto renderer = Renderer::getSingleton();
	auto vs = renderer->compileShader(L"shaders/quad_vs.hlsl", L"vs", SM_VS);
	auto ps = renderer->compileShader(M2U(psname), L"ps", SM_PS);
	std::vector<Renderer::Shader::Ptr> shaders = { vs, ps };

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

	Renderer::RenderState rs = settingrs;
	rs.setInputLayout({
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		});

	mPipelineState = renderer->createPipelineState(shaders,rs);

	mConstant = mPipelineState->createConstantBuffer(Renderer::Shader::ST_PIXEL,"PSConstant");

}

Renderer::PipelineState::Ref Quad::getPipelineState() const
{
	return mPipelineState;
}

void Quad::setResource(const std::string & name, const D3D12_GPU_DESCRIPTOR_HANDLE & handle)
{
	mPipelineState->setResource(Renderer::Shader::ST_PIXEL, name,handle);
}

void Quad::setResource(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE & handle)
{
	mPipelineState->setPSResource(slot, handle);
}

Renderer::Buffer::Ptr Quad::getSharedVertices() const
{
	static Renderer::Buffer::Ptr vertices;
	if (!vertices)
	{
		std::pair<Vector2, Vector2> triangleVertices[] =
		{
			{ { 1.0f, 1.0f  }, { 1.0f, 0.0f }},
			{ { 1.0, -1.0f  }, { 1.0f, 1.0f }},
			{ { -1.0f, -1.0f  }, { 0.0f, 1.0f}},

			{ { 1.0f, 1.0f }, { 1.0f, 0.0f }},
			{ { -1.0f, -1.0f  }, { 0.0f, 1.0f}},
			{ { -1.0f, 1.0f  }, { 0.0f, 0.0f}}
		};
		vertices = Renderer::getSingleton()->createBuffer(sizeof(triangleVertices), sizeof(std::pair<Vector2, Vector2>), D3D12_HEAP_TYPE_DEFAULT, triangleVertices, sizeof(triangleVertices));
	}
	return vertices;
}

