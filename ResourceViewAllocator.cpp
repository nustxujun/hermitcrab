#include "ResourceViewAllocator.h"
#include "D3DHelper.h"

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

		Renderer::Resource::Ref res;
		if (depth == 1)
			res = Renderer::getSingleton()->createTexture2DBase(width, height, depth, format,1,D3D12_HEAP_TYPE_DEFAULT, flags);
		else
			res = Renderer::getSingleton()->createTexture3D(width, height, depth, format, 1, flags, D3D12_HEAP_TYPE_DEFAULT);
		switch (type)
		{
		case Renderer::VT_RENDERTARGET: 
			{
				res->createRenderTargetView(NULL); 
				res->createShaderResource(NULL); 
				break;
			}
		case Renderer::VT_DEPTHSTENCIL: 
			{
				auto [srvfmt, dsvfmt] = D3DHelper::matchReadableDepthFormat(format);
				{
					D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
					desc.Format = dsvfmt;
					desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
					desc.Flags = D3D12_DSV_FLAG_NONE;
					desc.Texture2D.MipSlice = 0;
					res->createDepthStencilView(&desc);
				}
			
				if (srvfmt != DXGI_FORMAT_UNKNOWN)
				{
					D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
					desc.Format = srvfmt;
					desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					desc.Texture2D.MostDetailedMip = 0;
					desc.Texture2D.MipLevels = -1;
					res->createShaderResource(&desc);
				}
			}
			break;
		case Renderer::VT_UNORDEREDACCESS: 
			{
				res->createUnorderedAccessView(NULL); 
				res->createShaderResource(NULL);
				break;
			}
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
