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

	void createMesh(
		const std::string& name, 
		const void* vertices, 
		UINT32 bytesofvertices, 
		UINT32 numvertices,
		UINT32 vertexStride,
		const void* indices,
		UINT32 bytesofindices,
		UINT32 numindices,
		UINT32 indexStride);

	void createTexture(
		const std::string& name,
		int width, int height, 
		DXGI_FORMAT format,
		const void*data
		);

	void createModel(
		const std::string& name,
		const std::vector<std::string> meshs,
		const Matrix& transform,
		const std::string& materialName
	);

	void createCamera(
		const std::string& name,
		const Matrix& view,
		const Matrix& proj,
		const D3D12_VIEWPORT& vp);

	void createMaterial(
		const std::string& name,
		const std::string& vs,
		const std::string& ps,
		const std::map<std::string, Vector4>& consts,
		const std::map<std::string, std::string>& textures);

	void createLight(
		const std::string& name, 
		UINT32 type,
		const Color& color,
		const Matrix& transform);

	void done();
private:
	static RenderCommand* instance;

	SimpleIPC mIPC;
};