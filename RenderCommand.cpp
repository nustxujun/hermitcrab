#include "RenderCommand.h"
#include "RenderContext.h"
#include "D3DHelper.h"


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

bool RenderCommand::createMesh(
	const std::string & name, 
	const void * vertices, 
	size_t bytesofvertices, 
	size_t numvertices, 
	size_t vertexStride,
	const void * indices, 
	size_t bytesofindices, 
	size_t numindices,
	size_t indexStride)
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

	bool ret;
	mIPC >> ret;
	return ret;
}

bool RenderCommand::createTexture(const std::string & name, int width, int height, DXGI_FORMAT format, const void * data)
{
	mIPC << "createTexture";

	size_t size = D3DHelper::sizeof_DXGI_FORMAT(format) * width * height;
	mIPC << name << width << height << format << size;
	mIPC.send(data, size);

	bool ret;
	mIPC >> ret;
	return ret;
}

bool RenderCommand::createModel(const std::string & name, const std::vector<std::string> meshs, const Matrix & transform)
{
	mIPC << "createModel";
	auto size = meshs.size();
	mIPC << size;
	for (auto& m : meshs)
		mIPC << m;

	mIPC << transform;
	bool ret;
	mIPC >> ret;
	return ret;
}


void RenderCommand::record()
{
	std::map<std::string, std::function<bool(void)>> processors;

	processors["createMesh"] = [&ipc = mIPC]() {
		size_t numBytesVertices, numBytesIndices, numVertices, numIndices, verticesStride, indicesStride;
		std::vector<char> vertices, indices;
		
		std::string name;
		ipc >> name;

		ipc >> numBytesVertices >> numVertices >> verticesStride;
		vertices.resize(numBytesVertices);
		ipc.receive(vertices.data(), numBytesVertices);

		ipc >> numBytesIndices >> numIndices >> indicesStride;
		vertices.resize(numBytesIndices);
		ipc.receive(indices.data(), numBytesIndices);

		std::string texname;
		ipc >> texname;
		
		auto context = RenderContext::getSingleton();
		auto texture = context->getObject<Texture>(texname);
		auto mesh = context->createObject<Mesh>(name);
		return true;
	};

	processors["createTexture"] = [&ipc = mIPC]() {
		std::vector<char> data;
		size_t width, height, size;
		DXGI_FORMAT fmt;
		std::string name;
		ipc >> name >> ipc >> width >> height >> fmt >> size;
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

		size_t meshcount;
		ipc >> meshcount;
		for (size_t i = 0; i < meshcount; ++i)
		{
			std::string meshname;
			ipc >> meshname;
			auto mesh = context->getObject<Mesh>(meshname);
			model->meshs.push_back(mesh);
		}
		
		ipc >> model->transform;

		return true;
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



