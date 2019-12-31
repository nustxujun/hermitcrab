#include "RenderCommand.h"
#include "RenderContext.h"
#include "D3DHelper.h"
#include <iostream>

RenderCommand* RenderCommand::instance = 0;

void RenderCommand::init(bool host )
{
	instance = new RenderCommand(host);
}

void RenderCommand::uninit()
{
	delete instance ;
	instance = nullptr;
}

RenderCommand * RenderCommand::getSingleton()
{
	return instance;
}

RenderCommand::RenderCommand(bool host)
{
	if (host)
		mIPC.listen("renderstation");
	else
		mIPC.connect("renderstation");
}

RenderCommand::~RenderCommand()
{
	mIPC.close();
}

void RenderCommand::done()
{
	mIPC << "done";
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
	UINT32 indexStride)
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


}

void RenderCommand::createTexture(const std::string & name, int width, int height, DXGI_FORMAT format, const void * data)
{
	mIPC << "createTexture";

	UINT32 size = D3DHelper::sizeof_DXGI_FORMAT(format) * width * height;
	mIPC << name << width << height << format << size;
	mIPC.send(data, size);
}

void RenderCommand::createModel(const std::string & name, const std::vector<std::string> meshs, const Matrix & transform, const std::string& materialName)
{
	mIPC << "createModel";
	mIPC << name;
	UINT32 size = meshs.size();
	mIPC << size;
	for (auto& m : meshs)
		mIPC << m;

	mIPC << transform << materialName;

}

void RenderCommand::createCamera(const std::string & name, const Matrix & view, const Matrix & proj, const D3D12_VIEWPORT & vp)
{
	mIPC << "createCamera" << name<< view << proj << vp;

}

void RenderCommand::createMaterial(const std::string & name, const std::string & vs, const std::string & ps, const std::map<std::string, Vector4>& consts, const std::map<std::string, std::string>& textures)
{
	mIPC << "createMaterial" << name << vs << ps;
	mIPC << (UINT32)consts.size();
	for (auto& c: consts)
		mIPC << c.first << c.second;

	mIPC << (UINT32)textures.size();
	for (auto& t: textures)
		mIPC << t.first << t.second;

}

void RenderCommand::createLight(const std::string& name, UINT32 type, const Color& color, const Matrix& transform)
{
	mIPC << "createLight" << name << type << color << transform;
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

		
		auto context = RenderContext::getSingleton();
		auto mesh = context->createObject<Mesh>(name);

		mesh->init(vertices,indices,verticesStride, indicesStride, numIndices);
		return true;
	};

	processors["createTexture"] = [&ipc = mIPC]() {
		std::vector<char> data;
		UINT32 width, height, size;
		DXGI_FORMAT fmt;
		std::string name;
		ipc >> name >>  width >> height >> fmt >> size;
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
		
		ipc >> model->transform;
		std::string matname;
		ipc >> matname;
		//model->material = context->getObject<Material>(matname);
		model->init(context->getObject<Material>(matname));
		context->addToRenderList(model);
		return true;
	};

	processors["createMaterial"] = [&ipc = mIPC]() {
		auto context = RenderContext::getSingleton();
		std::string name;
		ipc >> name;
		auto material = context->createObject<Material>(name);
		std::string vs, ps;
		ipc >> vs >> ps;

		UINT32 count;
		ipc >> count;
		for (UINT32 i = 0; i < count; ++i)
		{
			std::string pn;
			Vector4 p;
			ipc >> pn >> p;
			material->parameters[pn] = p;
		}

		ipc >> count;
		for (UINT32 i = 0; i < count; ++i)
		{
			std::string semantic, texname;
			ipc >> semantic >> texname;

			material->textures[semantic] = context->getObject<Texture>(texname);
		}

		material->init(vs, ps);
		return true;
	};

	processors["createCamera"] = [&ipc = mIPC]() {
		auto context = RenderContext::getSingleton();
		std::string name;
		ipc >> name;
		auto cam = context->createObject<Camera>(name);
		ipc >> cam->view >> cam->proj >> cam->viewport;

		return true;
	};

	processors["createLight"] = [&ipc = mIPC]() {
		auto context = RenderContext::getSingleton();
		std::string name;
		ipc >> name;
		auto light = context->createObject<Light>(name);
		ipc >> light->type >> light->color >> light->transform;
		return true;
	};


	processors["done"] = []() {
		return false;
	};

	while (true)
	{
		std::string cmd;
		mIPC >> cmd;
		auto p = processors.find(cmd);
	
		Common::Assert(p != processors.end(), "unknonw command: " + cmd);

		if (!p->second())
			break;
	}
}



