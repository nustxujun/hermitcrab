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

	auto rect = quad->getRect();
	cmdlist->setScissorRect(rect);
	cmdlist->setViewport({0,0, (float)rect.right, (float)rect.bottom, 0.0f, 1.0f});
	cmdlist->setPipelineState(quad->getPipelineState());
	cmdlist->setVertexBuffer(quad->getSharedVertices());
	cmdlist->setPrimitiveType();
	
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

void Material::applyTextures()
{
	for (auto& t : textures)
	{
		pipelineState->setPSResource(t.first, t.second->texture->getShaderResource());
	}
}

const char * Material::genShaderContent(Visualizaion v)
{
	static std::string content;
	content.clear();
	switch (v)
	{
	case Visualizaion::Final:
		content += "half3 _final = (half3)directBRDF(Roughness, Metallic, F0_DEFAULT, Base_Color.rgb, _normal.xyz,-sundir, campos - input.worldPos);";
		content += " return half4(_final * suncolor.rgb + Emissive_Color.rgb,1);";
		break;
	case Visualizaion::BaseColor:
		content += "return half4(Base_Color.rgb + Emissive_Color.rgb, 1) ;";
		break;
	case Visualizaion::VertexColor:
		content += "return input.color;";
		break;
	case Visualizaion::Roughness:
		content += "return Roughness;";
		break;
	case Visualizaion::Metallic:
		content += "return Metallic;";
		break;
	case Visualizaion::Normal:
		content += "return half4(_normal.xyz * 0.5f + 0.5f, 1);";
		break;
	}


	return content.c_str();
}

void Material::compileShaders(Visualizaion v)
{

	if (pipelineStateCaches[(size_t)v])
	{
		pipelineState = pipelineStateCaches[(size_t)v];
		return;
	}
	auto renderer = Renderer::getSingleton();

	auto vs = renderer->compileShaderFromFile(shaders.vs, "vs", SM_VS);
	std::string blob = shaders.psblob;
	const char content_macro[] = "__SHADER_CONTENT__";
	blob.replace(blob.find(content_macro),sizeof(content_macro),genShaderContent(v));
	auto ps = renderer->compileShader(shaders.ps, blob, "ps", SM_PS);
	ps->enable32BitsConstantsByName("PSConstant");
	std::vector<Renderer::Shader::Ptr> ss = { vs, ps };
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
		{ "COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		});


	pipelineState = renderer->createPipelineState(ss, rs);
	pipelineStateCaches[(size_t)v] = pipelineState;
	pipelineState->get()->SetName(M2U("Material " + name).c_str());
}

void Material::init(const std::string& vsname, const std::string& psname, const std::string& pscontent)
{
	shaders.vs = vsname;
	shaders.ps = psname;
	shaders.psblob = pscontent;

	compileShaders(Visualizaion::Final);
}

void ReflectionProbe::init(UINT cubesize, const void* data, UINT size)
{
	UINT textureSize = cubesize * cubesize * 64;

	UINT miplevels = 0;
	UINT total = 0;
	while (total < size)
	{
		total += textureSize;
		textureSize /= 4;
		miplevels++;
	}

	auto renderer = Renderer::getSingleton();
	auto texcube = renderer->createTexture(cubesize,cubesize,1,DXGI_FORMAT_R16G16B16A16_FLOAT, miplevels);


}
