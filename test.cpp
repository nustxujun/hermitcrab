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
			void init()
			{
				auto renderer = Renderer::getSingleton();

				auto vs = renderer->compileShader("../Engine/shaders/shaders.hlsl", "VSMain", "vs_5_0");
				auto ps = renderer->compileShader("../Engine/shaders/shaders.hlsl", "PSMain", "ps_5_0");
				std::vector<Renderer::Shader::Ptr> shaders = { vs, ps };
				ps->registerSRV(1,0,0);

				Renderer::RenderState rs = Renderer::RenderState::Default;
				rs.setInputLayout({
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
					});

				pso = renderer->createPipelineState(shaders, rs);

				std::pair<Vector3, Vector4> triangleVertices[] =
				{
					{ { 0.0f, 0.25f , 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
					{ { 0.25f, -0.25f , 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
					{ { -0.25f, -0.25f , 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
				};

				vertices = renderer->createVertexBuffer(sizeof(triangleVertices), sizeof(std::pair<Vector3, Vector4>), D3D12_HEAP_TYPE_DEFAULT, triangleVertices, sizeof(triangleVertices));

				renderer->createTexture("test.png");

			}

			void updateImpl()
			{
				auto renderer = Renderer::getSingleton();
				auto bb = renderer->getBackBuffer();
				auto cmdlist = renderer->getCommandList();

				cmdlist->transitionTo(bb->getTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
				cmdlist->clearRenderTarget(bb, { 0.5,0.5,0.5,1 });
				cmdlist->setRenderTarget(bb);

				auto desc = bb->getTexture()->getDesc();
				cmdlist->setViewport({
					0.0f, 0.0f,(float) desc.Width,(float)desc.Height, 0.0f, 1.0f
					});

				cmdlist->setScissorRect({ 0,0, (LONG)desc.Width, (LONG)desc.Height });

				cmdlist->setPipelineState(pso);
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

