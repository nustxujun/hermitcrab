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
		UINT32 bytesofvertices, 
		UINT32 numvertices,
		UINT32 vertexStride,
		const void* indices,
		UINT32 bytesofindices,
		UINT32 numindices,
		UINT32 indexStride);

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