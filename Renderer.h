#pragma once
#include "Common.h"
#include "TaskExecutor.h"
#include "Fence.h"

#if defined(D3D12ON7) 
	#include "D3D12Downlevel.h"

	#define SM_VS	"vs_5_0"
	#define SM_GS	"gs_5_0"
	#define SM_PS	"ps_5_0"
	#define SM_CS	"cs_5_0"
#else
	#define SM_VS	"vs_5_0"
	#define SM_GS	"gs_5_0"
	#define SM_PS	"ps_5_0"
	#define SM_CS	"cs_5_0"
#endif

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


public:
	static constexpr auto FEATURE_LEVEL = D3D_FEATURE_LEVEL_12_0;
	static auto constexpr NUM_BACK_BUFFERS = 3;
	static auto constexpr NUM_MAX_RENDER_TARGET_VIEWS = 8 * 1024;
	static auto constexpr NUM_MAX_DEPTH_STENCIL_VIEWS = 4 * 1024;
	static auto constexpr NUM_MAX_CBV_SRV_UAVS = 32 * 1024;
	static DXGI_FORMAT const FRAME_BUFFER_FORMAT;
	static DXGI_FORMAT const BACK_BUFFER_FORMAT;
	static size_t const  NUM_COMMANDLISTS;
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
	struct DebugInfo
	{
		std::string adapter;

		size_t drawcallCount = 0;
		size_t primitiveCount = 0;
		size_t numResources = 0;
		size_t videoMemory = 0;

		void reset()
		{
			drawcallCount = 0;
			primitiveCount = 0;
			numResources = 0;
			videoMemory = 0;
		}

		void operator =(const DebugInfo& di)
		{
			drawcallCount = di.drawcallCount;
			primitiveCount = di.primitiveCount;
			numResources = di.numResources;
			videoMemory = di.videoMemory;
		}
	};


	template<class T>
	class WeakPtr
	{
	public:
		inline WeakPtr()
		{
		}

		inline WeakPtr(const std::shared_ptr<T>& p) :
			mPointer(p)
		{
		}

		inline WeakPtr(WeakPtr&& p) noexcept :
			mPointer(std::move(p.mPointer))
		{
		}

		inline WeakPtr(const WeakPtr& p) :
			mPointer(p.mPointer)
		{
		}
		

		template<class U>
		inline WeakPtr(const WeakPtr<U>& p):
			mPointer(std::static_pointer_cast<T>(p.shared()))
		{
		}

		inline void operator=(const WeakPtr& p)
		{
			mPointer = p.mPointer;
		}

		inline operator bool() const
		{
			return !mPointer.expired();
		}


		inline bool operator==(const WeakPtr<T>& p)const
		{
			if (mPointer.expired() || p.mPointer.expired())
				return false;
			return mPointer.lock().get() == p.mPointer.lock().get();
		}
		
		inline bool operator== (const std::shared_ptr<T>& p)const
		{
			if (mPointer.expired())
				return false;
			return mPointer.lock().get() == p.get();
		}

		inline std::shared_ptr<T> operator->()const
		{
			ASSERT(!mPointer.expired(), "invalid pointer");
			return mPointer.lock();
		}


		inline std::weak_ptr<T> weak()const {
			return mPointer;
		}

		inline std::shared_ptr<T> shared() const{
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
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
		UINT64 pos = 0;

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

		bool valid()const
		{
			return cpu.ptr != 0 && gpu.ptr != 0;
		}

		void reset()
		{
			cpu.ptr = 0;
			gpu.ptr = 0;
			pos = -1;
		}
	private:

	};

	class CommandList;
	class Resource: public Interface<Resource>
	{
		friend class Renderer::CommandList;
		friend class Renderer;

		enum HandleType
		{
			HT_ShaderResource,
			HT_RenderTarget,
			HT_DepthStencil,
			HT_UnorderedAccess,

			HT_Num
		};
	public:

		Resource();
		Resource(ComPtr<ID3D12Resource> res, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);
		virtual ~Resource();
		void init(UINT64 size, D3D12_HEAP_TYPE ht, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
		void init(const D3D12_RESOURCE_DESC& resdesc, D3D12_HEAP_TYPE ht, D3D12_RESOURCE_STATES ressate);
		void init(UINT width, UINT height, D3D12_HEAP_TYPE ht, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags);
		virtual void blit(const void* data, UINT64 size, UINT subresource = 0);
		char* map(UINT sub);
		void unmap(UINT sub);
		

		const D3D12_RESOURCE_STATES& getState(UINT sub = 0)const;
		// transition by cmdlist
		//void setState(const D3D12_RESOURCE_STATES& s);
		void setName(const std::string& name);
		const std::string& getName()const{return mName;}

		ID3D12Resource* get()const{return mResource.Get();}
		const D3D12_RESOURCE_DESC& getDesc()const{return mDesc;}
		UINT64 getSize()const{return mDesc.Width;}
		
		const D3D12_GPU_DESCRIPTOR_HANDLE& getShaderResource(UINT i = 0);
		const D3D12_CPU_DESCRIPTOR_HANDLE& getRenderTarget(UINT i = 0);
		const D3D12_CPU_DESCRIPTOR_HANDLE& getDepthStencil(UINT i = 0);
		const D3D12_GPU_DESCRIPTOR_HANDLE& getUnorderedAccess(UINT i = 0);

		void createShaderResource(const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr, UINT i = 0);
		void createBuffer(DXGI_FORMAT format, UINT64 begin, UINT num, UINT stride, UINT i =0);
		void createTexture2D(UINT begin = 0, UINT count = -1, UINT i = 0);
		void createTextureCube(UINT begin = 0, UINT count = -1, UINT i = 0);
		void createTextureCubeArray(UINT begin = 0, UINT count = -1, UINT arrayBegin = 0, UINT numCubes = -1, UINT i = 0);

		ViewType getViewType()const ;
		void createRenderTargetView(const D3D12_RENDER_TARGET_VIEW_DESC* desc, UINT index = 0);
		void createDepthStencilView(const D3D12_DEPTH_STENCIL_VIEW_DESC* desc, UINT index = 0);
		void createUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, UINT index = 0);

	private:
		DescriptorHandle& assignHandle(UINT i, HandleType type);
		void releaseAllHandle();

		D3D12_GPU_VIRTUAL_ADDRESS getVirtualAddress()const;

	private:
		ComPtr<ID3D12Resource> mResource;
		D3D12_RESOURCE_DESC mDesc;
		std::vector<D3D12_RESOURCE_STATES> mState;
		std::string mName;
		std::array<std::vector<DescriptorHandle>, HT_Num> mHandles;
	};

	class ConstantBufferAllocator:public Interface<ConstantBufferAllocator>
	{
	static const UINT64 num_consts = 1024;
	static const UINT64 cache_size = num_consts * 256;

	public:
		ConstantBufferAllocator();
		UINT64 alloc(UINT64 size);
		void dealloc(UINT64 address, UINT64 size);

		D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress();
		void blit(UINT64 offset, const void* buffer, UINT64 size);
		void sync();
	private:
		Resource::Ref mResource;
		std::array<char,cache_size> mCache;
		std::vector< std::vector<UINT64>> mFree;
		char* mEnd;
		bool mRefresh = false;
	};


	class Buffer : public Resource
	{
		friend class Renderer;
	public:
		using Ref = WeakPtr<Buffer>;

		UINT getSize()const{ return (UINT)Resource::getSize();}
		UINT getStride()const{return  mStride;}
	private:
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
		std::vector<UINT> mUsed;

	};

	class CommandQueue;
	class Fence : public Interface<Fence>
	{
		friend class Renderer::CommandQueue;
	public :
		Fence();
		~Fence();
		void wait();
		void wait(ID3D12CommandQueue* q);
		void signal();
		void signal(ID3D12CommandQueue* q);
		bool completed();
	private:
		ComPtr<ID3D12Fence> mFence;
		HANDLE mFenceEvent;
		UINT64 mFenceValue;
	};

	class CommandAllocator final: public Interface<CommandAllocator>
	{
	friend class Renderer::CommandList;
	public:
		CommandAllocator(D3D12_COMMAND_LIST_TYPE type);
		~CommandAllocator();
		void reset();
		void wait();
		void signal(ID3D12CommandQueue* q);
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
			std::map<std::string, CBuffer> cbuffersBy32Bits;
			std::map<std::string, UINT> textures;
			std::map<UINT, UINT> texturesBySlot;
			std::map<std::string, UINT> samplers;
			std::map<std::string, UINT> uavs;
		};

	public:
		Shader(const MemoryData& data, ShaderType type);
		void registerStaticSampler( const D3D12_STATIC_SAMPLER_DESC& desc);
		void registerStaticSampler(const std::string& name, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE mode);
		void enable32BitsConstants(bool b);
		void enable32BitsConstantsByName(const std::string& name);
		void enableStaticSampler(bool b);
	private:
		D3D12_SHADER_VISIBILITY getShaderVisibility()const;
		D3D12_DESCRIPTOR_RANGE_TYPE getRangeType(D3D_SHADER_INPUT_TYPE type)const;
		void createRootParameters();
	private:
		ShaderType mType;
		MemoryData mCodeBlob;
		bool mUseStaticSamplers = true; 
		bool mUse32BitsConstants = false;
		std::set<std::string> mUse32BitsConstantsSet;

		ComPtr<ShaderReflection> mReflection;
		std::map<std::string, D3D12_STATIC_SAMPLER_DESC> mSamplerMap;
		std::vector< D3D12_STATIC_SAMPLER_DESC> mStaticSamplers;
		std::vector<D3D12_ROOT_PARAMETER> mRootParameters;
		std::vector<D3D12_DESCRIPTOR_RANGE> mRanges;


		Reflection mSemanticsMap;
	};


	class ConstantBuffer
	{
	public:
		using Ptr = std::shared_ptr<ConstantBuffer>;

		ConstantBuffer(size_t size, ConstantBufferAllocator::Ref allocator);
		~ConstantBuffer();

		void setReflection(const std::map<std::string, Shader::Variable>& rft);
		template<class T>
		void setVariable(const std::string& name, const T& v)
		{
			setVariable(name, &v, sizeof(v));
		}
		void setVariable(const std::string& name, const void* data, size_t size);
		void blit(const void* buffer, UINT64 offset = 0, UINT64 size = -1);
		D3D12_GPU_DESCRIPTOR_HANDLE getHandle()const;
	private:
		enum
		{
			CT_32BIT,
			CT_ROOT_CONST,
			CT_ROOT_DESCTABLE,
		};
	private:
		size_t mType;
		UINT64 mSize;
		UINT64 mOffset;
		DescriptorHandle mView;
		ConstantBufferAllocator::Ref mAllocator;
		std::map<std::string, Shader::Variable> mVariables;
	};


	class RenderState
	{
	public:
		friend class Renderer::PipelineState;
		static const RenderState Default;
		static const RenderState GeneralSolid;
	public:
		RenderState(std::function<void(RenderState& self)> initializer);
		RenderState(DXGI_FORMAT targetfmt);

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
		enum Type
		{
			PST_Graphic,
			PST_Compute,
		};

		PipelineState(const RenderState& rs, const std::vector<Shader::Ptr>& shaders);
		PipelineState(const Shader::Ptr& computeShader);
		~PipelineState();

		ID3D12PipelineState* get(){return mPipelineState.Get();}
		ID3D12RootSignature* getRootSignature(){return mRootSignature.Get();}
		Type getType()const {return mType;}


		void setResource(Shader::ShaderType type,const std::string& name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
		void setVSResource(const std::string& name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
		void setPSResource( const std::string& name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
		void setCSResource(const std::string& name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);

		void setResource(Shader::ShaderType type, UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
		void setVSResource(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
		void setPSResource(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
		void setCSResource(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);

		void setConstant(Shader::ShaderType type, const std::string& name,const ConstantBuffer::Ptr& c);
		void setVSConstant(const std::string& name, const ConstantBuffer::Ptr& c);
		void setPSConstant(const std::string& name, const ConstantBuffer::Ptr& c);
		void setCSConstant(const std::string& name, const ConstantBuffer::Ptr& c);


		void setVariable(Shader::ShaderType type, const std::string& name, const void* data);
		void setVSVariable( const std::string& name, const void* data);
		void setPSVariable( const std::string& name, const void* data);
		void setCSVariable(const std::string& name, const void* data);


		ConstantBuffer::Ptr createConstantBuffer(Shader::ShaderType type, const std::string& name);
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC& getDesc()const;
	private:
		void setRootDescriptorTable(CommandList* cmdlist);
	private:
		Type mType = PST_Graphic;
		ComPtr<ID3D12PipelineState> mPipelineState;
		ComPtr<ID3D12RootSignature> mRootSignature;

		std::map<Shader::ShaderType, Shader::Reflection> mSemanticsMap;
		std::map<Shader::ShaderType, std::map<UINT, D3D12_GPU_DESCRIPTOR_HANDLE>> mTextures;
		std::map<Shader::ShaderType, std::map<UINT, D3D12_GPU_DESCRIPTOR_HANDLE>> mCBuffers;
		std::map<Shader::ShaderType, std::map<UINT, std::vector<char>>> mCBuffersBy32Bits;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC mDesc;
	};



	class CommandList final : public Interface<CommandList>
	{
		friend class Renderer;
	public:
		CommandList(ID3D12CommandQueue* q, D3D12_COMMAND_LIST_TYPE type);
		~CommandList();
		//ID3D12GraphicsCommandList* get();


		void close();
		void reset();

		void transitionBarrier( Resource::Ref res, D3D12_RESOURCE_STATES state, UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool autoflush = false);
		void uavBarrier(Resource::Ref res, bool autoflush = false);
		void addResourceTransition(const Resource::Ref& res, D3D12_RESOURCE_STATES state, UINT subresource);
		void flushResourceBarrier();
		void copyBuffer(Resource::Ref dst, UINT dstStart, Resource::Ref src, UINT srcStart, UINT64 size );
		void copyTexture(Resource::Ref dst, UINT dstSub, const std::array<UINT, 3>& dstStart, Resource::Ref src, UINT srcSub, const D3D12_BOX* srcBox );
		void copyResource(const Resource::Ref& dst, const Resource::Ref& src);
		void discardResource(const Resource::Ref& rt);
		void clearRenderTarget(const Resource::Ref& rt, const Color& color);
		void clearDepth(const Resource::Ref& rt, float depth);
		void clearStencil(const Resource::Ref& rt, UINT8  stencil);
		void clearDepthStencil(const Resource::Ref& rt, float depth, UINT8 stencil);

		void setViewport(const D3D12_VIEWPORT& vp);
		void setViewportToScreen();
		void setScissorRect(const D3D12_RECT& rect);
		void setScissorRectToScreen();
		void setRenderTarget(const Resource::Ref& rt, const Resource::Ref& ds = {});
		void setRenderTargets(const std::vector<Resource::Ref>& rts, const Resource::Ref& ds = {});
		void setRenderTargets(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& rts, const D3D12_CPU_DESCRIPTOR_HANDLE* ds = 0);
		void setPipelineState(PipelineState::Ref ps);
		void setVertexBuffer(const std::vector<Buffer::Ref>& vertices);
		void setVertexBuffer(const Buffer::Ref& vertices);
		void setIndexBuffer(const Buffer::Ref& indices);
		void setPrimitiveType(D3D_PRIMITIVE_TOPOLOGY type = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		void setDescriptorHeap(DescriptorHeap::Ref heap);
		void setRootDescriptorTable(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
		void setComputeRootDescriptorTable(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
		void set32BitConstants(UINT slot, UINT num, const void* data, UINT offset);
		void setCompute32BitConstants(UINT slot, UINT num, const void* data, UINT offset);

		void drawInstanced(UINT vertexCount, UINT instanceCount = 1, UINT startVertex = 0, UINT startInstance = 0);
		void drawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount = 1U, UINT startIndex = 0, INT startVertex = 0, UINT startInstance = 0);
		void dispatch(UINT x, UINT y , UINT z);
		void endQuery(ComPtr<ID3D12QueryHeap> queryheap, D3D12_QUERY_TYPE type, UINT queryidx);

		//Fence::Ptr getFence(){return mAllocator->mFence;}
		CommandAllocator::Ref getAllocator(){return mAllocators[mCurrentAllocator];}
	private:
		static const auto NUM_ALLOCATORS = 4;
		ID3D12CommandQueue* mQueue;
		std::array<CommandAllocator::Ptr, NUM_ALLOCATORS> mAllocators;
		size_t mCurrentAllocator = 0;
		ComPtr<ID3D12GraphicsCommandList> mCmdList;
		PipelineState::Ref mCurrentPipelineState;


		struct Transition
		{
			Resource::Ref res;
			D3D12_RESOURCE_STATES state;
			UINT subresource;
		};
		std::unordered_map<ID3D12Resource*,Transition> mTransitionBarrier;
		std::unordered_map<ID3D12Resource*, Resource::Ref> mUAVBarrier;
	};

	class Profile :public Interface<Profile>
	{
		friend class Renderer;
	public:
		Profile(UINT index);

		float getCPUTime();
		float getGPUTime();

		float getCPUMax();
		float getGPUMax();

		void reset();

		void begin(Renderer::CommandList::Ref cl);
		void end(Renderer::CommandList::Ref cl);
	private:
		UINT mIndex;
		float mCPUHistory = 0;
		float mCPUMax = 0;
		float mGPUHistory = 0;
		float mGPUMax = 0;
		std::chrono::high_resolution_clock::time_point mCPUTime;
	};



	class CommandQueue : public Interface<CommandQueue>
	{
	public:
		using Command = std::function<void(CommandList::Ref)>;
	public:
		CommandQueue(D3D12_COMMAND_LIST_TYPE type, size_t maxCmdlistSize = NUM_COMMANDLISTS);
		~CommandQueue();

		void addCommand(Command&& task, bool strand = false, bool impl = false);

		void execute();
		void flush();
		ID3D12CommandQueue* get();

		void signal();
		void wait();
		void wait(CommandQueue::Ref prequeue);

		Fence::Ref getFence();
	private:
		std::vector<CommandList::Ptr> mCommandLists;
		std::vector<ID3D12CommandList*> mOriginCommandLists;
		size_t mMaxCommandListSize;

		ComPtr<ID3D12CommandQueue> mQueue;
		UINT mUsedCommandListsCount = 0;
		TaskExecutor mTaskExecutor{ Dispatcher::getSharedContext() };
		Fence::Ptr mFence;
		std::mutex mMutex;
		DescriptorHeap::Ref mHeap;
	};

	using RenderTask = CommandQueue::Command;
	using ObjectTask = std::function<void()>;


	static Renderer::Ptr create();
	static void destory();
	static Renderer::Ptr getSingleton();


	Renderer();
	~Renderer();
	void initialize(HWND window);
	void resize(int width, int height);
	void beginFrame();
	void endFrame();
	std::string findFile(const std::string& filename);

	std::array<LONG,2> getSize();
	void setVSync(bool enable);
	void addSearchPath(const std::string& path);
	const DebugInfo& getDebugInfo()const;
	HWND getWindow()const;
	ID3D12Device* getDevice();
	CommandQueue::Ref getRenderQueue();
	CommandQueue::Ref getComputeQueue();
	Resource::Ref getBackBuffer();
	UINT getCurrentFrameIndex();
	void updateResource(Resource::Ref res, UINT subresource, const void* buffer, UINT64 size, const std::function<void(CommandList::Ref, Resource::Ref, UINT)>& copy);
	void updateBuffer(Resource::Ref res, UINT subresource, const void* buffer, UINT64 size);
	void updateTexture(Resource::Ref res, UINT subresource, const void* buffer, UINT64 size, bool srgb);
	void executeResourceCommands(RenderTask&& dofunc);

	Shader::Ptr compileShaderFromFile(const std::string& path, const std::string& entry, const std::string& target, const std::vector<D3D_SHADER_MACRO>& macros = {});
	Shader::Ptr compileShader(const std::string& name, const std::string& context, const std::string& entry, const std::string& target, const std::vector<D3D_SHADER_MACRO>& macros = {}, const std::string& cachename = {});
	Fence::Ptr createFence();
	// resource 
	void destroyResource(Resource::Ref res);
	// texture
	Resource::Ref createResourceView(UINT width, UINT height, DXGI_FORMAT format, ViewType type);
	Resource::Ref createTexture2DBase(UINT width, UINT height, UINT depth, DXGI_FORMAT format, UINT nummips = 1, D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	Resource::Ref createTexture2D(UINT width, UINT height, DXGI_FORMAT format, UINT miplevels, const void* data, bool srgb);
	Resource::Ref createTextureCube(UINT size, DXGI_FORMAT format, UINT nummips = 1, D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	Resource::Ref createTextureCubeArray(UINT size, DXGI_FORMAT format, UINT arraySize, UINT nummips = 1, D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	Resource::Ref createTextureFromFile(const std::string& filename, bool srgb);
	Resource::Ref createTexture3D(UINT width, UINT height, UINT depth, DXGI_FORMAT format, UINT miplevels = 1, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT);
	// buffer
	Resource::Ref Renderer::createBufferBase(size_t size, bool isShaderResource,D3D12_HEAP_TYPE type );
	Buffer::Ref createBuffer(UINT size, UINT stride, bool isShaderResource, D3D12_HEAP_TYPE type, const void* data = nullptr, size_t count = 0);
	ConstantBuffer::Ptr createConstantBuffer(UINT size);
	PipelineState::Ref createPipelineState(const std::vector<Shader::Ptr>& shaders, const RenderState& rs);
	PipelineState::Ref createComputePipelineState(const Shader::Ptr& shader);
	void destroyPipelineState(PipelineState::Ref pso);
	Profile::Ref createProfile();
	void generateMips(Resource::Ref texture);


	void addRenderTask(RenderTask&& task, bool strand = false, bool impl = false);
	void addFencingTask(ObjectTask&& task);
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
	void initResources();
	Shader::ShaderType mapShaderType(const std::string& target);
	void collectDebugInfo();

	void fetchNextFrame();
	void processTasks();

	ComPtr<IDXGIFACTORY> getDXGIFactory();
	std::vector<ComPtr<IDXGIAdapter>> getAdapter();
	DescriptorHeap::Ref getDescriptorHeap(DescriptorHeapType);
	void addResource(Resource::Ptr res);

	void present();
	void updateTimeStamp();
private:
	static Renderer::Ptr instance;

	HWND mWindow;
	std::vector<std::string> mFileSearchPaths;

	ComPtr<ID3D12Device> mDevice;
	ComPtr<IDXGISwapChain3> mSwapChain;
	CommandQueue::Ptr mRenderQueue;
	CommandQueue::Ptr mComputeQueue;
	CommandQueue::Ptr mResourceQueue;
	CommandQueue::Ptr mTimerQueue;


	UINT mCurrentFrame;


	std::array< Resource::Ptr, NUM_BACK_BUFFERS> mBackbuffers;
	std::array<DescriptorHeap::Ptr, DHT_MAX_NUM> mDescriptorHeaps;
	std::vector<Resource::Ptr> mResources;

	std::unordered_map<std::string, Resource::Ref> mTextureMap;
	std::vector<PipelineState::Ptr> mPipelineStates;
	ComPtr<ID3D12QueryHeap> mTimeStampQueryHeap;
	std::vector<Profile::Ptr> mProfiles;
	Profile::Ref mRenderProfile;
	Resource::Ref mProfileReadBack;
	//CommandAllocator::Ptr mProfileCmdAlloc;
	ConstantBufferAllocator::Ptr mConstantBufferAllocator;
	std::array<PipelineState::Ref, 4> mGenMipsPSO;
	PipelineState::Ref mSRGBConv;
	bool mVSync = false;

	asio::io_context mFencingContext;
	TaskExecutor mFencingTasks{ mFencingContext };
	//std::vector<ObjectTask> mFencingTasks;


};