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

	void init(int width, int height, DXGI_FORMAT format, const void* data, bool srgb)
	{
		texture = Renderer::getSingleton()->createTexture2D((UINT)width, (UINT)height, format,0, data, srgb);
		texture->setName(("Texture " + name));
	}
	Renderer::Resource::Ref texture;


	static Renderer::Resource::Ref LUT;
	static void createLUT();
};

struct Mesh : public Object
{
	using Ptr = std::shared_ptr<Mesh>;
	struct SubMesh
	{
		UINT materialIndex;
		UINT startIndex;
		UINT numIndices;
	};

	Texture::Ptr texture;
	Renderer::Buffer::Ptr vertices;
	Renderer::Buffer::Ptr indices;
	size_t numIndices;
	std::vector<SubMesh> submeshes ;

	void init(const std::vector<char>& vs, const std::vector<char>& is, size_t vsstride, size_t isstride, size_t ni)
	{
		vertices = Renderer::getSingleton()->createBuffer((UINT)vs.size(), (UINT)vsstride,false, D3D12_HEAP_TYPE_DEFAULT, vs.data(), (UINT)vs.size());
		indices = Renderer::getSingleton()->createBuffer((UINT)is.size(), (UINT)isstride, false, D3D12_HEAP_TYPE_DEFAULT, is.data(), (UINT)is.size());
		numIndices = ni;

		vertices->getResource()->setName(("Vertex " + name));
		indices->getResource()->setName(("Index " + name ));

	}
};

struct Material: public Object
{
	using Ptr = std::shared_ptr<Material>;
	enum class Visualizaion
	{
		Final,
		VertexColor,
		BaseColor,
		Roughness,
		Metallic,

		Normal,
		Num,
	};


	Renderer::PipelineState::Ref pipelineState;
	std::array<Renderer::PipelineState::Ref,(size_t)Visualizaion::Num> pipelineStateCaches;

	std::map<std::string, Texture::Ptr > textures;

	struct Shader
	{
		std::string vs;
		std::string ps;
		std::string psblob;
	} shaders;

	void applyTextures();


	void init(const std::string& vsname, const std::string& psname, const std::string& pscontent);
	void compileShaders(Visualizaion v);

private:
	std::string genShaderContent(Visualizaion v);
	std::string genBoundResouces();
	
};

struct Model : public Object
{
	using Ptr = std::shared_ptr<Model>;

	std::vector<Mesh::Ptr> meshs;
	Matrix transform;
	Matrix normTransform;
	std::vector<Material::Ptr> materials;
	Renderer::ConstantBuffer::Ptr vcbuffer;
	//Renderer::ConstantBuffer::Ptr pcbuffer;

	void init()
	{
		vcbuffer = materials[0]->pipelineState->createConstantBuffer(Renderer::Shader::ST_VERTEX,"VSConstant");
		//pcbuffer = m->pipelineState->createConstantBuffer(Renderer::Shader::ST_PIXEL, "PSConstant");

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

struct ReflectionProbe :public Object
{
	using Ptr = std::shared_ptr<ReflectionProbe>;
	
	// instance
	Matrix transform;
	float influence = 1.0f;
	float brightness = 1.0f;

	std::vector<char> textureData;
	void init(UINT cubesize, const void* data, UINT size);

	// shared
	const static UINT miplevels = 8;
	const static UINT cubeSize = 128;
	const static UINT dataSize = 1048560; // cubeSize * cubeSize * 6 (faces) * 8 (half float) with all mips
	static Renderer::Resource::Ref textureCubeArray;
	static void initTextureCubeArray(const std::vector<Ptr>& probes);

};

struct Environment : public Object
{
	using Ptr = std::shared_ptr<Environment>;

	Mesh::Ptr mesh;
	Material::Ptr material;
	Matrix transform;
	void init();

	Model::Ptr model;
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
			
		auto& type = typeid(T);

		std::cout << "create " << type.name() << " : " << name << std::endl;
		auto o = std::shared_ptr<T>(new T());
		o->name = name;
		mObjects[type.name()][name] = o;
		process(o);
		return o;
	}

	template<class T>
	std::shared_ptr<T> getObject(const std::string& name)
	{
		auto& type = typeid(T);
		auto ret = mObjects[type.name()].find(name);
		if (ret == mObjects[type.name()].end())
		{
			//Common::Assert(false, "cannot find object: " + name);
			//std::cout << "cannot find " << name << std::endl;
			return {};
		}
		else
			return std::static_pointer_cast<T>(ret->second);

	}

	template<class T>
	void visiteObjects(const std::function<void(std::shared_ptr<T>)>& f)
	{
		auto& type = typeid(T);
		for (auto& o : mObjects[type.name()])
		{
			f(std::static_pointer_cast<T>(o.second));
		}
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
	std::map<std::string,std::map<std::string, Object::Ptr>> mObjects;
	std::vector<Light::Ptr> mLights;
	std::vector<Model::Ptr> mRenderList;
	std::vector<Material::Ptr> mMaterials;

};
