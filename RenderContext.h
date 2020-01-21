#pragma once

#include "Common.h"
#include "Renderer.h"
#include "Quad.h"
#include <iostream>


struct Object
{
	using Ptr = std::shared_ptr<Object>;
	std::string name;
};

class Camera : public Object
{
public:
	using Ptr = std::shared_ptr<Camera>;
	
	Vector3 dir;
	Vector3 pos;
	Matrix view;
	Matrix proj;
	D3D12_VIEWPORT viewport;


};

struct Texture: public Object
{
	using Ptr = std::shared_ptr<Texture>;

	void init(int width, int height, DXGI_FORMAT format, const void* data)
	{
		texture = Renderer::getSingleton()->createTexture((UINT)width, (UINT)height, format,0, data);
		texture->setName(M2U("Texture " + name));
	}
	Renderer::Texture::Ref texture;
};

struct Mesh : public Object
{
	using Ptr = std::shared_ptr<Mesh>;

	Texture::Ptr texture;
	Renderer::Buffer::Ptr vertices;
	Renderer::Buffer::Ptr indices;
	size_t numIndices;

	void init(const std::vector<char>& vs, const std::vector<char>& is, size_t vsstride, size_t isstride, size_t ni)
	{
		vertices = Renderer::getSingleton()->createBuffer((UINT)vs.size(), (UINT)vsstride, D3D12_HEAP_TYPE_DEFAULT, vs.data(), (UINT)vs.size());
		indices = Renderer::getSingleton()->createBuffer((UINT)is.size(), (UINT)isstride, D3D12_HEAP_TYPE_DEFAULT, is.data(), (UINT)is.size());
		numIndices = ni;

		vertices->getResource()->setName(M2U("Vertex " + name));
		indices->getResource()->setName(M2U("Index " + name ));

	}
};

struct Material: public Object
{
	using Ptr = std::shared_ptr<Material>;
	enum class Visualizaion
	{
		Final,
		BaseColor,
		Normal,
	};


	Renderer::PipelineState::Ref pipelineState;
	std::map<std::string, Vector4> parameters;
	std::map<std::string, Texture::Ptr > textures;

	struct Shader
	{
		std::string vs;
		std::string ps;
		std::string psblob;
	} shaders;

	void apply(const Renderer::ConstantBuffer::Ptr& vscbuffer, const Renderer::ConstantBuffer::Ptr& pscbuffer)
	{
		if (vscbuffer)
			pipelineState->setVSConstant("VSConstant", vscbuffer);
		if (pscbuffer)
		{
			for (auto& p : parameters)
				pscbuffer->setVariable(p.first, &p.second);

			pipelineState->setPSConstant("PSConstant", pscbuffer);
		}

		for (auto& t : textures)
		{
			pipelineState->setPSResource(t.first, t.second->texture->getShaderResource());

		}
	}

	const char* genShaderContent(Visualizaion v);
	void compileShaders(Visualizaion v);
	void init(const std::string& vsname, const std::string& psname, const std::string& pscontent);
};

struct Model : public Object
{
	using Ptr = std::shared_ptr<Model>;

	std::vector<Mesh::Ptr> meshs;
	Matrix transform;
	Material::Ptr material;
	Renderer::ConstantBuffer::Ptr vcbuffer;
	Renderer::ConstantBuffer::Ptr pcbuffer;

	void init(Material::Ptr m)
	{
		material = m;
		vcbuffer = m->pipelineState->createConstantBuffer(Renderer::Shader::ST_VERTEX,"VSConstant");
		pcbuffer = m->pipelineState->createConstantBuffer(Renderer::Shader::ST_PIXEL, "PSConstant");

	}
};

struct Light : public Object
{
	using Ptr = std::shared_ptr<Light>;
	enum Type
	{
		LT_DIR,
		LT_POINT,
		LT_SPOT,
	};

	UINT32 type;
	Color color;
	Vector3 dir;
};

class RenderContext
{
	static RenderContext* instance;
public:
	void recompileMaterials(Material::Visualizaion v);
	virtual void renderScene(Camera::Ptr cam, UINT flags = 0, UINT mask = 0xffffffff) = 0;
	virtual void renderScreen(const Quad* quad) ;

	template<class T>
	std::shared_ptr<T> createObject(const std::string& name)
	{
		auto ret = getObject<T>(name);
		if (ret)
			return ret;

		std::cout << "create object: " << name << std::endl;
		auto o = std::shared_ptr<T>(new T());
		o->name = name;
		mObjects[name] = o;
		process(o);
		return o;
	}

	template<class T>
	std::shared_ptr<T> getObject(const std::string& name)
	{
		auto ret = mObjects.find(name);
		if (ret == mObjects.end())
		{
			//Common::Assert(false, "cannot find object: " + name);
			//std::cout << "cannot find " << name << std::endl;
			return {};
		}
		else
			return std::static_pointer_cast<T>(ret->second);

	}

	void resize(int width, int height);
	Camera::Ptr getMainCamera()const;

	RenderContext()
	{
		instance = this;

		mCamera = createObject<Camera>("main");


	}

	virtual ~RenderContext()
	{
		instance = nullptr;
	}
	
	static RenderContext* getSingleton()
	{
		return instance;
	}

	template<class T>
	void process(const std::shared_ptr<T>& o)
	{
	}

	template<>
	void process(const Light::Ptr& l)
	{
		mLights.push_back(l);
	}

	template<>
	void process(const  Model::Ptr& model)
	{
		mRenderList.push_back(model);
	}

	template<>
	void process(const Material::Ptr& m)
	{
		mMaterials.push_back(m);
	}
protected:
	Camera::Ptr mCamera;
	std::map<std::string, Object::Ptr> mObjects;
	std::vector<Light::Ptr> mLights;
	std::vector<Model::Ptr> mRenderList;
	std::vector<Material::Ptr> mMaterials;

};
