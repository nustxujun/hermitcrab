#pragma once
#include "Common.h"

#if _MSC_VER < 1920
#define D3D12ON7
#endif

#if defined(D3D12ON7)
#include "D3D12Downlevel.h"
#define IDXGIFACTORY IDXGIFactory1
#else
#define IDXGIFACTORY IDXGIFactory4
#endif

class Renderer
{
	static const auto FEATURE_LEVEL = D3D_FEATURE_LEVEL_12_0;
	static auto const NUM_BACK_BUFFERS = 2;
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

		WeakPtr(const std::shared_ptr<T>& p) :
			mPointer(p)
		{
		}

		//WeakPtr(const std::weak_ptr<T>& p):
		//	mPointer(p)
		//{
		//}

		WeakPtr(WeakPtr&& p) noexcept :
			mPointer(std::move(p.mPointer))
		{
		}

		WeakPtr(const WeakPtr& p) :
			mPointer(p.mPointer)
		{
		}
		

		template<class U>
		WeakPtr(const WeakPtr<U>& p):
			mPointer(p.weak())
		{
		}

		void operator=(const WeakPtr& p) 
		{
			mPointer = p.mPointer;
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
			Common::Assert(!mPointer.expired(), "invalid pointer");
			return mPointer.lock();
		}


		std::weak_ptr<T> weak()const {
			return mPointer;
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

		template<class ... Args>
		Ptr static create(Args ... args)
		{
			auto ptr = Ptr(new T(args...));
			return ptr;
		}


		template<class U>
		U& to() { return static_cast<U&>(*this); }
	};

	class DescriptorHandle
	{
	public:
		D3D12_CPU_DESCRIPTOR_HANDLE cpu;
		D3D12_GPU_DESCRIPTOR_HANDLE gpu;
		UINT64 pos;

		DescriptorHandle() = default;
		DescriptorHandle(UINT64 alloc, D3D12_CPU_DESCRIPTOR_HANDLE c, D3D12_GPU_DESCRIPTOR_HANDLE g):
			cpu(c),gpu(g), pos(alloc)
		{
		}


		operator const D3D12_CPU_DESCRIPTOR_HANDLE& ()const
		{
			return cpu;
		}

		operator const D3D12_GPU_DESCRIPTOR_HANDLE& ()const
		{
			return gpu;
		}

	private:

	};

	class Resource: public Interface<Resource>
	{
	public:
		Resource() = default;
		virtual ~Resource();
		Resource(ComPtr<ID3D12Resource> res, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);
		void init(UINT64 size, D3D12_HEAP_TYPE ht, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
		void init(const D3D12_RESOURCE_DESC& resdesc, D3D12_HEAP_TYPE ht, D3D12_RESOURCE_STATES ressate);
		void blit(const void* data, UINT64 size);
		char* map(UINT sub);
		void unmap(UINT sub);
		

		const D3D12_RESOURCE_STATES& getState()const;
		// transition by cmdlist
		void setState(const D3D12_RESOURCE_STATES& s);


		ID3D12Resource* get()const{return mResource.Get();}
		const D3D12_RESOURCE_DESC& getDesc()const{return mDesc;}
	private:
		ComPtr<ID3D12Resource> mResource;
		D3D12_RESOURCE_DESC mDesc;
		D3D12_RESOURCE_STATES mState;
	};

	class Texture final: public Resource , public Interface<Texture>
	{
	public:
		using Ptr = Interface<Texture>::Ptr;
		using Ref = Interface<Texture>::Ref;

		template<class ... Args>
		static Ptr create(Args ... args) {
			return Ptr(new Texture(args ...));
		}

		Texture() = default;
		Texture(ComPtr<ID3D12Resource> res, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON):Resource(res, state){}

		virtual void init(UINT width, UINT height, D3D12_HEAP_TYPE ht, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags );

		const DescriptorHandle& getHandle();
	private:
		void createView();
	private:
		DescriptorHandle mHandle;
	};

	class RenderTarget final:public Interface<RenderTarget>
	{
	public:
		RenderTarget(const Texture::Ref& res);
		RenderTarget(UINT width, UINT height, DXGI_FORMAT format);
		~RenderTarget();

		const DescriptorHandle& getHandle()const;
		operator const DescriptorHandle& ()const;

		Texture::Ref getTexture()const;
	private:
		void createView();
	private:
		DescriptorHandle mHandle;
		Texture::Ref mTexture;
	};

	class VertexBuffer final :public Interface<VertexBuffer>
	{
	public:
		VertexBuffer(UINT size, UINT stride, D3D12_HEAP_TYPE type);
		const D3D12_VERTEX_BUFFER_VIEW& getView()const{return mView;}

		Resource::Ref getResource()const{return mResource;}
		void blit(const void* buffer, size_t size);
	private:
		Resource::Ref mResource;
		D3D12_VERTEX_BUFFER_VIEW mView;
	};

	class DescriptorHeap :public Interface<DescriptorHeap>
	{
	public:

		DescriptorHeap(UINT count, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
		
		DescriptorHandle alloc();
		void dealloc(DescriptorHandle& handle);



		ID3D12DescriptorHeap* get();
		SIZE_T getSize()const{return mSize;}

	private:
		void dealloc(UINT64 pos);
		UINT64 allocHeap();
	private:
		ComPtr<ID3D12DescriptorHeap> mHeap;
		SIZE_T mSize;
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
		Fence::Ptr mFence;
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
		void registerStaticSampler(const D3D12_STATIC_SAMPLER_DESC& desc);
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
		RenderState(std::function<void(RenderState& self)> initializer);

		void setBlend(const D3D12_BLEND_DESC& bs){mBlend = bs;};
		void setRasterizer(const D3D12_RASTERIZER_DESC& rs){mRasterizer = rs;}
		void setDepthStencil(const D3D12_DEPTH_STENCIL_DESC& dss){mDepthStencil = dss;};
		void setInputLayout(const std::vector< D3D12_INPUT_ELEMENT_DESC> layout){mLayout = layout;};
		void setPrimitiveType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) {mPrimitiveType = type;}
		void setRenderTargetFormat(const std::vector<DXGI_FORMAT>& fmts){mRTFormats = fmts;}
		void setDepthStencilFormat(DXGI_FORMAT fmt) {mDSFormat = fmt;}
		void setSample(UINT count, UINT quality){mSample = {count, quality};}
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

	class PipelineState final : public Interface<PipelineState>
	{
	public:
		PipelineState(const RenderState& rs, const std::vector<Shader::Ptr>& shaders);
		~PipelineState();

		ID3D12PipelineState* get(){return mPipelineState.Get();}
		ID3D12RootSignature* getRootSignature(){return mRootSignature.Get();}
	private:
		ComPtr<ID3D12PipelineState> mPipelineState;
		ComPtr<ID3D12RootSignature> mRootSignature;
	};



	class CommandList final : public Interface<CommandList>
	{
	public:
		CommandList(const CommandAllocator::Ref& alloc);
		~CommandList();
		ID3D12GraphicsCommandList* get();


		void close();
		void reset(const CommandAllocator::Ref& alloc);

		void transitionTo( Resource::Ref res, D3D12_RESOURCE_STATES state);
		void addResourceBarrier(const D3D12_RESOURCE_BARRIER& resbarrier);
		void flushResourceBarrier();
		void copyBuffer(Resource::Ref dst, UINT dstStart, Resource::Ref src, UINT srcStart, UINT64 size );
		void copyTexture(Resource::Ref dst, UINT dstSub, const std::array<UINT, 3>& dstStart, Resource::Ref src, UINT srcSub, const std::pair<std::array<UINT,3>, std::array<UINT, 3>>* srcBox );

		void clearRenderTarget(const RenderTarget::Ref& rt, const Color& color);

		void setViewport(const D3D12_VIEWPORT& vp);
		void setScissorRect(const D3D12_RECT& rect);
		void setRenderTarget(const RenderTarget::Ref& rt);
		void setPipelineState(PipelineState::Ref ps);
		void setVertexBuffer(const std::vector<VertexBuffer::Ptr>& vertices);
		void setVertexBuffer(const VertexBuffer::Ptr& vertices);
		void setPrimitiveType(D3D_PRIMITIVE_TOPOLOGY type = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		void setTexture(UINT slot, Texture::Ref tex );
		void drawInstanced(UINT vertexCount, UINT instanceCount = 1, UINT startVertex = 0, UINT startInstance = 0);
	private:
		ComPtr<ID3D12GraphicsCommandList> mCmdList;
		std::vector<D3D12_RESOURCE_BARRIER> mResourceBarriers;
	};



	static Renderer::Ptr create();
	static void destory();
	static Renderer::Ptr getSingleton();

	Renderer();
	~Renderer();
	void initialize(HWND window);
	void resize(int width, int height);
	void beginFrame();
	void endFrame();

	ID3D12Device* getDevice();
	ID3D12CommandQueue* getCommandQueue();
	CommandList::Ref getCommandList();
	RenderTarget::Ref getBackBuffer();
	void flushCommandQueue();
	void updateResource(Resource::Ref res, const void* buffer, UINT64 size, const std::function<void(CommandList::Ref, Resource::Ref )>& copy);
	//void updateBuffer(Resource::Ref res, const void* buffer, size_t size);
	//void updateTexture(Resource::Ref res,const void* buffer, size_t numRows, size_t rowSize);

	Shader::Ptr compileShader(const std::string& path, const std::string& entry, const std::string& target, const std::vector<D3D_SHADER_MACRO>& macros = {});
	Fence::Ptr createFence();
	Resource::Ref createResource(size_t size, D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT);
	void destroyResource(Resource::Ref res);
	Texture::Ref createTexture(int width, int height, DXGI_FORMAT format, D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	Texture::Ref createTexture(const std::string& filename);
	VertexBuffer::Ptr createVertexBuffer(UINT size, UINT stride, D3D12_HEAP_TYPE type, const void* data = nullptr, size_t count = 0);
	PipelineState::Ref createPipelineState(const std::vector<Shader::Ptr>& shaders, const RenderState& rs);

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
	Fence::Ptr mQueueFence;
	CommandList::Ptr mCommandList;
	CommandList::Ptr mResourceCommandList;

	std::vector<CommandAllocator::Ptr> mCommandAllocators;
	UINT mCurrentFrame;
	CommandAllocator::Ref mCurrentCommandAllocator;


	std::array< RenderTarget::Ptr, NUM_BACK_BUFFERS> mBackbuffers;
	std::array<DescriptorHeap::Ptr, DHT_MAX_NUM> mDescriptorHeaps;
	std::vector<D3D12_RESOURCE_BARRIER> mResourceBarriers;
	std::vector<Resource::Ptr> mResources;
	std::unordered_map<std::string, Texture::Ref> mTextureMap;
	std::vector<PipelineState::Ptr> mPipelineStates;
};