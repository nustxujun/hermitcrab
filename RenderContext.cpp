#include "RenderContext.h"

RenderContext* RenderContext::instance = nullptr;



void RenderContext::recompileMaterials(Material::Visualizaion v)
{
	for (auto& m: mMaterials)
		m->compileShaders(v);
}

void RenderContext::renderScreen(const Quad* quad, Renderer::CommandList::Ref cmdlist)
{
	auto renderer = Renderer::getSingleton();
	auto rect = quad->getRect();
	cmdlist->setScissorRect(rect);
	cmdlist->setViewport({ 0,0, (float)rect.right, (float)rect.bottom, 0.0f, 1.0f });
	cmdlist->setPipelineState(quad->getPipelineState());
	cmdlist->setPrimitiveType(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	// draw without vertexbuffer
	cmdlist->drawInstanced(4);

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

	pipelineState->setPSResource("ReflectionEnvs", ReflectionProbe::textureCubeArray->getShaderResource());
	pipelineState->setPSResource("PreintegratedGF", Texture::LUT->getShaderResource());

}

std::string Material::genShaderContent(Visualizaion v)
{
	std::string content;
	switch (v)
	{
	case Visualizaion::Final:
		content += "	half3 R = normalize(reflect(-V.xyz, _normal.xyz));\n";
		content += "	half3 _directLitColor = (half3)directBRDF(Roughness, Metallic, F0_DEFAULT, Base_Color.rgb, _normal.xyz,-sundir, V);\n";
		content += "	half4 refenvscoord = half4(R.xyz, 0);\n";
		// get mip from roughness (ue4)
		content += Common::format("	half _mip = ", ReflectionProbe::miplevels - 1, " - 1 -", 1, " + 1.2f * log2(Roughness);\n");
		content += "	half3 _prefiltered = ReflectionEnvs.SampleLevel(linearSampler, refenvscoord, _mip).rgb;\n";
		content += "	half3 _lutvalue = LUT(_normal.xyz, V, Roughness, PreintegratedGF,linearClamp);\n";
		content += "	half3 _iblcolor = indirectBRDF(0, _prefiltered, _lutvalue, Roughness, Metallic, F0_DEFAULT, Base_Color.rgb, _normal.xyz, V);\n";
		content += "	return half4(_directLitColor * suncolor.rgb + Emissive_Color.rgb + _iblcolor.rgb,1);\n";
		//content += "	return half4(_lutvalue.xyz,1);";
		break;
	case Visualizaion::BaseColor:
		content += "return half4(Base_Color.rgb , 1) ;";
		break;
	case Visualizaion::EmissiveColor:
		content += "return half4(Emissive_Color.rgb, 1) ;";
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


	return content;
}

std::string Material::genBoundResouces()
{
	std::string ret;
	ret += "TextureCubeArray ReflectionEnvs;\n";
	ret += "Texture2D PreintegratedGF;\n";
	ret +=	"cbuffer PSConstant\n"
			"{\n"
			"	float3	objpos;\n"
			"	float	objradius;\n"
			"};\n";

	return ret;
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
	const char resource_macro[] = "__BOUND_RESOURCE__";
	blob.replace(blob.find(resource_macro), sizeof(resource_macro), genBoundResouces());
	auto ps = renderer->compileShader(shaders.ps, blob, "ps", SM_PS);
	//ps->enable32BitsConstantsByName("PSConstant");
	std::vector<Renderer::Shader::Ptr> ss = { vs, ps };
	ps->registerStaticSampler({
		D3D12_FILTER_MIN_MAG_MIP_POINT,
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
		1,0,
		D3D12_SHADER_VISIBILITY_PIXEL
		});

	ps->registerStaticSampler({
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0,0,
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		0,
		D3D12_FLOAT32_MAX,
		2,0,
		D3D12_SHADER_VISIBILITY_PIXEL
		});

	ps->registerStaticSampler({
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		0,0,
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		0,
		D3D12_FLOAT32_MAX,
		3,0,
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

Renderer::Resource::Ref ReflectionProbe::textureCubeArray;
void ReflectionProbe::initTextureCubeArray(const std::vector<Ptr>& probes)
{
	auto renderer = Renderer::getSingleton();
	if (textureCubeArray)
		renderer->destroyResource(textureCubeArray);
	auto texcube = renderer->createTextureCubeArray(cubeSize, DXGI_FORMAT_R16G16B16A16_FLOAT, (UINT)probes.size(),miplevels );
	texcube->setName("ReflectionEnvs");
	textureCubeArray = texcube;

	UINT arrayIndex = 0;
	for (auto& p : probes)
	{
		UINT textureSize = cubeSize * cubeSize * 8;
		
		const char* buffer = p->textureData.data();
		for (UINT i = 0; i < miplevels; ++i)
		{
			for (UINT j = 0; j < 6; ++j)
			{
				renderer->updateTexture(texcube, i + j * miplevels + arrayIndex * miplevels * 6, buffer, textureSize, false);
				buffer += textureSize;
			}
			textureSize /= 4;
		}

		arrayIndex++;
	}

}

void ReflectionProbe::init(UINT cubesize, const void* data, UINT size)
{
	Common::Assert(cubesize == cubeSize, "all reflection cubemap must be the same size");
	Common::Assert(dataSize == size, "reflection probe data is invalid.");
	textureData.reserve(size);
	memcpy(textureData.data(), data, size);
}

Renderer::Resource::Ref Texture::LUT;

void Texture::createLUT()
{
	if (LUT)
		Renderer::getSingleton()->destroyResource(LUT);

	LUT = Renderer::getSingleton()->createTextureFromFile("lut.png", false);
}

void Sky::init()
{
	model = RenderContext::getSingleton()->createObject<Model>(name +  "_model");

	model->mesh = (mesh);
	model->materials.push_back(material);
	model->transform = transform;
	model->normTransform = transform;
	model->aabb = aabb;
	model->init();
	
}

void Model::init()
{

	for (auto& m : materials)
	{
		cbuffers.push_back({
			m->pipelineState->createConstantBuffer(Renderer::Shader::ST_VERTEX, "VSConstant"),
			{},{},{},
			m->pipelineState->createConstantBuffer(Renderer::Shader::ST_PIXEL, "PSConstant"),
		});
	}

	boundingradius = std::max(aabb.extent[0], std::max(aabb.extent[1], aabb.extent[2]));
}

void Model::visitConstant(Renderer::Shader::ShaderType type,UINT index, const std::function<void(Renderer::ConstantBuffer::Ptr&)>& visitor)
{
	if (index >= cbuffers.size())
		return;

	auto& cb = cbuffers[index];
	if (!cb[type])
		return;

	visitor(cb[type]);
}
