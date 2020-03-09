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
	float deltaTime = 0;

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
		if (!commonConsts)
			return;
	
		Vector3 sundir;
		Vector4 camdir, campos;
		Color suncolor;
		for (auto& l : mLights)
		{
			switch (l->type)
			{
			case Light::LT_DIR:{sundir = l->dir; suncolor = l->color;} break;
			case Light::LT_POINT:
			case Light::LT_SPOT:
			default:
				break;
			}
		}

		auto cam = getObject<Camera>("main");
		camdir = { cam->dir[0],cam->dir[1],cam->dir[2],0 };
		campos = { cam->pos[0],cam->pos[1],cam->pos[2],0 };


		commonConsts->setVariable("campos", &campos);
		commonConsts->setVariable("camdir", &camdir);
		int numlights = 0;
		commonConsts->setVariable("numlights", &numlights);
		commonConsts->setVariable("sundir", &sundir);
		commonConsts->setVariable("suncolor", &suncolor);
		commonConsts->setVariable("deltatime", &deltaTime);

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
			deltaTime = (float)framecount * 1000.0f / (float)delta;
			framecount = 0;
			static float history = deltaTime;
			history = history * 0.99f + deltaTime * 0.01f;

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
			model->vcbuffer->setVariable("view", &cam->view);
			model->vcbuffer->setVariable("proj", &cam->proj);
			model->vcbuffer->setVariable("world", &model->transform);
			model->vcbuffer->setVariable("nworld", &model->normTransform);

			struct Renderable
			{
				Mesh::SubMesh* sm;
				Mesh::Ptr m;
			};

			UINT numMaterials = model->materials.size();
			std::vector<std::vector<Renderable>> subs(numMaterials);
			
			for (auto& mesh : model->meshs)
			{
				for (auto& sm : mesh->submeshes)
				{
					subs[sm.materialIndex].push_back({&sm, mesh});
				}
			}
			
			for (UINT i = 0; i < numMaterials; ++i)
			{
				auto& material = model->materials[i];
				material->applyTextures();
				auto& pso = material->pipelineState;
				pso->setVSConstant("VSConstant", model->vcbuffer);
				if (commonConsts)
					pso->setPSConstant("CommonConstants", commonConsts);

				cmdlist->setPipelineState(pso);

				for (auto& sm : subs[i])
				{
					cmdlist->setVertexBuffer(sm.m->vertices);
					cmdlist->setIndexBuffer(sm.m->indices);
					cmdlist->drawIndexedInstanced(sm.sm->numIndices, 1U, sm.sm->startIndex);
				}
			}
		}
	}

};


