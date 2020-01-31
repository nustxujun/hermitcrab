#pragma once

#include "Renderer.h"

class Quad
{
public:
	using Ptr = std::shared_ptr<Quad>;

	void init(const std::string& psname, const Renderer::RenderState& rs = Renderer::RenderState::Default);
	Renderer::PipelineState::Ref getPipelineState()const;

	void setResource(const std::string& name, D3D12_GPU_DESCRIPTOR_HANDLE handle);

	template<class T>
	void setVariable(const std::string& name, const T& val)
	{
		mPipelineState->setPSVariable(name, &val);
	}

	Renderer::Buffer::Ptr getSharedVertices()const;
	const D3D12_RECT& getRect()const {return mRect;}
	void setRect(const D3D12_RECT& r){mRect = r;}
	void fitToScreen();
private:
	D3D12_RECT mRect;
	Renderer::PipelineState::Ref mPipelineState;
	Renderer::ConstantBuffer::Ptr mConstant;
	Renderer::Buffer::Ptr mVertices;
};

