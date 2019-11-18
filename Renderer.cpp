#include "Renderer.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#include <D3Dcompiler.h>

#include <sstream>



Renderer::Ptr Renderer::instance;

//#define CHECK(hr) { if (hr == 0x887a0005) Common::checkResult(Renderer::getSingleton()->getDevice()->GetDeviceRemovedReason()); else Common::checkResult(hr);}
#define CHECK Common::checkResult

Renderer::Ptr Renderer::create()
{
	instance = Renderer::Ptr(new Renderer());
	return instance;
}

void Renderer::destory()
{
	instance->uninitialize();
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
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = NUM_BACK_BUFFERS;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

#if defined(D3D12ON7)
	
	ComPtr<ID3D12DeviceDownlevel> deviceDownlevel;
	CHECK(mDevice.As(&deviceDownlevel));

	mBackbuffers[0] = RenderTarget::create(width, height,swapChainDesc.Format);

#else
	if (mSwapChain)
	{
		mBackbuffers.fill({});
		CHECK(mSwapChain->ResizeBuffers(NUM_BACK_BUFFERS,width, height, swapChainDesc.Format,0));
	}
	else
	{
		auto factory = getDXGIFactory();
		ComPtr<IDXGISwapChain1> swapChain;
		CHECK(factory->CreateSwapChainForHwnd(
			mCommandQueue.Get(),
			mWindow,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain));

		swapChain.As(&mSwapChain);
	}

	for (auto i = 0; i < NUM_BACK_BUFFERS; ++i)
	{
		ComPtr<ID3D12Resource> buffer;
		CHECK(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&buffer)));
		auto res = Resource::create(buffer, D3D12_RESOURCE_STATE_PRESENT);
		mResources.push_back(res);
		mBackbuffers[i] = RenderTarget::create(res);
	}

	mCurrentFrame = mSwapChain->GetCurrentBackBufferIndex();

#endif
}

void Renderer::onRender()
{
	commitCommands();

	present();

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

Renderer::CommandList::Ref Renderer::getCommandList() 
{
	return mCommandList;
}

Renderer::RenderTarget::Ref Renderer::getBackBuffer()
{
	return mBackbuffers[mCurrentFrame];
}

Renderer::Shader::Ptr Renderer::compileShader(const std::string & path, const std::string & entry, const std::string & target, const std::vector<D3D_SHADER_MACRO>& macros)
{
	Shader::ShaderType type = Shader::ST_MAX_NUM;
	switch (target[0])
	{
	case 'v': type = Shader::ST_VERTEX; break;
	case 'p': type = Shader::ST_PIXEL; break;
	case 'c': type = Shader::ST_COMPUTE; break;
	default:
		Common::Assert(false, "unsupported!");
		break;
	}

	struct _stat attrs;
	if (_stat(path.c_str(), &attrs) != 0)
	{
		auto err = errno;
		Common::Assert(0, "fail to open shader file");
		return {};
	}

	auto time = attrs.st_mtime;

	std::regex p(std::string("^.+[/\\\\](.+\\..+)$"));
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
	std::hash<std::string> hash;
	std::stringstream ss;
	ss << std::hex << hash(path + entry + target);
	std::string cachefilename = "cache/" + ss.str();
	{
		std::fstream cachefile(cachefilename, std::ios::in | std::ios::binary);

		if (cachefile)
		{
			__time64_t lasttime;
			cachefile.read((char*)&lasttime, sizeof(lasttime));

			if (lasttime == time)
			{
				unsigned int size ;
				cachefile >> size;
				auto buffer = createBuffer(size);
				cachefile.read(buffer->data(), size);

				return Shader::Ptr(new Shader(buffer, type));

			}
		}
	}

	std::fstream file(path, std::ios::in | std::ios::binary);
	file.seekg(0,std::ios::end);
	size_t size = (size_t)file.tellg();
	file.seekg(0);
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

	{
		std::fstream cachefile(cachefilename, std::ios::out | std::ios::binary);
		cachefile.write((const char*)&time, sizeof(time));
		//cachefile << time;
		cachefile << (unsigned int)result->size();
		cachefile.write(result->data(), result->size());
	}

	return Shader::Ptr(new Shader(result, type));
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
	res->init(size, type);
	mResources.push_back(res);

	return res;
}

Renderer::Resource::Ref Renderer::createTexture(int width, int height, DXGI_FORMAT format, D3D12_HEAP_TYPE type,D3D12_RESOURCE_FLAGS flags)
{
	auto tex = Resource::Ptr(new Texture());
	tex->to<Texture>().init(width, height, type, format, flags);

	mResources.push_back(tex);

	return tex;
}

void Renderer::destroyResource(Resource::Ref res)
{
	auto endi = mResources.end();
	for (auto i = mResources.begin(); i != endi; ++i)
	{
		if (res == *i)
		{
			mResources.erase(i);
			return;
		}
	}
}



void Renderer::uninitialize()
{

	mBackbuffers.fill({});
	mResources.clear();

	mDescriptorHeaps.fill({});
	
	mResourceBarriers.clear();

	mCommandAllocators.clear();
	mCommandQueue.Reset();
	mCommandList.reset();

	mFences.clear();
	mSwapChain.Reset();
	auto device = mDevice.Detach();
	auto count = device->Release();
	if (count != 0)
	{
#ifndef D3D12ON7
		ID3D12DebugDevice* dd;
		device->QueryInterface(&dd);
		dd->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY);
		dd->Release();
#endif
		MessageBox(NULL, TEXT("some objects were not released."), NULL, NULL);
	}
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
	mCommandList = CommandList::create(mCurrentCommandAllocator);
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

	if (mCommandAllocators.size() >= 16)
		return mCommandAllocators[0];

	auto a = CommandAllocator::Ptr(new CommandAllocator());
	mCommandAllocators.push_back(a);
	return a;
}

void Renderer::commitCommands()
{
	//mCommandList->transitionTo(mBackbuffers[mCurrentFrame]->getResource(), D3D12_RESOURCE_STATE_PRESENT);

#ifndef D3D12ON7
	mCommandList->close();
	ID3D12CommandList* ppCommandLists[] = { mCommandList->get() };
	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
#endif
}

void Renderer::resetCommands()
{
	mCurrentCommandAllocator->signal();
	mCurrentCommandAllocator = allocCommandAllocator();

	mCurrentCommandAllocator->reset();

	mCommandList->reset(mCurrentCommandAllocator);

#ifndef D3D12ON7
	mCurrentFrame = mSwapChain->GetCurrentBackBufferIndex();
#endif

	mCommandList->transitionTo(mBackbuffers[mCurrentFrame]->getResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
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
void Renderer::present()
{
#if defined(D3D12ON7)
	ComPtr<ID3D12CommandQueueDownlevel> commandQueueDownlevel;

	CHECK(mCommandQueue.As(&commandQueueDownlevel));
	CHECK(commandQueueDownlevel->Present(
		mCommandList->get(),
		mBackbuffers[0]->getResource()->get(),
		mWindow,
		D3D12_DOWNLEVEL_PRESENT_FLAG_WAIT_FOR_VBLANK));
#else
	CHECK(mSwapChain->Present(1, 0));
#endif


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

	mUsed.resize(ALIGN(count, sizeof(int)), 0);

}

SIZE_T Renderer::DescriptorHeap::alloc()
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

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::DescriptorHeap::allocCPUDescriptorHandle()
{
	auto start = mHeap->GetCPUDescriptorHandleForHeapStart();
	start.ptr += alloc() * mSize;
	return start;
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::DescriptorHeap::allocGPUDescriptorHandle()
{
	auto start = mHeap->GetGPUDescriptorHandleForHeapStart();
	start.ptr += UINT64(alloc() * mSize);
	return start;
}

void Renderer::DescriptorHeap::dealloc(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	auto start = mHeap->GetCPUDescriptorHandleForHeapStart();
	auto pos = (handle.ptr - start.ptr) / mSize;
	int stride = sizeof(int);
	int i = pos / stride;
	int j = pos % stride;

	mUsed[i] &= ~(1 << j);
}

void Renderer::DescriptorHeap::dealloc(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
	auto start = mHeap->GetGPUDescriptorHandleForHeapStart();
	auto pos = (handle.ptr - start.ptr) / mSize;
	UINT64 stride = sizeof(int);
	int i = int(pos / stride);
	int j = int(pos % stride);

	mUsed[i] &= ~(1 << j);
}


ID3D12DescriptorHeap * Renderer::DescriptorHeap::get()
{
	return mHeap.Get();
}

Renderer::RenderTarget::RenderTarget(const Resource::Ref& res):
	mTexture(res)
{
	createView();
}

Renderer::RenderTarget::RenderTarget(UINT width, UINT height, DXGI_FORMAT format)
{
	auto renderer = Renderer::getSingleton();
	mTexture = renderer->createTexture(width, height, format, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	
	createView();
}

Renderer::RenderTarget::~RenderTarget()
{
	auto heap = Renderer::getSingleton()->getDescriptorHeap(DHT_RENDERTARGET);
	heap->dealloc(mHandle);

	Renderer::getSingleton()->destroyResource(mTexture);
}

const D3D12_CPU_DESCRIPTOR_HANDLE& Renderer::RenderTarget::getHandle() const
{
	return mHandle;
}

Renderer::RenderTarget::operator const D3D12_CPU_DESCRIPTOR_HANDLE& () const
{
	return mHandle;
}

Renderer::Resource::Ref Renderer::RenderTarget::getResource() const
{
	return mTexture;
}

void Renderer::RenderTarget::createView()
{
	auto renderer = Renderer::getSingleton();
	auto device = renderer->getDevice();
	auto heap = renderer->getDescriptorHeap(DHT_RENDERTARGET);
	mHandle = heap->allocCPUDescriptorHandle();
	auto res = mTexture->get();
	device->CreateRenderTargetView(res, nullptr, mHandle);
}


Renderer::CommandAllocator::CommandAllocator()
{
	mFence = Renderer::getSingleton()->createFence();
	auto device = Renderer::getSingleton()->getDevice();
	CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mAllocator)));
	
	static int i = 0;
	std::wstringstream ss;
	ss << i++;
	auto str = ss.str();
	mAllocator->SetName(L"aaa");
}

Renderer::CommandAllocator::~CommandAllocator()
{
	signal();
	wait();
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

Renderer::Resource::Resource(ComPtr<ID3D12Resource> res, D3D12_RESOURCE_STATES state):
	mResource(res), mState(state)
{
	mDesc = res->GetDesc();
}

Renderer::Resource::~Resource()
{
}

void Renderer::Resource::init(size_t size, D3D12_HEAP_TYPE heaptype, DXGI_FORMAT format)
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

	init(resdesc, heaptype, D3D12_RESOURCE_STATE_COMMON);
}

void Renderer::Resource::init(const D3D12_RESOURCE_DESC& resdesc, D3D12_HEAP_TYPE ht, D3D12_RESOURCE_STATES ressate)
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
	heapprop.Type = ht;

	auto device = Renderer::getSingleton()->getDevice();
	CHECK(device->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, mState, nullptr, IID_PPV_ARGS(&mResource)));

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

const D3D12_RESOURCE_STATES & Renderer::Resource::getState() const
{
	return mState;
}

void Renderer::Resource::setState(const D3D12_RESOURCE_STATES & s) 
{
	mState = s;
}

void Renderer::Texture::init(UINT width, UINT height, D3D12_HEAP_TYPE ht, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags)
{
	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resdesc.Alignment = 0;
	resdesc.Width = width;
	resdesc.Height = height;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = format;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resdesc.Flags = flags;

	Resource::init(resdesc, ht, D3D12_RESOURCE_STATE_COMMON);
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
	mFence.Reset();
}

void Renderer::Fence::wait()
{
	if (completed())
		return;
	CHECK(mFence->SetEventOnCompletion(mFenceValue, mFenceEvent));
	auto constexpr infinity = 0xffffffff; // same macros INFINITY in other headers
	auto ret = WaitForSingleObject(mFenceEvent, infinity);

	if (!completed())
		abort();
}

void Renderer::Fence::signal()
{
	auto queue = Renderer::getSingleton()->getCommandQueue();
	CHECK(queue->Signal(mFence.Get(), ++mFenceValue));
}

bool Renderer::Fence::completed()
{
	auto value = mFence->GetCompletedValue();
	return  value >= mFenceValue;
}

Renderer::CommandList::CommandList(const CommandAllocator::Ref & alloc)
{
	auto device = Renderer::getSingleton()->getDevice();
	CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc->get(), nullptr, IID_PPV_ARGS(&mCmdList)));

}

Renderer::CommandList::~CommandList()
{
}

void Renderer::CommandList::transitionTo(const Resource::Ref res, D3D12_RESOURCE_STATES  state)
{
	const auto& cur = res->getState();
	if (state == cur)
		return;

	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

	barrier.Transition.pResource = res->get();
	barrier.Transition.StateBefore = cur;
	barrier.Transition.StateAfter = state;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	res->setState(state);
	auto cmdlist = Renderer::getSingleton()->getCommandList();
	cmdlist->addResourceBarrier(barrier);
	cmdlist->flushResourceBarrier();
}

void Renderer::CommandList::addResourceBarrier(const D3D12_RESOURCE_BARRIER& resbarrier)
{
	mResourceBarriers.push_back(resbarrier);
}

void Renderer::CommandList::flushResourceBarrier()
{
	if (mResourceBarriers.empty())
		return;
	mCmdList->ResourceBarrier((UINT)mResourceBarriers.size(), mResourceBarriers.data());
	mResourceBarriers.clear();
}

void Renderer::CommandList::clearRenderTarget(const RenderTarget::Ref & rt, const Color & color)
{
	mCmdList->ClearRenderTargetView(rt->getHandle(), color.data(),0, nullptr);
}

void Renderer::CommandList::close()
{
	CHECK(mCmdList->Close());
}

void Renderer::CommandList::reset(const CommandAllocator::Ref& alloc)
{
	CHECK(mCmdList->Reset(alloc->get(), nullptr));
}

ID3D12GraphicsCommandList * Renderer::CommandList::get()
{
	return mCmdList.Get();
}


const Renderer::RenderState Default([](Renderer::RenderState& self) {
	{
		D3D12_BLEND_DESC desc = {};
		desc.RenderTarget[0] = {
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};
		self.setBlend(desc);
	}
	self.setDepthStencil({});
	self.setDepthStencilFormat(DXGI_FORMAT_D24_UNORM_S8_UINT);
	self.setPrimitiveType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	self.setRasterizer({
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_BACK,
		FALSE,
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,
		FALSE,
		FALSE,
		0,
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	});

	self.setRenderTargetFormat({ DXGI_FORMAT_R8G8B8A8_UNORM });
	self.setSample(1,0);
});


Renderer::RenderState::RenderState()
{
}

Renderer::RenderState::RenderState(std::function<void(RenderState&self)> initializer)
{
	initializer(*this);
}

Renderer::Shader::Shader(const Buffer& data, ShaderType type):
	mCodeBlob(data), mType(type)
{
}

Renderer::PipelineState::PipelineState(const RenderState & rs, const std::vector<Shader::Ptr>& shaders)
{
	auto device = Renderer::getSingleton()->getDevice();

	D3D12_ROOT_SIGNATURE_DESC rsd = {};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

	std::vector<D3D12_ROOT_PARAMETER> params;
	std::vector< D3D12_STATIC_SAMPLER_DESC> samplers;
	for (auto& s : shaders)
	{
		samplers.insert(samplers.end(), s->mStaticSamplers.begin(), s->mStaticSamplers.end());

		D3D12_ROOT_PARAMETER rpt = {};
		rpt.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rpt.DescriptorTable = {UINT(s->mRanges.size()), s->mRanges.data()};
		switch (s->mType)
		{
		case Shader::ST_VERTEX: {
			rsd.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; 
			desc.VS = {s->mCodeBlob->data(), (UINT)s->mCodeBlob->size()}; 
			rpt.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			break;}
		case Shader::ST_HULL:{
			desc.HS = { s->mCodeBlob->data(), (UINT)s->mCodeBlob->size()}; 
			rpt.ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;
			break;}
		case Shader::ST_DOMAIN:{
			desc.DS = { s->mCodeBlob->data(), (UINT)s->mCodeBlob->size() }; 
			rpt.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
			break;}
		case Shader::ST_GEOMETRY: {
			desc.GS = { s->mCodeBlob->data(), (UINT)s->mCodeBlob->size() }; 
			rpt.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
			break; }
		case Shader::ST_PIXEL: {
			desc.PS = { s->mCodeBlob->data(), (UINT)s->mCodeBlob->size() }; 
			rpt.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			break; }
		default:
			break;
		}

		params.push_back(rpt);
	}
	rsd.NumParameters = (UINT)params.size();
	rsd.pParameters = params.data();
	rsd.pStaticSamplers = samplers.data();

	ComPtr<ID3D10Blob> blob;
	ComPtr<ID3D10Blob> err;
	CHECK(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1,&blob,&err));
	device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature));
	

	desc.pRootSignature = mRootSignature.Get();
	
	desc.BlendState = rs.mBlend;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = rs.mRasterizer;
	desc.DepthStencilState = rs.mDepthStencil;
	desc.InputLayout = {  rs.mLayout.data(),(UINT)rs.mLayout.size() };
	desc.PrimitiveTopologyType = rs.mPrimitiveType;
	desc.NumRenderTargets = (UINT)rs.mRTFormats.size();
	memcpy(desc.RTVFormats, rs.mRTFormats.data(),desc.NumRenderTargets * sizeof(DXGI_FORMAT));
	desc.DSVFormat = rs.mDSFormat;
	
	
	CHECK(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineState)));
}
