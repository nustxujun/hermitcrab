#pragma once
#include "Common.h"
#include "SimpleIPC.h"

class RenderCommand
{
public:
	RenderCommand();
	~RenderCommand();
	void init(bool host);
	void record();

	struct SubMesh
	{
		UINT materialIndex;
		UINT startIndex;
		UINT numIndices;
	};

	void createMesh(
		const std::string& name, 
		const void* vertices, 
		UINT32 bytesofvertices, 
		UINT32 numvertices,
		UINT32 vertexStride,
		const void* indices,
		UINT32 bytesofindices,
		UINT32 numindices,
		UINT32 indexStride,
		const std::vector<SubMesh>& submeshs);

	void createTexture(
		const std::string& name,
		int width, int height, 
		DXGI_FORMAT format,
		bool srgb,
		const void* data
		);

	void createModel(
		const std::string& name,
		const std::vector<std::string> meshs,
		const Matrix& transform,
		const Matrix& normaltransform,
		const std::vector<std::string>& materialNames
	);

	void createCamera(
		const std::string& name,
		const Vector3& pos,
		const Vector3& dir,
		const Matrix& view,
		const Matrix& proj,
		const D3D12_VIEWPORT& vp);

	void createMaterial(
		const std::string& name,
		const std::string& vs,
		const std::string& ps,
		const std::string& pscontent,
		const std::set<std::string>& textures);

	void createLight(
		const std::string& name, 
		UINT32 type,
		const Color& color,
		const Vector3& dir);

	void createReflectionProbe(
		const std::string& name,
		const Matrix& transform,
		float influence,
		float brightness,
		UINT cubesize,
		const void* data,
		UINT size
	);

	void createSky(
		const std::string& name, 
		const std::string& mesh,
		const std::string& material ,
		const Matrix& transform
	);

	void done();
	void invalid();
	void invalidSelf();
private:
	static RenderCommand* instance;

	SimpleIPC mIPC;
};