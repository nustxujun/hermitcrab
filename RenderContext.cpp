#include "RenderContext.h"

RenderContext* RenderContext::instance = nullptr;

Camera::Ptr RenderContext::getMainCamera() const
{
	return mCamera;
}
