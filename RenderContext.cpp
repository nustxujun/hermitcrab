#include "RenderContext.h"

RenderContext* RenderContext::instance = nullptr;



void RenderContext::resize(int width, int height)
{
	mCamera->viewport = {0,0,(float)width, (float)height, 0, 1.0f};
}

Camera::Ptr RenderContext::getMainCamera() const
{
	return mCamera;
}

void RenderContext::addToRenderList(Model::Ptr model)
{
	mRenderList.push_back(model);
}

