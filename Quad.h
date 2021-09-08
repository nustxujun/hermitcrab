#pragma once

#include "Renderer.h"

class Quad
{
public:
	using Ptr = std::shared_ptr<Quad>;

	void init(const std::string& psname, const Renderer::RenderState& rs = Renderer::RenderState::Default);
	void init(Renderer::Shader::Ptr ps, const Renderer::RenderState& rs = Renderer::RenderState::Default);


	//void setResource(const std::string& name, D3D12_GPU_DESCRIPTOR_HANDLE handle);

	//template<class T>
	//void setVariable(const std::string& name, const T& val)
	//{
	//	mPipelineState->setPSVariable(name, &val);
	//}
	//void setConstants(const std::string& name, Renderer::ConstantBuffer::Ptr c);

	const D3D12_RECT& getRect()const {return mRect;}
	void setRect(const D3D12_RECT& r){mRect = r;}
	void fitToScreen();

	void draw(Renderer::CommandList *& cmdlist)const;
private:
	D3D12_RECT mRect;
	Renderer::PipelineStateInstance::Ptr mPipelineState;
	std::map<std::string, Renderer::ConstantBuffer::Ptr> mConstants;
};

