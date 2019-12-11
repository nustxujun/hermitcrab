// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "Framework.h"
#include "Pipeline.h"
#include "RenderContext.h"
#include "ImguiOverlay.h"
#include <sstream>

#ifdef NO_UE4
int main()
{
	{

		class Frame : public Framework, public RenderContext
		{
		public:
			ForwardPipeline pipeline;
			Renderer::PipelineState::Ref pso;
			Renderer::Buffer::Ptr vertices;
			Renderer::Texture::Ref tex;
			ImguiText* fps;
			void init()
			{
				auto renderer = Renderer::getSingleton();

				auto vs = renderer->compileShader(L"shaders/shaders.hlsl", L"VSMain", L"vs_5_0");
				auto ps = renderer->compileShader(L"shaders/shaders.hlsl", L"PSMain", L"ps_5_0");
				std::vector<Renderer::Shader::Ptr> shaders = { vs, ps };
				std::vector<Renderer::RootParameter> rootparams = {D3D12_SHADER_VISIBILITY_PIXEL};
				rootparams[0].srv(0);
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

				pso = renderer->createPipelineState(shaders, rs, rootparams);

				std::pair<Vector3, Vector4> triangleVertices[] =
				{
					{ { 0.0f, 0.25f , 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
					{ { 0.25f, -0.25f , 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
					{ { -0.25f, -0.25f , 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
				};

				vertices = renderer->createBuffer(sizeof(triangleVertices), sizeof(std::pair<Vector3, Vector4>), D3D12_HEAP_TYPE_DEFAULT, triangleVertices, sizeof(triangleVertices));

				tex = renderer->createTexture(L"test.png");

				auto mainbar = ImguiObject::root()->createChild<ImguiMenuBar>(true);
				fps = mainbar->createChild<ImguiText>("test");
			}

			void renderScreen()
			{
			}

			void updateImpl()
			{
				static auto lastTime = GetTickCount64();
				auto cur = GetTickCount64();
				auto delta = cur - lastTime;
				if (delta > 0)
				{
					lastTime = cur;

					float time = 1000.0f / delta;
					static float history = time;

					history =  history * 0.99f + time * 0.01f;

					std::stringstream ss;
					ss.precision(4);
					ss << history;
					fps->text = ss.str();
				}

				pipeline.update();
			}

			void renderScene(Camera::Ptr cam, UINT, UINT)
			{
				auto renderer = Renderer::getSingleton();
				auto bb = renderer->getBackBuffer();
				auto cmdlist = renderer->getCommandList();
				cmdlist->setPipelineState(pso);

				cmdlist->setTexture(0, tex);


				auto desc = bb->getTexture()->getDesc();
				cmdlist->setViewport({
					0.0f, 0.0f,(float) desc.Width,(float)desc.Height, 0.0f, 1.0f
					});

				cmdlist->setScissorRect({ 0,0, (LONG)desc.Width, (LONG)desc.Height });

				cmdlist->setPrimitiveType();
				cmdlist->setVertexBuffer(vertices);
				cmdlist->drawInstanced(3);

			}
		} frame;


		frame.init();
		frame.update();
	}

	_CrtDumpMemoryLeaks();
	return 0;
}

#endif