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

const char * Material::genShaderContent()
{
	static std::string content;
	content.clear();
	content += "half3 _final = directBRDF(Roughness, Metallic, F0_DEFAULT, Base_Color.rgb, _normal.xyz,-sundir, campos - input.worldPos);";
	content += " return half4(_final ,1) * suncolor;";

	return content.c_str();
}
