// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "Framework.h"

int main()
{
	{

		class Frame : public Framework
		{
		public:
			Renderer::PipelineState::Ref pso;
			Renderer::VertexBuffer::Ptr vertices;
			Renderer::Texture::Ref tex;
			void init()
			{
				auto renderer = Renderer::getSingleton();

				auto vs = renderer->compileShader("Engine/shaders/shaders.hlsl", "VSMain", "vs_5_0");
				auto ps = renderer->compileShader("Engine/shaders/shaders.hlsl", "PSMain", "ps_5_0");
				std::vector<Renderer::Shader::Ptr> shaders = { vs, ps };
				ps->registerSRV(1,0,0);
				ps->registerStaticSampler({
					D3D12_FILTER_MIN_MAG_MIP_POINT,
					D3D12_TEXTURE_ADDRESS_MODE_BORDER,
					D3D12_TEXTURE_ADDRESS_MODE_BORDER,
					D3D12_TEXTURE_ADDRESS_MODE_BORDER,
					0,0,
					D3D12_COMPARISON_FUNC_NEVER,
					D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
					0,
					D3D12_FLOAT32_MAX,
					0,0,
					D3D12_SHADER_VISIBILITY_PIXEL
					});

				Renderer::RenderState rs = Renderer::RenderState::Default;
				rs.setInputLayout({
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
					});

				pso = renderer->createPipelineState(shaders, rs);

				std::pair<Vector3, Vector4> triangleVertices[] =
				{
					{ { 0.0f, 0.25f , 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
					{ { 0.25f, -0.25f , 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
					{ { -0.25f, -0.25f , 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
				};

				vertices = renderer->createVertexBuffer(sizeof(triangleVertices), sizeof(std::pair<Vector3, Vector4>), D3D12_HEAP_TYPE_DEFAULT, triangleVertices, sizeof(triangleVertices));

				tex = renderer->createTexture("test.jpg");

			}

			void updateImpl()
			{
				auto renderer = Renderer::getSingleton();
				auto bb = renderer->getBackBuffer();
				auto cmdlist = renderer->getCommandList();
				cmdlist->setPipelineState(pso);

				//cmdlist->setTexture(0, tex);

				cmdlist->transitionTo(bb->getTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
				cmdlist->clearRenderTarget(bb, { 0.5,0.5,0.5,1 });
				cmdlist->setRenderTarget(bb);

				auto desc = bb->getTexture()->getDesc();
				cmdlist->setViewport({
					0.0f, 0.0f,(float) desc.Width,(float)desc.Height, 0.0f, 1.0f
					});

				cmdlist->setScissorRect({ 0,0, (LONG)desc.Width, (LONG)desc.Height });

				cmdlist->setPrimitiveType();
				cmdlist->setVertexBuffer(vertices);
				cmdlist->drawInstanced(3);

				cmdlist->transitionTo(bb->getTexture(), D3D12_RESOURCE_STATE_PRESENT);
			}
		} frame;


		frame.init();
		frame.update();
	}

	_CrtDumpMemoryLeaks();
	return 0;
}

