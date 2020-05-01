#pragma once

#include "Renderer.h"

class ResourceViewAllocator
{
public:
	static ResourceViewAllocator Singleton;

	Renderer::Resource::Ref alloc(UINT width, UINT height, UINT depth, DXGI_FORMAT format, Renderer::ViewType type);
	void recycle(Renderer::Resource::Ref res);

private:
	size_t hash(UINT width, UINT height, UINT depth, DXGI_FORMAT format, Renderer::ViewType type);

private:
	std::map<size_t, std::vector<Renderer::Resource::Ref>> mResources;
};