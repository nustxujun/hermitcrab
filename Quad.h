#pragma once

#include "Renderer.h"

class Quad
{
public:
	using Ptr = std::shared_ptr<Quad>;

	void init(const std::string& psname, const Renderer::RenderState& rs = Renderer::RenderState::Default);
	Renderer::PipelineState::Ref getPipelineState()const;

	void setResource(const std::string& name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
	void setResource(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);

	template<class T>
	void setVariable(const std::string& name, const T& val)
	{
		mConstant->setVariable(name, &val);
	}

	Renderer::Buffer::Ptr getSharedVertices()const;
private:
	Renderer::PipelineState::Ref mPipelineState;
	Renderer::ConstantBuffer::Ptr mConstant;
	Renderer::Buffer::Ptr mVertices;
};

