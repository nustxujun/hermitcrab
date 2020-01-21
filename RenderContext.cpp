#include "RenderContext.h"

RenderContext* RenderContext::instance = nullptr;



void RenderContext::recompileMaterials(Material::Visualizaion v)
{
	for (auto& m: mMaterials)
		m->compileShaders(v);
}

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

const char * Material::genShaderContent(Visualizaion v)
{
	static std::string content;
	content.clear();
	switch (v)
	{
	case Visualizaion::Final:
		content += "half3 _final = directBRDF(Roughness, Metallic, F0_DEFAULT, Base_Color.rgb, _normal.xyz,-sundir, campos - input.worldPos);";
		content += " return half4(_final ,1) * suncolor;";
		break;
	case Visualizaion::BaseColor:
		content += "return half4(Base_Color.rgb, 1);";
		break;
	case Visualizaion::Normal:
		content += "return half4(_normal.xyz * 0.5f + 0.5f, 1);";
		break;
	}


	return content.c_str();
}

void Material::compileShaders(Visualizaion v)
{
	auto renderer = Renderer::getSingleton();

	auto vs = renderer->compileShaderFromFile(shaders.vs, "vs", SM_VS);
	auto ps = renderer->compileShader(shaders.ps, shaders.psblob, "ps", SM_PS, { { "__SHADER_CONTENT__", genShaderContent(v) } });
	std::vector<Renderer::Shader::Ptr> shaders = { vs, ps };
	ps->registerStaticSampler({
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		0,0,
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		0,
		D3D12_FLOAT32_MAX,
		0,0,
		D3D12_SHADER_VISIBILITY_PIXEL
		});
	Renderer::RenderState rs = Renderer::RenderState::GeneralSolid;
	rs.setInputLayout({
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		});

	if (pipelineState)
		renderer->destroyPipelineState(pipelineState);
	pipelineState = renderer->createPipelineState(shaders, rs);
	pipelineState->get()->SetName(M2U("Material " + name).c_str());
}

void Material::init(const std::string& vsname, const std::string& psname, const std::string& pscontent)
{
	shaders.vs = vsname;
	shaders.ps = psname;
	shaders.psblob = pscontent;

	compileShaders(Visualizaion::Final);
}
