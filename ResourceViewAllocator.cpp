#include "ResourceViewAllocator.h"

ResourceViewAllocator ResourceViewAllocator::Singleton;

std::pair<Renderer::Resource::Ref, size_t> ResourceViewAllocator::alloc(UINT width, UINT height, UINT depth, DXGI_FORMAT format, Renderer::ViewType type)
{
	auto hv = hash(width, height, depth, format, type);
	auto& stack = mResources[hv];
	if (stack.empty())
	{
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

		switch (type)
		{
		case Renderer::VT_RENDERTARGET: flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; break;
		case Renderer::VT_DEPTHSTENCIL: flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; break;
		case Renderer::VT_UNORDEREDACCESS: flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; break;
		}

		auto res = Renderer::getSingleton()->createTexture(width, height, depth, format,1,D3D12_HEAP_TYPE_DEFAULT, flags);
		switch (type)
		{
		case Renderer::VT_RENDERTARGET: res->createRenderTargetView(NULL); res->createTexture2D();break;
		case Renderer::VT_DEPTHSTENCIL: res->createDepthStencilView(NULL); break;
		case Renderer::VT_UNORDEREDACCESS: res->createUnorderedAccessView(NULL); res->createTexture2D(); break;
		}

		return {res, hv};
	}

	auto res = stack.back();
	stack.pop_back();
	return {res, hv};
}

void ResourceViewAllocator::recycle(Renderer::Resource::Ref res, size_t hashvalue)
{
	auto& desc = res->getDesc();
	auto hv = hashvalue ? hashvalue : hash(desc.Width, desc.Height, desc.DepthOrArraySize, desc.Format, res->getViewType());
	mResources[hv].emplace_back(std::move(res));
}

size_t ResourceViewAllocator::hash(UINT width, UINT height, UINT depth, DXGI_FORMAT format, Renderer::ViewType type)
{
	auto cal = [=](){
		size_t value = 0;

		auto combine = [&](auto v){
			value ^= std::hash<decltype(v)>{}(v)+ 0x9e3779b9 + (value << 6) + (value >> 2);
		};

		combine(width);
		combine(height) ;
		combine(depth);
		combine(format);
		combine(type) ;

		return value;
	};
	return cal();
}
