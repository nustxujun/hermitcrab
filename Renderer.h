#pragma once
#include "Common.h"

//#if WINVER < 0x0A00
#define D3D12ON7
//#endif

#if defined(D3D12ON7)
#include "D3D12Downlevel.h"
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

		WeakPtr(std::shared_ptr<T>& p) :
			mPointer(p)
		{

		}

		WeakPtr(std::weak_ptr<T>& p):
			mPointer(p)
		{

		}

		operator bool() const
		{
			return !mPointer.expired();
		}


		bool operator==(const WeakPtr<T>& p)const
		{
			if (mPointer.expired() || p.mPointer.expired())
				return false;
			return mPointer.lock().get() == p.mPointer.lock().get();
		}
		
		bool operator== (const std::shared_ptr<T>& p)const
		{
			if (mPointer.expired())
				return false;
			return mPointer.lock().get() == p.get();
		}

		std::shared_ptr<T> operator->()const
		{
			return mPointer.lock();
		}

	/*	T* get()const
		{
			return mPointer.lock()->get();
		}*/
	private:
		std::weak_ptr<T> mPointer;
	};

	template<class T>
	class Interface
	{
	public:
		using Ptr = std::shared_ptr<T>;
		using Ref = WeakPtr<T>;

		template<class ... Args>
		Ptr static create(Args& ... args)
		{
			auto ptr = Ptr(new T(args...));
			return ptr;
		}


		template<class U>
		U& to() { return static_cast<U&>(*this); }
	};


	class Resource: public Interface<Resource>
	{
	public:
		virtual ~Resource();
		void create(size_t size, D3D12_HEAP_TYPE ht, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
		void create(const D3D12_RESOURCE_DESC& resdesc, D3D12_HEAP_TYPE ht, D3D12_RESOURCE_STATES ressate);
		void blit(void* data, size_t size);

		const D3D12_RESOURCE_STATES& getState()const;
		// transition by cmdlist
		void setState(const D3D12_RESOURCE_STATES& s);


		ID3D12Resource* get()const{return mResource.Get();}
	private:
		ComPtr<ID3D12Resource> mResource;
		D3D12_RESOURCE_DESC mDesc;
		D3D12_RESOURCE_STATES mState;
	};

	class Texture final: public Resource
	{
	public:
		virtual void create(UINT width, UINT height, D3D12_HEAP_TYPE ht, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags );
	};

	class RenderTarget:public Interface<RenderTarget>
	{
	public:
		RenderTarget(UINT width, UINT height, DXGI_FORMAT format);
		~RenderTarget();

		const D3D12_CPU_DESCRIPTOR_HANDLE& getHandle()const;
		operator const D3D12_CPU_DESCRIPTOR_HANDLE& ()const;

		Resource::Ref getResource()const;
	private:
		D3D12_CPU_DESCRIPTOR_HANDLE mHandle;
		Resource::Ref mTexture;
	};

	class DescriptorHeap :public Interface<DescriptorHeap>
	{
	public:

		DescriptorHeap(UINT count, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
		
		D3D12_CPU_DESCRIPTOR_HANDLE allocCPUDescriptorHandle();
		D3D12_GPU_DESCRIPTOR_HANDLE allocGPUDescriptorHandle();

		void dealloc(D3D12_CPU_DESCRIPTOR_HANDLE handle);
		void dealloc(D3D12_GPU_DESCRIPTOR_HANDLE pos);


		ID3D12DescriptorHeap* get();
		UINT getSize()const{return mSize;}

	private:
		UINT64 alloc();

	private:
		ComPtr<ID3D12DescriptorHeap> mHeap;
		UINT mSize;
		std::vector<int> mUsed;

	};

	class Fence : public Interface<Fence>
	{
	public :
		Fence();
		~Fence();
		void wait();
		void signal();
		bool completed();

		private:
			ComPtr<ID3D12Fence> mFence;
			HANDLE mFenceEvent;
			UINT64 mFenceValue;
	};

	class CommandAllocator final: public Interface<CommandAllocator>
	{
	public:
		CommandAllocator();
		~CommandAllocator();
		void reset();
		void wait();
		void signal();
		bool completed();
		ID3D12CommandAllocator* get();

	private:
		ComPtr<ID3D12CommandAllocator> mAllocator;
		Fence::Ref mFence;
	};

	class CommandList final: public Interface<CommandList>
	{
	public:
		CommandList(const CommandAllocator::Ref& alloc);
		~CommandList();
		ID3D12GraphicsCommandList* get();


		void close();
		void reset(const CommandAllocator::Ref& alloc);

		void transitionTo(const Resource::Ref res, const D3D12_RESOURCE_STATES& state);
		void addResourceBarrier(const D3D12_RESOURCE_BARRIER& resbarrier);
		void flushResourceBarrier();

		void clearRenderTarget(const RenderTarget::Ref& rt, const Color& color);
	private:
		ComPtr<ID3D12GraphicsCommandList> mCmdList;
		std::vector<D3D12_RESOURCE_BARRIER> mResourceBarriers;
	};

	class PipelineState;
	class Shader
	{
		friend class Renderer::PipelineState;
	public:
		using Ptr = std::shared_ptr<Shader>;
		enum ShaderType {
			ST_VERTEX,
			ST_HULL,
			ST_DOMAIN,
			ST_GEOMETRY,
			ST_PIXEL,

			ST_COMPUTE,

			ST_MAX_NUM
		};
	public:
		Shader(const Buffer& data, ShaderType type);

		void registerSRV(UINT num, UINT start, UINT space);
		void registerUAV(UINT num, UINT start, UINT space);
		void registerCBV(UINT num, UINT start, UINT space);
		void registerSampler(UINT num, UINT start, UINT space);
		void registerStaticSampler(D3D12_STATIC_SAMPLER_DESC);
	private:
		ShaderType mType;
		Buffer mCodeBlob;

		std::vector<D3D12_DESCRIPTOR_RANGE> mRanges;
		std::vector< D3D12_STATIC_SAMPLER_DESC> mStaticSamplers;
	};

	class RenderState
	{
	public:
		friend class Renderer::PipelineState;
		static const RenderState Default;
	public:
		RenderState();
		RenderState(std::function<void(RenderState& self)> initializer);

		void setBlend(const D3D12_BLEND_DESC& bs);
		void setRasterizer(const D3D12_RASTERIZER_DESC& rs);
		void setDepthStencil(const D3D12_DEPTH_STENCIL_DESC& dss);
		void setInputLayout(const std::vector< D3D12_INPUT_ELEMENT_DESC> layout);
		void setPrimitiveType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type);
		void setRenderTargetFormat(const std::vector<DXGI_FORMAT>& fmts);
		void setDepthStencilFormat(DXGI_FORMAT fmt);
		void setSample(UINT count, UINT quality);
	private:
		D3D12_BLEND_DESC mBlend;
		D3D12_RASTERIZER_DESC mRasterizer;
		D3D12_DEPTH_STENCIL_DESC mDepthStencil;
		D3D12_PRIMITIVE_TOPOLOGY_TYPE mPrimitiveType;
		DXGI_FORMAT mDSFormat;
		DXGI_SAMPLE_DESC mSample;
		std::vector<DXGI_FORMAT> mRTFormats;
		std::vector<D3D12_INPUT_ELEMENT_DESC> mLayout;
	};

	class PipelineState final : public Interface<CommandList>
	{
	public:
		PipelineState(const RenderState& rs, const std::vector<Shader::Ptr>& shaders);
		~PipelineState();

		ID3D12PipelineState* get();

	private:
		ComPtr<ID3D12PipelineState> mPipelineState;
		ComPtr<ID3D12RootSignature> mRootSignature;
	};

	static Renderer::Ptr create();
	static void destory();
	static Renderer::Ptr getSingleton();

	Renderer();
	~Renderer();
	void initialize(HWND window);
	void resize(int width, int height);
	void onRender();

	ID3D12Device* getDevice();
	ID3D12CommandQueue* getCommandQueue();
	CommandList::Ref getCommandList();
	RenderTarget::Ref getBackBuffer();
	Shader::Ptr compileShader(const std::string& path, const std::string& entry, const std::string& target, const std::vector<D3D_SHADER_MACRO>& macros = {});

	Fence::Ref createFence();
	Resource::Ref createResource(size_t size, D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT);
	Resource::Ref createTexture(int width, int height, DXGI_FORMAT format, D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	void destroyResource(Resource::Ref res);
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

	void present();
private:
	static Renderer::Ptr instance;

	HWND mWindow;

	ComPtr<ID3D12Device> mDevice;
	ComPtr<IDXGISwapChain3> mSwapChain;
	ComPtr<ID3D12CommandQueue> mCommandQueue;
	CommandList::Ptr mCommandList;
	std::vector<CommandAllocator::Ptr> mCommandAllocators;
	UINT mCurrentFrame;
	CommandAllocator::Ref mCurrentCommandAllocator;


	std::array< RenderTarget::Ptr, NUM_BACK_BUFFERS> mBackbuffers;
	std::array<DescriptorHeap::Ptr, DHT_MAX_NUM> mDescriptorHeaps;
	std::vector<D3D12_RESOURCE_BARRIER> mResourceBarriers;
	std::vector<Resource::Ptr> mResources;
	std::vector<Fence::Ptr> mFences;
};