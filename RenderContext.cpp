#include "RenderContext.h"

RenderContext* RenderContext::instance = nullptr;

void RenderContext::resize(int width, int height)
{
	mCamera->setViewport(0,0,(float)width, (float)height);
}

Camera::Ptr RenderContext::getMainCamera() const
{
	return mCamera;
}

void Camera::setViewMatrix(const Matrix & v)
{
	mView = v;
}

void Camera::setProjectionMatrix(const Matrix & v)
{
	mProjection = v;
}

void Camera::setViewport(float left, float top, float width, float height)
{
	mViewport = {left, top, width, height, 0, 1.0f};
}
