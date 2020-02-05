#include "Quad.h"

void Quad::init(const std::string & psname, const Renderer::RenderState& settingrs)
{
	{
		static std::weak_ptr<Renderer::Buffer> vertices;
		if (vertices.expired())
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
			mVertices = Renderer::getSingleton()->createBuffer(sizeof(triangleVertices), sizeof(std::pair<Vector2, Vector2>), false, D3D12_HEAP_TYPE_DEFAULT, triangleVertices, sizeof(triangleVertices));
			vertices = mVertices;
		}
		else
			mVertices = vertices.lock();
	}

	auto renderer = Renderer::getSingleton();
	auto vs = renderer->compileShaderFromFile("shaders/quad_vs.hlsl", "vs", SM_VS);
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
	rs.setInputLayout({
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		});

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

Renderer::Buffer::Ptr Quad::getSharedVertices() const
{
	return mVertices;
}

void Quad::fitToScreen()
{
	auto renderer = Renderer::getSingleton();
	auto size = renderer->getSize();
	mRect = {0,0,(LONG)size[0], (LONG)size[1]};
}

