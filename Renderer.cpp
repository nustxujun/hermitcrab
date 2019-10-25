#include "Renderer.h"

Renderer::Ptr instance;

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

	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}

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
	}
}

void Renderer::onRender()
{
	commitCommands();

	CHECK( mSwapChain->Present(1, 0) );

	syncFrame();
	resetCommands();
}

ComPtr<ID3D12Device> Renderer::getDevice()
{
	return mDevice;
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

	for (auto i = 0; i < NUM_BACK_BUFFERS; ++i)
		CHECK(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocators[i])));

	CHECK(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&mCommandList)));
	CHECK(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
	mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	
	mFenceValue = 0;
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

void Renderer::commitCommands()
{
	CHECK(mCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

void Renderer::resetCommands()
{
	CHECK(mCommandAllocators[mCurrentFrame]->Reset());
	mCommandList->Reset(mCommandAllocators[mCurrentFrame].Get(), nullptr);
}

void Renderer::syncFrame()
{
	mCurrentFrame = mSwapChain->GetCurrentBackBufferIndex();
	auto fence = mFenceValue;
	mFenceValue++;

	CHECK(mCommandQueue->Signal(mFence.Get(), fence));
	if (mFence->GetCompletedValue() < fence)
	{
		mFence->SetEventOnCompletion(fence, mFenceEvent);
		WaitForSingleObjectEx(mFenceEvent, INFINITE,FALSE);
	}

}

ComPtr<IDXGIFactory4> Renderer::getDXGIFactory()
{
	UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	ComPtr<IDXGIFactory4> fac;
	CHECK(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&fac)));
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
	auto stride = sizeof(int);
	for (auto i = 0; i < mUsed.size(); ++i)
	{
		for (auto j = 0; j < stride; ++j)
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
}

void Renderer::DescriptorHeap::dealloc(UINT64 pos)
{
	auto stride = sizeof(int);
	auto i = pos / stride;
	auto j = pos % stride;

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
