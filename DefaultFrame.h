#pragma once


#include "Framework.h"
#include "Pipeline.h"
#include "RenderContext.h"
#include "RenderCommand.h"
#include "Renderer.h"

#include <array>

class DefaultFrame :public RenderContext, public Framework
{
public:
	RenderCommand rendercmd ;
	std::shared_ptr<DefaultPipeline> pipeline;
	Renderer::ConstantBuffer::Ptr commonConsts;


public:
	void init(bool runwithipc = false)
	{
		Framework::initialize();
		pipeline = decltype(pipeline)(new DefaultPipeline);

		if (runwithipc)
		{
			rendercmd.init(false);
			rendercmd.record();
			commonConsts = mRenderList[0]->materials[0]->pipelineState->createConstantBuffer(Renderer::Shader::ST_PIXEL,"CommonConstants");
		}
		auto cam = getObject<Camera>("main");

		Framework::resize(cam->viewport.Width, cam->viewport.Height);
	}

	void renderScreen()
	{
	}

	void updateConsts()
	{
		struct {
			Vector4 campos;
			Vector4 camdir;

			struct 
			{
				Vector4 pos; // pos and range
				Vector4 dir;
				Vector4 color;
			} lights[4];
			int numlights;

			Vector3 sundir;
			Vector4 suncolor;
		}infos = {};

		for (auto& l : mLights)
		{
			switch (l->type)
			{
			case Light::LT_DIR:{infos.sundir = l->dir; infos.suncolor = l->color;} break;
			case Light::LT_POINT:
			case Light::LT_SPOT:
			default:
				break;
			}
		}

		auto cam = getObject<Camera>("main");
		infos.camdir = { cam->dir[0],cam->dir[1],cam->dir[2],0 };
		infos.campos = { cam->pos[0],cam->pos[1],cam->pos[2],0 };

		infos.suncolor = { infos.suncolor[0] ,infos.suncolor[1] ,infos.suncolor[2]  };
		if (commonConsts)
			commonConsts->blit(&infos,0, sizeof(infos));
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

		updateConsts();
		pipeline->update();

	}

	void renderScene(Camera::Ptr cam, UINT, UINT)
	{
		auto renderer = Renderer::getSingleton();
		auto cmdlist = renderer->getCommandList();


		cmdlist->setViewport(cam->viewport);

		cmdlist->setScissorRect({ 0,0, (LONG)cam->viewport.Width, (LONG)cam->viewport.Height });

		cmdlist->setPrimitiveType();

		for (auto& model : mRenderList)
		{
			for (auto& mesh : model->meshs)
			{
				cmdlist->setVertexBuffer(mesh->vertices);
				cmdlist->setIndexBuffer(mesh->indices);

				for (auto& sm : mesh->submeshes)
				{
					auto& material = model->materials[sm.materialIndex];
					auto& pso = material->pipelineState;
					pso->setVSVariable("view", &cam->view);
					pso->setVSVariable("proj", &cam->proj);
					pso->setVSVariable("world", &model->transform);
					pso->setVSVariable("nworld", &model->normTransform);
					
					if (commonConsts)
						pso->setPSConstant("CommonConstants", commonConsts);

					cmdlist->setPipelineState(pso);
					cmdlist->drawIndexedInstanced(sm.numIndices,1U,sm.startIndex, sm.startVertexIndex);
				}
			}
		}
	}

};


