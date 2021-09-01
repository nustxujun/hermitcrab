#pragma once

#include "Renderer.h"

class ResourceViewAllocator
{
public:
	static ResourceViewAllocator Singleton;

	std::pair<Renderer::Resource::Ref, size_t> alloc(UINT width, UINT height, UINT depth, DXGI_FORMAT format, Renderer::ViewType type, Renderer::ClearValue cv);
	void recycle(Renderer::Resource::Ref res, size_t hashvalue = 0);

private:
	size_t hash(UINT width, UINT height, UINT depth, DXGI_FORMAT format, Renderer::ViewType type, Renderer::ClearValue cv);

private:
	std::map<size_t, std::vector<Renderer::Resource::Ref>> mResources;
};