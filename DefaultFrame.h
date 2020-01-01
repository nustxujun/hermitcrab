#pragma once


#include "Framework.h"
#include "Pipeline.h"
#include "RenderContext.h"
#include "RenderCommand.h"
#include "Renderer.h"

#include <array>

class DefaultFrame :public RenderContext, public Framework
{
	RenderCommand rendercmd = false;
	std::shared_ptr<DefaultPipeline> pipeline;
	Renderer::ConstantBuffer::Ptr lightsConsts;

	struct LightInfo
	{
		Vector4 pos; // pos and range
		Vector4 dir;
		Vector4 color;
	};

	std::vector<LightInfo> lights;
public:
	void init()
	{
		pipeline = decltype(pipeline)(new DefaultPipeline);

		rendercmd.record();

		auto cam = getObject<Camera>("main");

		Framework::resize(cam->viewport.Width, cam->viewport.Height);
		lightsConsts = Renderer::getSingleton()->createConstantBuffer(sizeof(LightInfo) * 4);
	}

	void renderScreen()
	{
	}

	void updateLights()
	{
		struct {
			LightInfo lights[4];
			int numlights;
		}infos;

		memcpy(infos.lights, lights.data(), sizeof(LightInfo) * lights.size());

		lightsConsts->blit(&infos,0, sizeof(infos));
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
			float time = (float)framecount * 1000.0f / (float)delta;
			framecount = 0;
			static float history = time;
			history = history * 0.99f + time * 0.01f;

			std::stringstream ss;
			ss.precision(4);
			ss << history << "(" << 1000.0f / history << "ms)";
			::SetWindowTextA(Renderer::getSingleton()->getWindow(), ss.str().c_str());
		}

		updateLights();
		pipeline->update();

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
			model->material->pipelineState->setPSConstant("Lights", lightsConsts);
			for (auto& mesh : model->meshs)
			{
				cmdlist->setVertexBuffer(mesh->vertices);
				cmdlist->setIndexBuffer(mesh->indices);
				cmdlist->drawIndexedInstanced(mesh->numIndices, 1U);
			}
		}
	}

};


