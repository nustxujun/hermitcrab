#pragma once
#include "Common.h"


//#if WINVER  < _WIN32_WINNT_WIN10
//	#define D3D12ON7
//	#include "D3D12Downlevel.h"
//#endif


class Renderer
{
#if defined(D3D12ON7)
	using IDXGIFACTORY = IDXGIFactory1;

	using ShaderReflection =	ID3D11ShaderReflection;
	using ShaderDesc =			D3D11_SHADER_DESC;
	using ShaderInputBindDesc = D3D11_SHADER_INPUT_BIND_DESC;
	using ShaderBufferDesc =	D3D11_SHADER_BUFFER_DESC;
	using ShaderVariableDesc =	D3D11_SHADER_VARIABLE_DESC;
#else
	using IDXGIFACTORY = IDXGIFactory4;

	using ShaderReflection =	ID3D12ShaderReflection;
	using ShaderDesc =			D3D12_SHADER_DESC;
	using ShaderInputBindDesc = D3D12_SHADER_INPUT_BIND_DESC;
	using ShaderBufferDesc =	D3D12_SHADER_BUFFER_DESC;
	using ShaderVariableDesc =	D3D12_SHADER_VARIABLE_DESC;
#endif



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
	using MemoryData = std::shared_ptr<std::vector<char>>;

	enum ViewType
	{
		VT_UNKNOWN,

		VT_RENDERTARGET,
		VT_DEPTHSTENCIL,
		VT_UNORDEREDACCESS,
	};
public:



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

		std::shared_ptr<T> shared() const{
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

	class CommandList;
	class Resource: public Interface<Resource>
	{
		friend class Renderer::CommandList;
	public:
		enum ResourceType
		{
			RT_PERSISTENT,
			RT_TRANSIENT,
		};

		Resource(ResourceType type = RT_PERSISTENT);
		Resource(ComPtr<ID3D12Resource> res, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);
		virtual ~Resource();
		virtual void init(UINT64 size, D3D12_HEAP_TYPE ht, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
		virtual void init(const D3D12_RESOURCE_DESC& resdesc, D3D12_HEAP_TYPE ht, D3D12_RESOURCE_STATES ressate);
		virtual void blit(const void* data, UINT64 size);
		char* map(UINT sub);
		void unmap(UINT sub);
		

		const D3D12_RESOURCE_STATES& getState()const;
		// transition by cmdlist
		//void setState(const D3D12_RESOURCE_STATES& s);
		void setName(const std::wstring& name);

		ID3D12Resource* get()const{return mResource.Get();}
		const D3D12_RESOURCE_DESC& getDesc()const{return mDesc;}
		ResourceType getType()const{return mType;}
		static size_t hash(const D3D12_RESOURCE_DESC& desc);
		size_t hash();
		UINT64 getSize()const{return mDesc.Width;}
		const DescriptorHandle& getHandle();
	protected:
		virtual DescriptorHandle createView();
	private:
		ComPtr<ID3D12Resource> mResource;
		D3D12_RESOURCE_DESC mDesc;
		D3D12_RESOURCE_STATES mState;
		ResourceType mType = RT_PERSISTENT;
		size_t mHashValue = 0;
		std::wstring mName;
		DescriptorHandle mHandle;

	};

	class ConstantBuffer final: public Resource
	{
	public:
		using Resource::Resource;
		void init(size_t size, D3D12_HEAP_TYPE ht, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
	private:
		DescriptorHandle createView() override;
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

		using Resource::Resource;
		Texture(ComPtr<ID3D12Resource> res, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON):Resource(res, state){}

		virtual void init(UINT width, UINT height, D3D12_HEAP_TYPE ht, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags );

	private:
		DescriptorHandle createView() override;
	};

	class ResourceView final:public Interface<ResourceView>
	{
	public:


		ResourceView(ViewType type, const Texture::Ref& res);
		ResourceView(ViewType type, UINT width, UINT height, DXGI_FORMAT format, Resource::ResourceType rt = Resource::RT_PERSISTENT);
		~ResourceView();

		const DescriptorHandle& getHandle()const;
		operator const DescriptorHandle& ()const;

		const Texture::Ref& getTexture()const;
		ViewType getType()const{return mType;};

	private:
		void createView();
		DescriptorHeapType matchDescriptorHeapType()const;
	private:
		DescriptorHandle mHandle;
		Texture::Ref mTexture;
		ViewType mType = VT_UNKNOWN;
	};


	class Buffer
	{
	public:
		using Ptr = std::shared_ptr<Buffer>;

		Buffer(UINT size, UINT stride, D3D12_HEAP_TYPE type);

		Resource::Ref getResource()const{return mResource;}
		void blit(const void* buffer, size_t size);
		UINT getSize()const{ return (UINT)mResource->getSize();}
		UINT getStride()const{return  mStride;}
		D3D12_GPU_VIRTUAL_ADDRESS getVirtualAddress()const{return mResource->get()->GetGPUVirtualAddress();}
	private:
		Resource::Ref mResource;
		UINT mStride;
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
	class RootParameter
	{
	public:
		RootParameter(D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

		void cbv(UINT start, UINT space = 0, UINT num = 1 );
		void cbv32(UINT start, UINT space = 0, UINT num = 1);
		void srv(UINT start, UINT space = 0, UINT num = 1);
		void uav(UINT start, UINT space = 0, UINT num = 1);

		D3D12_ROOT_PARAMETER genParameter()const;
	private:
		void record(UINT start, UINT space , UINT num);
	private:
		D3D12_SHADER_VISIBILITY mVisibility = D3D12_SHADER_VISIBILITY_ALL;
		D3D12_ROOT_PARAMETER_TYPE mType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		D3D12_DESCRIPTOR_RANGE mRange = {};
	};
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
		Shader(const MemoryData& data, ShaderType type);
		void registerStaticSampler(const D3D12_STATIC_SAMPLER_DESC& desc);

	private:
		D3D12_SHADER_VISIBILITY getShaderVisibility()const;
		D3D12_DESCRIPTOR_RANGE_TYPE getRangeType(D3D_SHADER_INPUT_TYPE type)const;
		void createRootParameters();
	private:
		ShaderType mType;
		MemoryData mCodeBlob;

		ComPtr<ShaderReflection> mReflection;
		std::vector< D3D12_STATIC_SAMPLER_DESC> mStaticSamplers;
		std::vector<D3D12_ROOT_PARAMETER> mRootParameters;
		std::vector<D3D12_DESCRIPTOR_RANGE> mRanges;

		struct Variable
		{
			UINT offset;
			UINT size;
		};

		struct CBuffer
		{
			UINT slot;
			UINT size;
			std::map<std::string, Variable> variables;
		};
		struct Reflection
		{
			UINT offset = 0;
			std::map<std::string, CBuffer> cbuffers;
			std::map<std::string, UINT> textures;
			std::map<std::string, UINT> samplers;
			std::map<std::string, UINT> uavs;
		};

		Reflection mSemanticsMap;
	};

	class RenderState
	{
	public:
		friend class Renderer::PipelineState;
		static const RenderState Default;
		static const RenderState GeneralSolid;
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
		friend class Renderer;
	public:
		PipelineState(const RenderState& rs, const std::vector<Shader::Ptr>& shaders, const std::vector<RootParameter>& params);
		~PipelineState();

		ID3D12PipelineState* get(){return mPipelineState.Get();}
		ID3D12RootSignature* getRootSignature(){return mRootSignature.Get();}

		void setResource(Shader::ShaderType type,const std::string& name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
		void setVSResource(const std::string& name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
		void setPSResource( const std::string& name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);

		void setConstant(Shader::ShaderType type, const std::string& name, const void* data);
		void setVSConstant(const std::string& name, const void* data);
		void setPSConstant(const std::string& name, const void* data);

	private:
		void createConstantBuffer();
		void refreshConstantBuffer();
		void setRootDescriptorTable(CommandList* cmdlist);
	private:
		ComPtr<ID3D12PipelineState> mPipelineState;
		ComPtr<ID3D12RootSignature> mRootSignature;

		std::map<Shader::ShaderType, Shader::Reflection> mSemanticsMap;
		std::map<Shader::ShaderType, std::map<UINT, D3D12_GPU_DESCRIPTOR_HANDLE>> mTextures;

		struct CBuffer
		{
			UINT slot;
			UINT size;
			MemoryData cpubuffer;
			Resource::Ref gpubuffer;
			bool needrefesh;
		};

		std::map<Shader::ShaderType, std::map<std::string, CBuffer>> mCBuffers;
	};

	class Profile:public Interface<Profile>
	{
		friend class Renderer;
		const static DWORD INTERVAL = 1;
	public:
		Profile(UINT index);

		float getCPUTime();
		float getGPUTime();

		void begin();
		void end();
	private:
		UINT mIndex;
		float mCPUHistory = 0;
		float mGPUHistory = 0;
		DWORD mDuration;
		DWORD mAccum = 0;
		UINT mFrameCount = 0;
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

		void discardResource(const ResourceView::Ref& rt);
		void clearRenderTarget(const ResourceView::Ref& rt, const Color& color);
		void clearDepth(const ResourceView::Ref& rt, float depth);
		void clearStencil(const ResourceView::Ref& rt, UINT8  stencil);
		void clearDepthStencil(const ResourceView::Ref& rt, float depth, UINT8 stencil);

		void setViewport(const D3D12_VIEWPORT& vp);
		void setScissorRect(const D3D12_RECT& rect);
		void setRenderTarget(const ResourceView::Ref& rt, const ResourceView::Ref& ds = {});
		void setRenderTargets(const std::vector<ResourceView::Ref>& rts, const ResourceView::Ref& ds = {});
		void setPipelineState(PipelineState::Ref ps);
		void setVertexBuffer(const std::vector<Buffer::Ptr>& vertices);
		void setVertexBuffer(const Buffer::Ptr& vertices);
		void setIndexBuffer(const Buffer::Ptr& indices);
		void setPrimitiveType(D3D_PRIMITIVE_TOPOLOGY type = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		void setTexture(UINT slot, Texture::Ref tex );
		void setTexture(UINT slot,const D3D12_GPU_DESCRIPTOR_HANDLE& handle);

		void setRootDescriptorTable(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
		void set32BitConstants(UINT slot, UINT num, const void* data, UINT offset);
		void drawInstanced(UINT vertexCount, UINT instanceCount = 1, UINT startVertex = 0, UINT startInstance = 0);
		void drawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndex = 0, INT startVertex = 0, UINT startInstance = 0);
		void endQuery(ComPtr<ID3D12QueryHeap> queryheap, D3D12_QUERY_TYPE type, UINT queryidx);
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

	void setVSync(bool enable);
	void addSearchPath(const std::wstring& path);
	HWND getWindow()const;
	ID3D12Device* getDevice();
	ID3D12CommandQueue* getCommandQueue();
	CommandList::Ref getCommandList();
	ResourceView::Ref getBackBuffer();
	void flushCommandQueue();
	void updateResource(Resource::Ref res, const void* buffer, UINT64 size, const std::function<void(CommandList::Ref, Resource::Ref )>& copy);
	//void updateBuffer(Resource::Ref res, const void* buffer, size_t size);
	//void updateTexture(Resource::Ref res,const void* buffer, size_t numRows, size_t rowSize);

	Shader::Ptr compileShader(const std::wstring& path, const std::wstring& entry, const std::wstring& target, const std::vector<D3D_SHADER_MACRO>& macros = {});
	Fence::Ptr createFence();
	Resource::Ref createResource(size_t size, D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT, Resource::ResourceType restype = Resource::RT_PERSISTENT);
	void destroyResource(Resource::Ref res);
	Texture::Ref createTexture(int width, int height, DXGI_FORMAT format, D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, Resource::ResourceType restype = Resource::RT_PERSISTENT);
	Texture::Ref createTexture(const std::wstring& filename);
	Buffer::Ptr createBuffer(UINT size, UINT stride, D3D12_HEAP_TYPE type, const void* data = nullptr, size_t count = 0);
	Resource::Ref createConstantBuffer(UINT size);
	PipelineState::Ref createPipelineState(const std::vector<Shader::Ptr>& shaders, const RenderState& rs, const std::vector<RootParameter>& rootparams);
	ResourceView::Ptr createResourceView(int width, int height, DXGI_FORMAT format, ViewType vt, Resource::ResourceType rt = Resource::RT_PERSISTENT);
	Profile::Ref createProfile();
	ComPtr<ID3D12QueryHeap> getTimeStampQueryHeap();
private:
	MemoryData createMemoryData(size_t size = 0)
	{
		return MemoryData(new std::vector<char>(size));
	}

	void uninitialize();
	void initDevice();
	void initCommands();
	void initDescriptorHeap();
	void initProfile();

	std::wstring findFile(const std::wstring& filename);

	CommandAllocator::Ptr allocCommandAllocator();
	void recycleCommandAllocator(CommandAllocator::Ptr ca);
	void commitCommands();
	void resetCommands();
	void syncFrame();

	ComPtr<IDXGIFACTORY> getDXGIFactory();
	ComPtr<IDXGIAdapter> getAdapter();
	DescriptorHeap::Ref getDescriptorHeap(DescriptorHeapType);
	Resource::Ref findTransient(const D3D12_RESOURCE_DESC& desc);
	void addResource(Resource::Ptr res);

	void present();
	void updateTimeStamp();
private:
	static Renderer::Ptr instance;

	HWND mWindow;
	std::vector<std::wstring> mFileSearchPaths;

	ComPtr<ID3D12Device> mDevice;
	ComPtr<IDXGISwapChain3> mSwapChain;
	ComPtr<ID3D12CommandQueue> mCommandQueue;
	Fence::Ptr mQueueFence;
	CommandList::Ptr mCommandList;
	CommandList::Ptr mResourceCommandList;

	std::vector<CommandAllocator::Ptr> mCommandAllocators;
	UINT mCurrentFrame;
	CommandAllocator::Ptr mCurrentCommandAllocator;


	std::array< ResourceView::Ptr, NUM_BACK_BUFFERS> mBackbuffers;
	std::array<DescriptorHeap::Ptr, DHT_MAX_NUM> mDescriptorHeaps;
	std::vector<D3D12_RESOURCE_BARRIER> mResourceBarriers;
	std::vector<Resource::Ptr> mResources;
	std::unordered_map<size_t, std::vector<Resource::Ref>> mTransients;

	std::unordered_map<std::wstring, Texture::Ref> mTextureMap;
	std::vector<PipelineState::Ptr> mPipelineStates;
	ComPtr<ID3D12QueryHeap> mTimeStampQueryHeap;
	std::vector<Profile::Ptr> mProfiles;
	Resource::Ref mProfileReadBack;
	CommandAllocator::Ptr mProfileCmdAlloc;




	bool mVSync = true;

};