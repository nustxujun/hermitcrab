// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "Framework.h"
#include "Pipeline.h"
#include "RenderContext.h"
#include "ImGuiOverlay.h"
#include <sstream>
#include "SimpleIPC.h"
#include <thread>
#include "RenderCommand.h"

#if defined(NO_UE4) || defined(_CONSOLE)

struct End
{
	~End()
	{
		_CrtDumpMemoryLeaks();
	}
}end;

int main()
{

	{

		class Frame : public Framework, public RenderContext
		{
		public:
			Renderer::PipelineState::Ref pso;
			Renderer::Buffer::Ptr vertices;
			Renderer::Texture::Ref tex;
			DefaultPipeline pipeline;
			void init()
			{
				RenderCommand::init(false);
				RenderCommand::getSingleton()->record();

				auto cam = getObject<Camera>("main");

				Framework::resize(cam->viewport.Width, cam->viewport.Height);

			}

			void renderScreen()
			{
			}

			void updateImpl()
			{
				static auto lastTime = GetTickCount64();
				static auto framecount = 0;
				auto cur = GetTickCount64();
				auto delta = cur - lastTime;
				framecount++;
				if (delta > 0)
				{
					lastTime = cur;
					float time = (float)framecount * 1000.0f/ (float)delta ;
					framecount = 0;
					static float history = time;
					history =  history * 0.99f + time * 0.01f;

					std::stringstream ss;
					ss.precision(4);
					ss << history << "(" << 1000.0f / history << "ms)";
					::SetWindowTextA(Renderer::getSingleton()->getWindow(),ss.str().c_str());
				}

				pipeline.update();
			}

			void renderScene(Camera::Ptr cam, UINT, UINT)
			{
				auto renderer = Renderer::getSingleton();
				auto bb = renderer->getBackBuffer();
				auto cmdlist = renderer->getCommandList();


				cmdlist->setViewport(cam->viewport);

				cmdlist->setScissorRect({ 0,0, (LONG)cam->viewport.Width, (LONG)cam->viewport.Height });

				cmdlist->setPrimitiveType();

				for (auto& model : mRenderList)
				{
					cmdlist->setPipelineState(model->material->pipelineState);
					model->vcbuffer->setVariable("view", &cam->view);
					model->vcbuffer->setVariable("proj", &cam->proj);
					model->vcbuffer->setVariable("world", &model->transform);
					model->material->apply(model->vcbuffer, model->pcbuffer);
					for (auto& mesh : model->meshs)
					{
						cmdlist->setVertexBuffer(mesh->vertices);
						cmdlist->setIndexBuffer(mesh->indices);
						cmdlist->drawIndexedInstanced(mesh->numIndices, 1U);
					}
				}
			}
		} frame;


		frame.init();
		frame.update();
	}

	return 0;
}

#endif