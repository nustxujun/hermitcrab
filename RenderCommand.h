#pragma once
#include "Common.h"
#include "SimpleIPC.h"

class RenderCommand
{
public:
	static void init(bool host = false);
	static void uninit();
	static RenderCommand* getSingleton();

	RenderCommand(bool host);
	~RenderCommand();

	void record();

	bool createMesh(
		const std::string& name, 
		const void* vertices, 
		size_t bytesofvertices, 
		size_t numvertices,
		size_t vertexStride,
		const void* indices,
		size_t bytesofindices,
		size_t numindices,
		size_t indexStride);

	bool createTexture(
		const std::string& name,
		int width, int height, 
		DXGI_FORMAT format,
		const void*data
		);

	bool createModel(
		const std::string& name,
		const std::vector<std::string> meshs,
		const Matrix& transform
	);
private:
	static RenderCommand* instance;

	SimpleIPC mIPC;
};