#pragma once

#include "Common.h"



class Camera
{
public:
	using Ptr = std::shared_ptr<Camera>;
public:
	void setViewMatrix(const Matrix& v);
	void setProjectionMatrix(const Matrix& v);
	void setViewPort(float left, float top, float width, float height);

private:
	Matrix mView;
	Matrix mProjection;
	D3D12_VIEWPORT mViewport;
};

class RenderContext
{
	static RenderContext* instance;
public:
	virtual void renderScene(Camera::Ptr cam, UINT flags = 0, UINT mask = 0xffffffff) = 0;
	virtual void renderScreen() = 0;

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
};
