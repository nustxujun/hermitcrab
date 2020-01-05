#include "RenderContext.h"

RenderContext* RenderContext::instance = nullptr;



void RenderContext::renderScreen(const Quad* quad)
{
	auto renderer = Renderer::getSingleton();
	auto cmdlist = renderer->getCommandList();

	auto size = renderer->getSize();
	cmdlist->setScissorRect({0,0, size[0], size[1]});
	cmdlist->setPipelineState(quad->getPipelineState());

	cmdlist->setVertexBuffer(quad->getSharedVertices());
	cmdlist->drawInstanced(6);
}

void RenderContext::resize(int width, int height)
{
	mCamera->viewport = {0,0,(float)width, (float)height, 0, 1.0f};
}

Camera::Ptr RenderContext::getMainCamera() const
{
	return mCamera;
}

