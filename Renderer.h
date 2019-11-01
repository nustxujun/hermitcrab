#pragma once
#include "Common.h"

#if defined(D3D12ON7)
#define IDXGIFACTORY IDXGIFactory1
#else
#define IDXGIFACTORY IDXGIFactory4
#endif

class Renderer
{
	static const auto FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0;
	static auto const NUM_BACK_BUFFERS = 3;
	static auto const NUM_MAX_RENDER_TARGET_VIEWS = 8 * 1024;
	static auto const NUM_MAX_DEPTH_STENCIL_VIEWS = 4 * 1024;
	static auto const NUM_MAX_CBV_SRV_UAVS = 32 * 1024;

	enum DescriptorHeapType
	{
		DHT_BACKBUFFER,
		DHT_RENDERTARGET,
		DHT_DEPTHSTENCIL,
		DHT_CBV_SRV_UAV,

		DHT_MAX_NUM
	};
public:
	using Ptr = std::shared_ptr<Renderer>;
	using Buffer = std::shared_ptr<std::vector<char>>;


	template<class T>
	class WeakPtr
	{
	public:
		WeakPtr()
		{

		}
		WeakPtr(std::weak_ptr<T> p):
			mPointer(p)
		{

		}

		operator bool()
		{
			return !mPointer.expired();
		}

		std::shared_ptr<T> operator->()
		{
			return mPointer.lock();
		}
	private:
		std::weak_ptr<T> mPointer;
	};

	template<class T>
	class Interface
	{
	public:
		using Ptr = std::shared_ptr<T>;
		using Ref = WeakPtr<T>;
	};


	class Resource: public Interface<Resource>
	{
	public:
		void create(size_t size, D3D12_HEAP_TYPE ht, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
		void create(const D3D12_RESOURCE_DESC& resdesc, D3D12_HEAP_TYPE ht, D3D12_RESOURCE_STATES ressate);
		void blit(void* data, size_t size);

		void setState(D3D12_RESOURCE_STATES state);

		template<class T>
		T& as() { return dynamic_cast<T>(*this); }
	private:
		ComPtr<ID3D12Resource> mResource;
		D3D12_RESOURCE_DESC mDesc;
		D3D12_RESOURCE_STATES mState;
	};

	class Texture : public Resource
	{
	public:
		virtual void create(size_t width, size_t height, D3D12_HEAP_TYPE ht, DXGI_FORMAT format);
	};

	class RenderTarget:public Interface<RenderTarget>
	{
	public:
		RenderTarget(ComPtr<ID3D12Resource> res);
		~RenderTarget();

		const D3D12_CPU_DESCRIPTOR_HANDLE& getHandle()const;
		operator const D3D12_CPU_DESCRIPTOR_HANDLE& ()const;
	private:
		UINT64 mPos;
		D3D12_CPU_DESCRIPTOR_HANDLE mHandle;
	};

	class DescriptorHeap :public Interface<DescriptorHeap>
	{
	public:

		DescriptorHeap(UINT count, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
		
		UINT64 alloc();
		void dealloc(UINT64 pos);

		ID3D12DescriptorHeap* get();
	private:
		ComPtr<ID3D12DescriptorHeap> mHeap;
		UINT mSize;
		std::vector<int> mUsed;
	};

	class CommandAllocator : public Interface<CommandAllocator>
	{
	public:
		CommandAllocator();
		~CommandAllocator();
		void reset();
		void wait();
		void signal(ComPtr<ID3D12CommandQueue> queue);
		bool completed();
		ID3D12CommandAllocator* get();

	private:
		ComPtr<ID3D12CommandAllocator> mAllocator;
		ComPtr<ID3D12Fence> mFence;
		HANDLE mFenceEvent;
		UINT64 mFenceValue;
	};


	static Renderer::Ptr create();
	static void destory();
	static Renderer::Ptr getSingleton();

	Renderer();
	~Renderer();
	void initialize(HWND window);
	void resize(int width, int height);
	void onRender();

	ComPtr<ID3D12Device> getDevice();
	Buffer compileShader(const std::string& path, const std::string& entry, const std::string& target, const std::vector<D3D_SHADER_MACRO>& macros = {});

	void addResourceBarrier(const D3D12_RESOURCE_BARRIER& resbarrier);
	void flushResourceBarrier();

	Resource::Ref createTexture(int width, int height, DXGI_FORMAT format, D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT);
	Resource::Ref createTexture(const std::string& filename);
private:
	Buffer createBuffer(size_t size = 0)
	{
		return Buffer(new std::vector<char>(size));
	}

	void uninitialize();
	void initDevice();
	void initCommands();
	void initDescriptorHeap();

	CommandAllocator::Ref allocCommandAllocator();
	void commitCommands();
	void resetCommands();
	void syncFrame();

	ComPtr<IDXGIFACTORY> getDXGIFactory();
	ComPtr<IDXGIAdapter> getAdapter();
	DescriptorHeap::Ref getDescriptorHeap(DescriptorHeapType);
private:
	static Renderer::Ptr instance;

	HWND mWindow;

	ComPtr<ID3D12Device> mDevice;
	ComPtr<IDXGISwapChain3> mSwapChain;
	std::array< RenderTarget::Ptr, NUM_BACK_BUFFERS> mBackbuffers;
	ComPtr<ID3D12CommandQueue> mCommandQueue;
	ComPtr<ID3D12GraphicsCommandList> mCommandList;
	std::vector<CommandAllocator::Ptr> mCommandAllocators;
	UINT mCurrentFrame;
	CommandAllocator::Ref mCurrentCommandAllocator;

	std::array<DescriptorHeap::Ptr, DHT_MAX_NUM> mDescriptorHeaps;
	std::vector<D3D12_RESOURCE_BARRIER> mResourceBarriers;
	std::vector<Resource::Ptr> mResources;
};