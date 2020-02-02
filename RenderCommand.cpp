#include "RenderCommand.h"
#include "RenderContext.h"
#include "D3DHelper.h"
#include <iostream>

RenderCommand* RenderCommand::instance = 0;



RenderCommand::RenderCommand()
{

}

RenderCommand::~RenderCommand()
{
	mIPC.close();
}

void RenderCommand::init(bool host)
{
	if (host)
		mIPC.listen("renderstation");
	else
		mIPC.connect("renderstation");
}

void RenderCommand::done()
{
	mIPC << "done";
}

void RenderCommand::invalid()
{
	mIPC << "invalid";
	mIPC.invalid();
}
void RenderCommand::invalidSelf()
{
	mIPC.invalid();
}

void RenderCommand::createMesh(
	const std::string & name, 
	const void * vertices, 
	UINT32 bytesofvertices, 
	UINT32 numvertices, 
	UINT32 vertexStride,
	const void * indices, 
	UINT32 bytesofindices, 
	UINT32 numindices,
	UINT32 indexStride,
	const std::vector<SubMesh>& submeshs)
{
	mIPC << "createMesh";
	
	mIPC << name;
	mIPC << bytesofvertices;
	mIPC << numvertices;
	mIPC << vertexStride;
	mIPC.send(vertices, bytesofvertices);


	mIPC << bytesofindices;
	mIPC << numindices;
	mIPC << indexStride;
	mIPC.send(indices, bytesofindices);

	mIPC << (UINT)submeshs.size();
	for (auto& sm : submeshs)
		mIPC << sm;

}

void RenderCommand::createTexture(const std::string & name, int width, int height, DXGI_FORMAT format,const void* data)
{
	mIPC << "createTexture";

	UINT32 size = D3DHelper::sizeof_DXGI_FORMAT(format) * width * height;
	mIPC << name << width << height << format << size ;
	mIPC.send(data, size);
}

void RenderCommand::createModel(const std::string & name, const std::vector<std::string> meshs, const Matrix & transform, const Matrix& normaltransform, const std::vector<std::string>& materialNames)
{
	mIPC << "createModel";
	mIPC << name;
	UINT32 size = meshs.size();
	mIPC << size;
	for (auto& m : meshs)
		mIPC << m;

	mIPC << transform<< normaltransform;
	mIPC << (UINT) materialNames.size();
	for (auto& m:materialNames)
		mIPC << m;
}

void RenderCommand::createCamera(const std::string & name, const Vector3& pos, const Vector3& dir, const Matrix & view, const Matrix & proj, const D3D12_VIEWPORT & vp)
{
	mIPC << "createCamera" <<  name<< pos << dir << view << proj << vp;

}

void RenderCommand::createMaterial(const std::string & name, const std::string & vs, const std::string & ps, const std::string& pscontent, const std::set< std::string>& textures)
{
	mIPC << "createMaterial" << name << vs << ps << pscontent;

	mIPC << (UINT32)textures.size();
	for (auto& t: textures)
		mIPC << t;

}

void RenderCommand::createLight(const std::string& name, UINT32 type, const Color& color, const Vector3& dir)
{
	mIPC << "createLight" << name << type << color << dir;
}


void RenderCommand::record()
{
	std::map<std::string, std::function<bool(void)>> processors;

	processors["createMesh"] = [&ipc = mIPC]() {
		UINT32 numBytesVertices, numBytesIndices, numVertices, numIndices, verticesStride, indicesStride;
		std::vector<char> vertices, indices;
		
		std::string name;
		ipc >> name;

		ipc >> numBytesVertices >> numVertices >> verticesStride;
		vertices.resize(numBytesVertices);
		ipc.receive(vertices.data(), numBytesVertices);

		ipc >> numBytesIndices >> numIndices >> indicesStride;
		indices.resize(numBytesIndices);
		ipc.receive(indices.data(), numBytesIndices);

		UINT numSubmeshs;
		ipc  >> numSubmeshs;
		std::vector<SubMesh> submeshs(numSubmeshs);

		for (auto& sm:submeshs)
			ipc >> sm;

		auto context = RenderContext::getSingleton();
		auto mesh = context->createObject<Mesh>(name);

		mesh->init(vertices,indices,verticesStride, indicesStride, numIndices);
		for (auto& sm:submeshs)
			mesh->submeshes.push_back({
				sm.materialIndex,
				sm.startIndex,
				sm.numIndices,
			});
		return true;
	};

	processors["createTexture"] = [&ipc = mIPC]() {
		UINT32 width, height, size;
		DXGI_FORMAT fmt;
		std::string name;
		ipc >> name >>  width >> height >> fmt >> size ;
		std::vector<char> data;
		data.resize(size);

		ipc.receive(data.data(), size);

		auto context = RenderContext::getSingleton();
		auto tex = context->createObject<Texture>(name);
		tex->init(width,height, fmt, data.data());
		return true;
	};

	processors["createModel"] = [&ipc = mIPC]() {
		auto context = RenderContext::getSingleton();
		std::string name;
		ipc >> name;
		auto model = context->createObject<Model>(name);

		UINT32 meshcount;
		ipc >> meshcount;
		for (UINT32 i = 0; i < meshcount; ++i)
		{
			std::string meshname;
			ipc >> meshname;
			auto mesh = context->getObject<Mesh>(meshname);
			model->meshs.push_back(mesh);
		}
		
		ipc >> model->transform >> model->normTransform;
		UINT  numMaterials;
		ipc >> numMaterials;
		for (UINT i = 0; i < numMaterials; ++i)
		{
			std::string matname;
			ipc >> matname;
			model->materials.push_back(context->getObject<Material>(matname));
		}

		model->init();
		return true;
	};

	processors["createMaterial"] = [&ipc = mIPC]() {
		auto context = RenderContext::getSingleton();
		std::string name;
		ipc >> name;
		auto material = context->createObject<Material>(name);
		std::string vs, ps, pscontent;
		ipc >> vs >> ps >> pscontent;

		UINT32 count;
		ipc >> count;
		for (UINT32 i = 0; i < count; ++i)
		{
			std::string texname;
			ipc >>  texname;

			material->textures[texname] = context->getObject<Texture>(texname);
		}

		material->init(vs, ps, pscontent);
		material->applyTextures();
		return true;
	};

	processors["createCamera"] = [&ipc = mIPC]() {
		auto context = RenderContext::getSingleton();
		std::string name;
		ipc >> name;
		auto cam = context->createObject<Camera>(name);
		ipc >>cam->pos >> cam->dir >> cam->view >> cam->proj >> cam->viewport;
		return true;
	};

	processors["createLight"] = [&ipc = mIPC]() {
		auto context = RenderContext::getSingleton();
		std::string name;
		ipc >> name;
		auto light = context->createObject<Light>(name);
		ipc >> light->type >> light->color >> light->dir;
		return true;
	};


	processors["done"] = []() {
		return false;
	};

	processors["invalid"] = [&ipc = mIPC]() {
		ipc.invalid();
		return false;
	};

	while (true)
	{
		std::string cmd;
		mIPC >> cmd;
		auto p = processors.find(cmd);
	
		//Common::Assert(p != processors.end(), "unknonw command: " + cmd);
		if (p == processors.end())
			break;

		if (!p->second())
			break;
	}
}



