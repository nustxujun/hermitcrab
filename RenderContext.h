#pragma once

#include "Common.h"
#include "Renderer.h"


struct Object
{
	using Ptr = std::shared_ptr<Object>;
	std::string name;
};

class Camera : public Object
{
public:
	using Ptr = std::shared_ptr<Camera>;
public:
	void setViewMatrix(const Matrix& v);
	void setProjectionMatrix(const Matrix& v);
	void setViewport(float left, float top, float width, float height);

	const Matrix& getViewMatrix()const {return mView;}
	const Matrix& getProjectionMatrix()const {return mProjection;}
	const D3D12_VIEWPORT getViewport()const{return mViewport;}
private:
	Matrix mView;
	Matrix mProjection;
	D3D12_VIEWPORT mViewport;
};

struct Texture: public Object
{
	using Ptr = std::shared_ptr<Texture>;

	void init(int width, int height, DXGI_FORMAT format, const void* data)
	{
		texture = Renderer::getSingleton()->createTexture(width, height, format, data);
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

	void init(const std::vector<char>& vs, const std::vector<char>& is, size_t vsstride, size_t isstride)
	{
		vertices = Renderer::getSingleton()->createBuffer(vs.size(),vsstride, D3D12_HEAP_TYPE_DEFAULT, vs.data(), vs.size());
		indices = Renderer::getSingleton()->createBuffer(is.size(), isstride, D3D12_HEAP_TYPE_DEFAULT, is.data(), is.size());

	}
};

struct Model : public Object
{
	using Ptr = std::shared_ptr<Model>;

	std::vector<Mesh::Ptr> meshs;
	Matrix transform;
};

class RenderContext
{
	static RenderContext* instance;
public:
	virtual void renderScene(Camera::Ptr cam, UINT flags = 0, UINT mask = 0xffffffff) = 0;
	virtual void renderScreen() = 0;

	template<class T>
	std::shared_ptr<T> createObject(const std::string& name)
	{
		auto o = std::shared_ptr<T>(new T());
		o->name = name;
		mObjects[name] = o;
		return o;
	}

	template<class T>
	std::shared_ptr<T> getObject(const std::string& name)
	{
		auto ret = mObjects.find(name);
		if (ret == mObjects.end())
			return {};
		else
			return std::static_pointer_cast<T>(ret->second);

	}

	void resize(int width, int height);
	Camera::Ptr getMainCamera()const;

	RenderContext()
	{
		instance = this;

		mCamera = Camera::Ptr(new Camera());
	}

	virtual ~RenderContext()
	{
		instance = nullptr;
	}
	
	static RenderContext* getSingleton()
	{
		return instance;
	}
private:
	Camera::Ptr mCamera;
	std::map<std::string, Object::Ptr> mObjects;
};
