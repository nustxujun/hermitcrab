#include "Renderer.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#include <D3Dcompiler.h>



Renderer::Ptr Renderer::instance;

#define CHECK Common::checkResult

Renderer::Ptr Renderer::create()
{
	instance = Renderer::Ptr(new Renderer());
	return instance;
}

void Renderer::destory()
{
	instance.reset();
}

Renderer::Ptr Renderer::getSingleton()
{
	return instance;
}

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
	uninitialize();
}

void Renderer::initialize(HWND window)
{
	mWindow = window;

#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}
#endif

	initDevice();
	initCommands();
	initDescriptorHeap();
}

void Renderer::resize(int width, int height)
{
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = NUM_BACK_BUFFERS;
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

#if defined(D3D12ON7)

#else
		auto factory = getDXGIFactory();
		ComPtr<IDXGISwapChain1> swapChain;
		CHECK(factory->CreateSwapChainForHwnd(
			mCommandQueue.Get() ,
			mWindow,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain));

		swapChain.As(&mSwapChain);

#endif
	}
}

void Renderer::onRender()
{
	commitCommands();

	CHECK( mSwapChain->Present(1, 0) );

	resetCommands();
}

ID3D12Device* Renderer::getDevice()
{
	return mDevice.Get();
}

ID3D12CommandQueue* Renderer::getCommandQueue()
{
	return mCommandQueue.Get();
}

Renderer::Buffer Renderer::compileShader(const std::string & path, const std::string & entry, const std::string & target, const std::vector<D3D_SHADER_MACRO>& macros)
{
	struct _stat attrs;
	if (_stat(path.c_str(), &attrs) != 0)
	{
		Common::Assert(0, "fail to open shader file");
		return {};
	}

	auto time = attrs.st_mtime;

	std::regex p(".+[/\\]([^/\\]+)$");
	std::smatch match;
	std::string filename = path;
	if (std::regex_match(path, match, p))
	{
		filename = match[1];
	}

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif


	std::string cachefilename = "cache/" + filename;
	std::fstream cachefile(cachefilename, std::ios::in | std::ios::binary);

	if (cachefile)
	{
		__time64_t lasttime;
		cachefile >> lasttime;

		if (lasttime == time)
		{
			size_t pos = cachefile.tellg();
			cachefile.seekg(std::ios::end);
			size_t size = cachefile.tellg();
			size = size - pos;
			cachefile.seekg(pos);

			auto buffer = createBuffer(size);
			cachefile.read(buffer->data(), size);

			return buffer;
		}
	}
	

	std::fstream file(path, std::ios::in | std::ios::binary);
	file.seekg(std::ios::end);
	size_t size = file.tellg();
	file.seekg(std::ios::beg);
	std::vector<char> data(size);
	file.read(data.data(), size);
	file.close();

	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> err;
	if (FAILED(D3DCompile(data.data(), size, path.c_str(), macros.data(), NULL, entry.c_str(), target.c_str(), compileFlags, 0, &blob, &err)))
	{
		Common::Assert(0, (const char*)err->GetBufferPointer());
		return {};
	}

	auto result = createBuffer(blob->GetBufferSize());
	memcpy(result->data(), blob->GetBufferPointer(), result->size());
	return result;
}

void Renderer::addResourceBarrier(const D3D12_RESOURCE_BARRIER& resbarrier)
{
	mResourceBarriers.push_back(resbarrier);
}

void Renderer::flushResourceBarrier()
{
	mCommandList->ResourceBarrier(mResourceBarriers.size(), mResourceBarriers.data());
	mResourceBarriers.clear();
}

Renderer::Fence::Ref Renderer::createFence()
{
	auto fence = Fence::create();
	mFences.push_back(fence);
	return fence;
}

Renderer::Resource::Ref Renderer::createResource(size_t size, D3D12_HEAP_TYPE type)
{
	auto res = Resource::Ptr(new Resource());
	res->create(size, type);
	mResources.push_back(res);

	return res;
}

Renderer::Resource::Ref Renderer::createTexture(int width, int height, DXGI_FORMAT format, D3D12_HEAP_TYPE type)
{
	auto tex = Resource::Ptr(new Texture());
	tex->as<Texture>().create(width, height, type, format);
	
	mResources.push_back(tex);

	return tex;
}


void Renderer::uninitialize()
{
}

void Renderer::initDevice()
{
	auto adapter = getAdapter();
	CHECK(D3D12CreateDevice(adapter.Get(), FEATURE_LEVEL, IID_PPV_ARGS(&mDevice)));
}

void Renderer::initCommands()
{
	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		CHECK(mDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&mCommandQueue)));
	}

	mCurrentCommandAllocator = allocCommandAllocator();
	CHECK(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCurrentCommandAllocator->get(), nullptr, IID_PPV_ARGS(&mCommandList)));

	mCurrentFrame = 0;
}

void Renderer::initDescriptorHeap()
{
	auto create = [&](auto count, auto type, auto flags) {
		return DescriptorHeap::Ptr(new DescriptorHeap(count, type, flags));
	};

	mDescriptorHeaps[DHT_BACKBUFFER] = create(NUM_BACK_BUFFERS, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
	mDescriptorHeaps[DHT_RENDERTARGET] = create(NUM_MAX_RENDER_TARGET_VIEWS, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
	mDescriptorHeaps[DHT_DEPTHSTENCIL] = create(NUM_MAX_DEPTH_STENCIL_VIEWS, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
	mDescriptorHeaps[DHT_CBV_SRV_UAV] = create(NUM_MAX_CBV_SRV_UAVS, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
}

Renderer::CommandAllocator::Ref Renderer::allocCommandAllocator()
{
	for (auto& a : mCommandAllocators)
	{
		if (a->completed())
		{
			return a;
		}
	}

	auto a = CommandAllocator::Ptr(new CommandAllocator());
	mCommandAllocators.push_back(a);
	return a;
}

void Renderer::commitCommands()
{
	CHECK(mCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

void Renderer::resetCommands()
{
	mCurrentCommandAllocator->signal();
	mCurrentCommandAllocator = allocCommandAllocator();

	mCurrentCommandAllocator->reset();
	mCommandList->Reset(mCurrentCommandAllocator->get(), nullptr);

	mCurrentFrame = mSwapChain->GetCurrentBackBufferIndex();
}

void Renderer::syncFrame()
{
}

ComPtr<IDXGIFACTORY> Renderer::getDXGIFactory()
{
	UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	ComPtr<IDXGIFACTORY> fac;
#if defined(D3D12ON7)
	CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&fac)));
#else
	CHECK(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&fac)));
#endif
	return fac;
}

ComPtr<IDXGIAdapter> Renderer::getAdapter()
{
	ComPtr<IDXGIAdapter1> adapter;
	auto factory = getDXGIFactory();

	for (auto i = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, &adapter); ++i)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			return adapter;
		}
	}

	Common::Assert(false, "fail to find adapter");
	return {};
}

Renderer::DescriptorHeap::Ref Renderer::getDescriptorHeap(DescriptorHeapType type)
{
	return mDescriptorHeaps[type];
}
Renderer::DescriptorHeap::DescriptorHeap(UINT count, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
	auto device = Renderer::getSingleton()->getDevice();
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = count;
	desc.Type = type;
	desc.Flags = flags;
	device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap));
	mSize = device->GetDescriptorHandleIncrementSize(type);
	
	mUsed.resize(ALIGN(count, sizeof(int)),0);
}

UINT64 Renderer::DescriptorHeap::alloc()
{
	size_t stride = sizeof(int);
	for (size_t i = 0; i < mUsed.size(); ++i)
	{
		for (size_t j = 0; j < stride; ++j)
		{
			auto index = 1 << j;
			if ((mUsed[i] & index) == 0)
			{
				mUsed[i] |= index;
				return i * stride + j;
			}
		}
	}
	Common::Assert(0, " cannot alloc from descriptor heap");
	return {};
}

void Renderer::DescriptorHeap::dealloc(UINT64 pos)
{
	UINT64 stride = sizeof(int);
	UINT64 i = pos / stride;
	UINT64 j = pos % stride;

	mUsed[i] &= ~(1 << j);
}

ID3D12DescriptorHeap * Renderer::DescriptorHeap::get()
{
	return mHeap.Get();
}

Renderer::RenderTarget::RenderTarget(ComPtr<ID3D12Resource> res)
{
	auto device = Renderer::getSingleton()->getDevice();
	auto heap = Renderer::getSingleton()->getDescriptorHeap(DHT_RENDERTARGET);

	mPos = heap->alloc();
	mHandle = heap->get()->GetCPUDescriptorHandleForHeapStart();
	mHandle.ptr += mPos;
	device->CreateRenderTargetView(res.Get(), nullptr, mHandle);
}

Renderer::RenderTarget::~RenderTarget()
{
	auto heap = Renderer::getSingleton()->getDescriptorHeap(DHT_RENDERTARGET);
	heap->dealloc(mPos);
}

const D3D12_CPU_DESCRIPTOR_HANDLE& Renderer::RenderTarget::getHandle() const
{
	return mHandle;
}

Renderer::RenderTarget::operator const D3D12_CPU_DESCRIPTOR_HANDLE& () const
{
	return mHandle;
}

Renderer::CommandAllocator::CommandAllocator()
{
	mFence = Renderer::getSingleton()->createFence();
}

Renderer::CommandAllocator::~CommandAllocator()
{
	CloseHandle(mFenceEvent);
}

void Renderer::CommandAllocator::reset()
{
	wait();
	CHECK(mAllocator->Reset());
}

void Renderer::CommandAllocator::wait()
{
	mFence->wait();
}

void Renderer::CommandAllocator::signal()
{
	mFence->signal();
}

bool Renderer::CommandAllocator::completed()
{
	return mFence->completed();
}

ID3D12CommandAllocator * Renderer::CommandAllocator::get()
{
	return mAllocator.Get();
}

void Renderer::Resource::create(size_t size, D3D12_HEAP_TYPE heaptype, DXGI_FORMAT format)
{
	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Alignment = 0;
	resdesc.Width = size;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = format;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	create(resdesc, heaptype, D3D12_RESOURCE_STATE_COMMON);
}

void Renderer::Resource::create(const D3D12_RESOURCE_DESC& resdesc, D3D12_HEAP_TYPE ht, D3D12_RESOURCE_STATES ressate)
{
	mState = ressate;
	if (ht == D3D12_HEAP_TYPE_READBACK)
	{
		mState = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	else if (ht == D3D12_HEAP_TYPE_UPLOAD)
	{
		mState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	auto device = Renderer::getSingleton()->getDevice();
	device->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, mState, nullptr, IID_PPV_ARGS(&mResource));

	mDesc = resdesc;
}


void Renderer::Resource::blit(void* data, size_t size)
{
	D3D12_RANGE readrange = { 0,0 };
	char* dst = 0;
	CHECK(mResource->Map(0, &readrange, (void**)&dst));
	memcpy(dst, data, size);
	D3D12_RANGE writerange = { 0, size };
	mResource->Unmap(0, &writerange);
}

void Renderer::Resource::setState(D3D12_RESOURCE_STATES state)
{
	if (state == mState)
		return;

	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

	barrier.Transition.pResource = mResource.Get();
	barrier.Transition.StateBefore = mState;
	barrier.Transition.StateAfter = state;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	mState = state;
	Renderer::getSingleton()->addResourceBarrier(barrier);
}


void Renderer::Texture::create(size_t width, size_t height, D3D12_HEAP_TYPE ht, DXGI_FORMAT format)
{
	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Alignment = 0;
	resdesc.Width = width;
	resdesc.Height = height;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = format;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	Resource::create(resdesc, ht, D3D12_RESOURCE_STATE_COMMON);
}

Renderer::Fence::Fence()
{
	auto device = Renderer::getSingleton()->getDevice();
	CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
	mFenceValue = 0;
	mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

Renderer::Fence::~Fence()
{
}

void Renderer::Fence::wait()
{
	if (completed())
		return;
	CHECK(mFence->SetEventOnCompletion(mFenceValue, mFenceEvent));
	WaitForSingleObjectEx(mFenceEvent, INFINITY, FALSE);
}

void Renderer::Fence::signal()
{
	auto queue = Renderer::getSingleton()->getCommandQueue();
	CHECK(queue->Signal(mFence.Get(), ++mFenceValue));
}

bool Renderer::Fence::completed()
{
	return mFence->GetCompletedValue() < mFenceValue;
}
