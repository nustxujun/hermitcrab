#pragma once

#include "Renderer.h"

class Quad
{
public:
	void init(const std::string& psname);
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

};

