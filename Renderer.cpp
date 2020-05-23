#include "Renderer.h"
#include "Profile.h"
#include "Fence.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "D3DHelper.h"

#include <sstream>

#undef min
#undef max

Renderer::Ptr Renderer::instance;





static Renderer::DebugInfo debugInfo;
static Renderer::DebugInfo debugInfoCache;


DXGI_FORMAT constexpr Renderer::FRAME_BUFFER_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;
DXGI_FORMAT constexpr Renderer::BACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
size_t const Renderer::NUM_COMMANDLIST = 256;
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
	mFileSearchPaths = {
		"",
		"../",
		"../Engine/",
		"../Engine/Shaders/",
		"Engine/",
		"Engine/Shaders/"
	};
}

Renderer::~Renderer()
{
}

void Renderer::initialize(HWND window)
{
	mWindow = window;

#if defined(_DEBUG) && !defined (D3D12ON7)
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
	initProfile();
	initResources();
	resize(1,1);
	fetchNextFrame();
	
}

void Renderer::resize(int width, int height)
{
	flushCommandQueue();

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = NUM_BACK_BUFFERS;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = BACK_BUFFER_FORMAT;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

#if defined(D3D12ON7)
	
	ComPtr<ID3D12DeviceDownlevel> deviceDownlevel;
	CHECK(mDevice.As(&deviceDownlevel));

	for (auto& b: mBackbuffers)
	{
		b = Resource::create();
		b->init(width,height,D3D12_HEAP_TYPE_DEFAULT,swapChainDesc.Format,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		b->createRenderTargetView(nullptr);
	}
	mCurrentFrame = 0;
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
		res->createTexture2D();
		res->createRenderTargetView(nullptr);
		std::stringstream ss;
		ss << "BackBuffer" << i;
		res->setName(ss.str());
		mBackbuffers[i] = res;
	}

	mCurrentFrame = mSwapChain->GetCurrentBackBufferIndex();

#endif
}

void Renderer::beginFrame()
{
	debugInfo.reset();


	addRenderTask([this](auto cmdlist){
		cmdlist->transitionBarrier(mBackbuffers[mCurrentFrame], D3D12_RESOURCE_STATE_RENDER_TARGET, 0, true);
		mRenderProfile = ProfileMgr::Singleton.begin("render", cmdlist);
	},false, true);

	
}

void Renderer::endFrame()
{

	addRenderTask([this](auto cmdlist){
		cmdlist->transitionBarrier(mBackbuffers[mCurrentFrame], D3D12_RESOURCE_STATE_PRESENT, 0, true);
		ProfileMgr::Singleton.end(mRenderProfile, cmdlist);
	}, false, true);

	processTasks();

	commitCommands();

	present();

	fetchNextFrame();

	updateTimeStamp();

	collectDebugInfo();

}

std::array<LONG, 2> Renderer::getSize()
{
	RECT rect;
	::GetClientRect(mWindow, &rect);
	return {(LONG)rect.right, (LONG)rect.bottom};
}

void Renderer::setVSync(bool enable)
{
	mVSync = enable;
}

void Renderer::addSearchPath(const std::string & path)
{
	mFileSearchPaths.push_back(path);
}

const Renderer::DebugInfo & Renderer::getDebugInfo() const
{
	return debugInfoCache;
}

HWND Renderer::getWindow() const
{
	return mWindow;
}

ID3D12Device* Renderer::getDevice()
{
	return mDevice.Get();
}

ID3D12CommandQueue* Renderer::getCommandQueue()
{
	return mCommandQueue.Get();
}

Renderer::Resource::Ref Renderer::getBackBuffer()
{
	return mBackbuffers[mCurrentFrame];
}

UINT Renderer::getCurrentFrameIndex()
{
	return mCurrentFrame;
}

void Renderer::flushCommandQueue()
{
	mQueueFence->signal();
	mQueueFence->wait();
}

void Renderer::updateResource(Resource::Ref res, UINT subresource, const void* buffer, UINT64 size, const std::function<void(CommandList::Ref, Resource::Ref, UINT)>& copy)
{
	auto src = Resource::create();
	const auto& desc = res->getDesc();

	if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		src->init(desc, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		src->blit(buffer, std::min(size, desc.Width));
	}
	else if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
	{
		UINT64 requiredSize = 0;
		UINT64 rowSize = 0;
		UINT numRows = 0;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		Renderer::getSingleton()->getDevice()->GetCopyableFootprints(&desc, subresource, 1, 0, &footprint, &numRows, &rowSize, &requiredSize);
		
		D3D12_RESOURCE_DESC resdesc = {};
		resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resdesc.Alignment = 0;
		resdesc.Width = requiredSize;
		resdesc.Height = 1;
		resdesc.DepthOrArraySize = 1;
		resdesc.MipLevels = 1;
		resdesc.Format = DXGI_FORMAT_UNKNOWN;
		resdesc.SampleDesc.Count = 1;
		resdesc.SampleDesc.Quality = 0;
		resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		
		src->init(resdesc, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);


		char* data = src->map(0);
		for (size_t i = 0; i < numRows; ++i)
		{
			memcpy(data, buffer, (size_t)rowSize);
			data += footprint.Footprint.RowPitch;
			buffer = (const char*)buffer + rowSize;
		}
		src->unmap(0);
	}
	else
		ASSERT(0,"unsupported resource.");

	executeResourceCommands([&](CommandList::Ref cmdlist){
		cmdlist->transitionBarrier(res, D3D12_RESOURCE_STATE_COPY_DEST, -1, true);
		copy(cmdlist, src, subresource);
	});
}

void Renderer::updateBuffer(Resource::Ref res, UINT subresource, const void* buffer, UINT64 size)
{
	updateResource(res, subresource, buffer, size, [dst = res, size](CommandList::Ref cmdlist, Resource::Ref src, UINT sub){
		cmdlist->copyBuffer(dst, sub, src, 0, size);
	});
}

void Renderer::updateTexture(Resource::Ref res, UINT subresource, const void* buffer, UINT64 size, bool srgb)
{
	Resource::Ptr uavref;

	updateResource(res, subresource, buffer, size, [dst = res, srgb , this, pso = mSRGBConv  , &uavref](auto cmdlist, auto src, auto sub) {
		if (srgb)
		{
			auto mid = Resource::create();
			uavref = mid;
			auto bufferdesc = src->getDesc();
			auto& texdesc = dst->getDesc();

			UINT64 requiredSize = 0;
			UINT64 rowSize = 0;
			UINT numRows = 0;
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
			Renderer::getSingleton()->getDevice()->GetCopyableFootprints(&texdesc, sub, 1, 0, &footprint, &numRows, &rowSize, &requiredSize);
			
			bufferdesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			mid->init(bufferdesc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			UINT stride = (UINT)D3DHelper::sizeof_DXGI_FORMAT(texdesc.Format);
			src->createBuffer(texdesc.Format,0, (UINT)requiredSize / stride,0,0);
			
			{
				D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc = {};
				uavdesc.Format = texdesc.Format;
				uavdesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				uavdesc.Buffer.FirstElement = 0;
				uavdesc.Buffer.NumElements = (UINT)requiredSize / stride;
				uavdesc.Buffer.StructureByteStride = 0;
				uavdesc.Buffer.CounterOffsetInBytes = 0;
				uavdesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
				mid->createUnorderedAccessView(&uavdesc);
			}
			//cmdlist->transitionBarrier(src, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0,true);

			pso->setResource(Shader::ST_COMPUTE,"input", src->getShaderResource());
			pso->setResource(Shader::ST_COMPUTE, "output", mid->getUnorderedAccess());
			UINT width = (UINT)texdesc.Width;
			pso->setVariable(Shader::ST_COMPUTE, "width", &width);

			cmdlist->setPipelineState(pso);
			cmdlist->dispatch(width, texdesc.Height,1);

			cmdlist->transitionBarrier(mid, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, true);
			cmdlist->copyTexture(dst, sub, {0,0,0}, mid,0,nullptr);
		}
		else
			cmdlist->copyTexture(dst, sub, { 0,0,0 }, src, 0, nullptr);
	});
}

void Renderer::executeResourceCommands(const std::function<void(CommandList::Ref)>& dofunc)
{

	mResourceCommandList->reset();

	auto heap = mDescriptorHeaps[DHT_CBV_SRV_UAV]->get();
	mResourceCommandList->get()->SetDescriptorHeaps(1, &heap);

	dofunc(mResourceCommandList);

	mResourceCommandList->close();
	ID3D12CommandList* ppCommandLists[] = { mResourceCommandList->get() };
	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	mResourceCommandList->getAllocator()->signal();
	mResourceCommandList->getAllocator()->wait();

}

Renderer::Shader::Ptr Renderer::compileShaderFromFile(const std::string & absfilepath, const std::string & entry, const std::string & target, const std::vector<D3D_SHADER_MACRO>& macros)
{
	auto path = findFile(absfilepath);
	std::fstream file(path, std::ios::in | std::ios::binary);
	ASSERT(!!file, "fail to open shader file");

	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::string context;
	context.resize(size);
	file.read(&context[0], size);
	return compileShader(absfilepath,context,entry,target,  macros);
}

Renderer::Shader::Ptr Renderer::compileShader(const std::string& name, const std::string & context, const std::string & entry, const std::string & target,const std::vector<D3D_SHADER_MACRO>& incomingMacros, const std::string& cachename)
{
	Shader::ShaderType type = mapShaderType(target);
	std::vector<D3D_SHADER_MACRO> macros = incomingMacros;
	if (!macros.empty())
	{
		if (macros.back().Name != NULL)
		{
			macros.push_back({NULL,NULL});
		}
	}
	auto gethash = [this](const std::string& context, const std::string & entry, const std::string & target, const std::vector<D3D_SHADER_MACRO>& macros) {
		std::hash<std::string> hash;
		auto hashcontext = context + entry + target;
		for (auto& m : macros)
		{
			if (m.Name)
			{
				hashcontext += m.Name;
			}
			if (m.Definition)
			{
				hashcontext += m.Definition;
			}
		}
		std::stringstream ss;
		return hash(hashcontext);
	};

	auto hash = gethash(context, entry, target, macros);

	std::stringstream ss;
	ss << hash;
	std::string cachefilename = "cache/" + ss.str();
	{
		std::fstream cachefile(cachefilename, std::ios::in | std::ios::binary);
		auto read = [&](auto& val) {
			cachefile.read((char*)&val, sizeof(val));
		};
		if (cachefile)
		{
			bool fail = false;
			UINT32 numincludes;
			read(numincludes);
			for (UINT32 i = 0; i < numincludes; ++i)
			{
				UINT32 numstr;
				std::string include;
				read(numstr);

				include.resize(numstr);
				cachefile.read(&include[0], numstr);
				size_t oldhash;
				read(oldhash);
				
				std::fstream file(findFile(include), std::ios::in | std::ios::binary);
				file.seekg(0, std::ios::end);
				size_t size = file.tellg();
				file.seekg(0, std::ios::beg);
				std::string context;
				context.resize(size);
				file.read(&context[0], size);

				auto newhash = gethash(context,{}, target, macros);
				if (newhash != oldhash)
				{
					fail = true;
					break;
				}
			}

			if (!fail)
			{
				unsigned int size;
				read(size);
				auto buffer = createMemoryData(size);
				cachefile.read(buffer->data(), size);

				return std::make_shared<Shader>(buffer, mapShaderType(target));
			}
		}
	}



	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> err;

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_ENABLE_STRICTNESS;
#else
	UINT compileFlags = 0;
#endif

	struct Include : public ID3DInclude
	{
		std::string target;
		const std::vector<D3D_SHADER_MACRO>* macros;
		std::vector<std::pair<std::string, size_t>> includes;
		STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
		{
			std::fstream file(Renderer::getSingleton()->findFile(pFileName), std::ios::in | std::ios::binary);
			if (!!file)
				LOG(std::string("cannot find included file ") + pFileName );
			file.seekg(0,std::ios::end);
			size_t size = file.tellg();
			file.seekg(0,std::ios::beg);
			
			char* data = new char[size];
			file.read(data, size);
			(*ppData) = data;
			(*pBytes) = (UINT)size;

			std::hash<std::string> hash;
			std::string hashcontext = std::string(data, size) + "" +target;
			for (auto& m: *macros)
			{
				if (m.Name)
				{
					hashcontext += m.Name;
				}
				if (m.Definition)
				{
					hashcontext += m.Definition;
				}
			}
			includes.push_back({pFileName, hash(hashcontext)});
			return S_OK;
		}
		STDMETHOD(Close)(THIS_ LPCVOID pData)
		{
			
			delete pData;
			return S_OK;
		}

	}include;
	include.target = target;
	include.macros = &macros;

	if (FAILED(D3DCompile(context.data(), context.size(), name.c_str(), macros.data(),&include, (entry).c_str(), (target).c_str(), compileFlags, 0, &blob, &err)))
	{
		std::string debnugcontext = ">1	" + context;
		auto i = debnugcontext.begin();
		int line = 2;
		for (; i != debnugcontext.end(); )
		{
			if (*i == '\n')
			{
				std::stringstream conv;
				conv << ">" << line++ << "	";
				std::string p = conv.str();
				i = debnugcontext.insert(i + 1,p.begin(), p.end());
				i +=3;
			}
			else
				++i;
		}

		::OutputDebugStringA(debnugcontext.c_str());


		::OutputDebugStringA((const char*)err->GetBufferPointer());
		ASSERT(0, ((const char*)err->GetBufferPointer()));
		return {};
	}

	auto result = createMemoryData(blob->GetBufferSize());
	memcpy(result->data(), blob->GetBufferPointer(), result->size());

	{
		
		std::fstream cachefile(cachefilename, std::ios::out | std::ios::binary);
		auto write = [&](auto val)
		{
			cachefile.write((const char*)&val, sizeof(val));
		};
		
		write(UINT32(include.includes.size()));
		for (auto& i: include.includes)
		{
			write(UINT32(i.first.size()));
			cachefile.write(i.first.data(), i.first.size());
			write(i.second);
		}

		write( (unsigned int)result->size());
		cachefile.write(result->data(), result->size());
	}


	return Shader::Ptr(new Shader(result, type));
}


Renderer::Fence::Ptr Renderer::createFence()
{
	auto fence = Fence::create();
	return fence;
}



Renderer::Resource::Ref Renderer::createTexture2DBase(UINT width, UINT height, UINT depth,  DXGI_FORMAT format, UINT nummips, D3D12_HEAP_TYPE type,D3D12_RESOURCE_FLAGS flags)
{
	if (width == 0 || height == 0)
	{
		auto& tmp = mBackbuffers[0]->getDesc();
		width = (UINT)tmp.Width;
		height = tmp.Height;
	}

	if (format == DXGI_FORMAT_UNKNOWN)
	{
		format = FRAME_BUFFER_FORMAT;
	}

	unsigned long maxmips;
	_BitScanReverse(&maxmips, width | height);
	nummips = std::min((UINT)maxmips + 1, nummips);

	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resdesc.Alignment = 0;
	resdesc.Width = width;
	resdesc.Height = height;
	resdesc.DepthOrArraySize = depth;
	resdesc.MipLevels = nummips;
	resdesc.Format = format;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resdesc.Flags = flags;

	auto tex = Resource::create();
	tex->init(resdesc, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);

	addResource(tex);

	return tex;
}

Renderer::Resource::Ref Renderer::createTextureCube(UINT size, DXGI_FORMAT format, UINT nummips,D3D12_HEAP_TYPE type, D3D12_RESOURCE_FLAGS flags)
{
	auto texcube = createTexture2DBase(size, size, 6 ,format, nummips,type, flags);
	texcube->createTextureCube();
	return texcube;
}

Renderer::Resource::Ref Renderer::createTextureCubeArray(UINT size, DXGI_FORMAT format, UINT arraySize, UINT nummips, D3D12_HEAP_TYPE type, D3D12_RESOURCE_FLAGS flags)
{
	auto texcube = createTexture2DBase(size, size, 6 * arraySize, format, nummips, type, flags);
	texcube->createTextureCubeArray();
	return texcube;
}

Renderer::Resource::Ref Renderer::createTextureFromFile(const std::string& filename, bool srgb)
{
	auto ret = mTextureMap.find(filename);
	if (ret != mTextureMap.end())
		return ret->second;

	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

	void* data = 0;
	int width, height, nrComponents;

	std::string fn = (findFile((filename)));
	if (stbi_is_hdr(fn.c_str()))
	{
		format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		data = stbi_loadf(fn.c_str(), &width, &height, &nrComponents, 4);
	}
	else
	{
		data = stbi_load(fn.c_str(), &width, &height, &nrComponents, 4);
	}
	ASSERT(data != nullptr,"cannot create texture");
	
	auto tex = createTexture2D(width, height, format,  -1,data, srgb);
	stbi_image_free(data);

	mTextureMap[filename] = tex;
	tex->setName(filename);
	return tex;
}

Renderer::Resource::Ref Renderer::createTexture3D(UINT width, UINT height, UINT depth, DXGI_FORMAT format, UINT miplevels, D3D12_RESOURCE_FLAGS flags, D3D12_HEAP_TYPE type)
{
	ASSERT(width != 0 && height != 0 && depth != 0, "size cannot be zero");
	ASSERT(format != DXGI_FORMAT_UNKNOWN, "format cannot be unknown");

	unsigned long maxmips;
	_BitScanReverse(&maxmips, width | height | depth);
	miplevels = std::min((UINT)maxmips + 1, miplevels);

	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	resdesc.Alignment = 0;
	resdesc.Width = width;
	resdesc.Height = height;
	resdesc.DepthOrArraySize = depth;
	resdesc.MipLevels = miplevels;
	resdesc.Format = format;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resdesc.Flags = flags;

	auto tex = Resource::create();
	tex->init(resdesc, type, D3D12_RESOURCE_STATE_COMMON);

	if ((flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0)
		tex->createShaderResource();

	addResource(tex);
	return tex;
}

Renderer::Resource::Ref Renderer::createTexture2D(UINT width, UINT height, DXGI_FORMAT format, UINT miplevels, const void* data, bool srgb)
{
	auto tex = createTexture2DBase(width, height, 1, format, miplevels,D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE);
	tex->createTexture2D();

	auto size = width * height * D3DHelper::sizeof_DXGI_FORMAT(format);

	if (data == nullptr)
	{
		LOG("recommend to call createTexture instead, createTexture2D is used for the texture with pixel data prepared.");
		return tex;
	}
		
	updateTexture(tex, 0, data, size,srgb);

	generateMips(tex); 
	
	return tex;
}

Renderer::Resource::Ref Renderer::createBufferBase(size_t size, bool isShaderResource, D3D12_HEAP_TYPE type)
{

	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Alignment = 0;
	resdesc.Width = size;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resdesc.Flags = isShaderResource ? D3D12_RESOURCE_FLAG_NONE : D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	auto b = new Buffer();
	auto res = Resource::Ptr(b);
	res->init(resdesc, type, D3D12_RESOURCE_STATE_COMMON);
	addResource(res);

	return Resource::Ref(res);
}

Renderer::Buffer::Ref Renderer::createBuffer(UINT size, UINT stride, bool isShaderResource, D3D12_HEAP_TYPE type, const void* buffer, size_t count)
{

	auto res = createBufferBase(size, isShaderResource,type);

	if (buffer)
	{
		if (type == D3D12_HEAP_TYPE_UPLOAD)
		{
			res->blit(buffer, std::min(count, (size_t)size));
		}
		else
			updateBuffer(res, 0, buffer, size);
	}
	Buffer::Ref b = Resource::Ref(res);
	b->mStride = stride;
	return b;
}

Renderer::ConstantBuffer::Ptr Renderer::createConstantBuffer(UINT size)
{
	auto cb = ConstantBuffer::Ptr(new ConstantBuffer(size, mConstantBufferAllocator));
	return cb;
}

void Renderer::destroyResource(Resource::Ref res)
{
	std::lock_guard<std::mutex> lock(mResourceMutex);
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

Renderer::Resource::Ref Renderer::createResourceView(UINT width, UINT height, DXGI_FORMAT format, ViewType type)
{
	D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_NONE;
	switch (type)
	{
	case Renderer::VT_RENDERTARGET: flag = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; break;
	case Renderer::VT_DEPTHSTENCIL:flag = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; break;
	case Renderer::VT_UNORDEREDACCESS:flag = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; break;
	}
	return createTexture2DBase(width, height, 1, format,1, D3D12_HEAP_TYPE_DEFAULT, flag);
}

Renderer::PipelineState::Ref Renderer::createPipelineState(const std::vector<Shader::Ptr>& shaders, const RenderState& rs)
{
	auto pso = PipelineState::create(rs, shaders);
	mPipelineStates.push_back(pso);
	return pso;
}

Renderer::PipelineState::Ref Renderer::createComputePipelineState(const Shader::Ptr & shader)
{
	auto pso = PipelineState::create(shader);
	mPipelineStates.push_back(pso);
	return pso;
}

Renderer::Profile::Ref Renderer::createProfile()
{
	auto ptr = Profile::create((UINT)mProfiles.size());
	mProfiles.push_back(ptr);
	return ptr;
}

void Renderer::generateMips(Resource::Ref texture)
{
	auto desc = texture->getDesc();

	if (desc.MipLevels == 1)
		return;

	Resource::Ref dst = createTexture2DBase(
		(UINT)desc.Width,
		desc.Height,
		desc.DepthOrArraySize,
		desc.Format,
		desc.MipLevels,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
	);
	dst->createShaderResource();

	executeResourceCommands([=](auto cmdlist){
		cmdlist->transitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
		cmdlist->transitionBarrier(dst, D3D12_RESOURCE_STATE_COPY_DEST, 0, true);
		cmdlist->copyTexture(dst, 0, { 0,0,0 }, texture, 0, nullptr);
		cmdlist->transitionBarrier(dst, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, true);


		for (auto i = 1; i < desc.MipLevels; ++i)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavd = {};
			uavd.Format = desc.Format;
			uavd.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavd.Texture2D.PlaneSlice = 0;
			uavd.Texture2D.MipSlice = i;

			dst->createUnorderedAccessView(&uavd, i);
		}

		for (auto mip = 0; mip < desc.MipLevels - 1; )
		{
			UINT32 width = std::max(1U, (UINT32)desc.Width >> mip);
			UINT32 height = std::max(1U, (UINT32)desc.Height >> mip);

			UINT non_power_of_two = (height & 1) << 1 | (width & 1);
			auto pso = mGenMipsPSO[non_power_of_two];
			cmdlist->setPipelineState(pso);

			pso->setVariable(Shader::ST_COMPUTE, "SrcMipLevel", &mip);

			UINT32 nummips = std::min(4, desc.MipLevels - mip - 1);
			pso->setVariable(Shader::ST_COMPUTE, "NumMipLevels", &nummips);

			UINT32 outputWidth = std::max(1U, width >> 1);
			UINT32 outputHeight = std::max(1U, height >> 1);

			float texelSize[] = {
				1.0f / (float)outputWidth,
				1.0f / (float)outputHeight
			};

			pso->setVariable(Shader::ST_COMPUTE, "TexelSize", &texelSize);

			pso->setResource(Shader::ST_COMPUTE, "SrcMip", dst->getShaderResource());

			for (UINT i = 0; i < nummips; ++i)
				pso->setResource(Shader::ST_COMPUTE, Common::format("OutMip", i + 1), dst->getUnorderedAccess(i + mip + 1));

			cmdlist->dispatch(outputWidth, outputHeight, 1);
			cmdlist->uavBarrier(dst, true);

			mip += nummips;
		}

		cmdlist->transitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST, -1);
		cmdlist->transitionBarrier(dst, D3D12_RESOURCE_STATE_COPY_SOURCE, -1, true);

		cmdlist->copyResource(texture, dst);

		cmdlist->transitionBarrier(texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, -1, true);

	});


	destroyResource(dst);

}


void Renderer::addRenderTask(RenderTask&& task, bool strand, bool impl )
{
	CHECK_RENDER_THREAD;
	auto index = mNumCommandLists.fetch_add(1, std::memory_order_relaxed);
	auto make_task = [t = std::move(task), index, this](){
		mCommandLists[index]->reset();
		auto heap = mDescriptorHeaps[DHT_CBV_SRV_UAV]->get();
		auto cmdlist = mCommandLists[index]->get();
		cmdlist->SetDescriptorHeaps(1, &heap);
		t(mCommandLists[index]);
		cmdlist->Close();
	};
	if (impl)
		make_task();
	else
		mRenderTasks.addTask(make_task, strand);
}


void Renderer::uninitialize()
{
	flushCommandQueue();

	Dispatcher::stop(Dispatcher::getSharedContext());
	//mObjectTaskExecutor.stop();
	//mRenderTaskExecutor.stop();

	// clear all commands
	mProfileCmdAlloc.reset();
	mCommandLists.clear();
	mResourceCommandList.reset();
	mCommandQueue.Reset();
	//mCommandList.reset();
	mQueueFence.reset();

	//clear resources
	mBackbuffers.fill({});
	mResources.clear();
	mPipelineStates.clear();
	mDescriptorHeaps.fill({});
	mResourceBarriers.clear();
	mTimeStampQueryHeap.Reset();
	mProfiles.clear();

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
	auto adapters = getAdapter();
	ComPtr<IDXGIAdapter> adapter;

	for (auto& a : adapters)
	{
		DXGI_ADAPTER_DESC desc;
		a->GetDesc(&desc);
		if (std::wstring(desc.Description).find(L"Intel") == std::string::npos || !adapter)
			adapter = a;
	}

	DXGI_ADAPTER_DESC desc;
	adapter->GetDesc(&desc);
	debugInfoCache.adapter = U2M(desc.Description);
	CHECK(D3D12CreateDevice(adapter.Get(), FEATURE_LEVEL, IID_PPV_ARGS(&mDevice)));

#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(mDevice.As(&infoQueue)))
	{
		D3D12_MESSAGE_SEVERITY severities[] = {
			D3D12_MESSAGE_SEVERITY_INFO,
		};
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, 
			//D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
			//D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			//D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		filter.DenyList.NumIDs = _countof(DenyIds);
		filter.DenyList.pIDList = DenyIds;

		CHECK(infoQueue->PushStorageFilter(&filter));
	}
#endif
}

void Renderer::initCommands()
{
	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		CHECK(mDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&mCommandQueue)));
	}

	//mCommandList = CommandList::create();
	//mCommandList->close();
	mResourceCommandList = CommandList::create();
	mResourceCommandList->close();
	mCurrentFrame = 0;
	mQueueFence = createFence();

	for (auto i = 0; i < NUM_COMMANDLIST; ++i)
	{
		auto cmdlist = CommandList::create();
		mCommandLists.emplace_back(cmdlist);
		cmdlist->close();
		mOriginCommandLists.emplace_back(cmdlist->get());
		cmdlist->get()->SetName(M2U(Common::format("CommandList", i)).c_str());
	}

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

	mDescriptorHeaps[DHT_RENDERTARGET]->get()->SetName(L"Heap RTV");
	mDescriptorHeaps[DHT_DEPTHSTENCIL]->get()->SetName(L"Heap DSV");
	mDescriptorHeaps[DHT_CBV_SRV_UAV]->get()->SetName(L"Heap CBV_SRV_UAV");
}

void Renderer::initProfile()
{
	D3D12_QUERY_HEAP_DESC desc;
	desc.Count = 1024;
	desc.NodeMask = 0;
	desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

	CHECK(mDevice->CreateQueryHeap(&desc, IID_PPV_ARGS(&mTimeStampQueryHeap)));
	mProfileReadBack = createBufferBase(1024 * sizeof(uint64_t),false,D3D12_HEAP_TYPE_READBACK);

}

void Renderer::initResources()
{
	mConstantBufferAllocator = ConstantBufferAllocator::create();

	for (int i = 0; i < 4; ++i)
	{
		std::stringstream ss;
		ss << i;
		auto shader = compileShaderFromFile("shaders/gen_mips.hlsl", "main", SM_CS,{{"NON_OF_POWER", ss.str().c_str()},{NULL,NULL}});
		shader->enable32BitsConstants(true);
		shader->registerStaticSampler({
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			0,0,
			D3D12_COMPARISON_FUNC_NEVER,
			D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
			0,
			D3D12_FLOAT32_MAX,
			0,0,
			D3D12_SHADER_VISIBILITY_ALL
			});
		mGenMipsPSO[i] = createComputePipelineState(shader);
	}

	{
		auto shader = compileShaderFromFile("shaders/srgb_conv.hlsl", "main", SM_CS );
		shader->enable32BitsConstants(true);
		shader->registerStaticSampler({
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			0,0,
			D3D12_COMPARISON_FUNC_NEVER,
			D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
			0,
			D3D12_FLOAT32_MAX,
			0,0,
			D3D12_SHADER_VISIBILITY_ALL
			});
		mSRGBConv = createComputePipelineState(shader);
	}
}

Renderer::Shader::ShaderType Renderer::mapShaderType(const std::string & target)
{
	Shader::ShaderType type = {};
	switch (target[0])
	{
	case 'v': type = Shader::ST_VERTEX; break;
	case 'p': type = Shader::ST_PIXEL; break;
	case 'c': type = Shader::ST_COMPUTE; break;
	case 'g': type = Shader::ST_GEOMETRY; break;
	default:
		ASSERT(false, "unsupported!");
		break;
	}
	return type;
}

void Renderer::collectDebugInfo()
{
	debugInfo.numResources = mResources.size();

	for (auto& r: mResources)
	{
		auto desc = r->get()->GetDesc();
		if (desc.Format != DXGI_FORMAT_UNKNOWN)
			debugInfo.videoMemory += desc.Width * desc.Height * desc.DepthOrArraySize * D3DHelper::sizeof_DXGI_FORMAT(desc.Format);
	}

	debugInfoCache = debugInfo;
}

std::string Renderer::findFile(const std::string & filename)
{
	for (auto& p : mFileSearchPaths)
	{
		auto path = p + filename;
		struct _stat attrs;
		if (_stat(path.c_str(), &attrs) == 0)
		{
			return path;
		}
	}

	return {};
}

void Renderer::commitCommands()
{


#ifndef D3D12ON7
	PROFILE("commit commands", {});


	auto count = mNumCommandLists.load(std::memory_order_relaxed);
	mCommandQueue->ExecuteCommandLists(UINT(count), mOriginCommandLists.data());
	mNumCommandLists.store(0, std::memory_order_relaxed);
#endif
}

void Renderer::fetchNextFrame()
{
#ifndef D3D12ON7
	mCurrentFrame = mSwapChain->GetCurrentBackBufferIndex();
#else
	mCurrentFrame = (mCurrentFrame + 1) % NUM_BACK_BUFFERS;
#endif


}

void Renderer::processTasks()
{
	{
		PROFILE("poll tasks", {});

		auto& cnt = Dispatcher::getSharedContext();

		while (mRenderThread.poll_one() + cnt.poll_one() != 0);

	}

	{
		PROFILE("waitting tasks", {});
		mRenderTasks.wait();
	}
}


ComPtr<Renderer::IDXGIFACTORY> Renderer::getDXGIFactory()
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

std::vector<ComPtr<IDXGIAdapter>> Renderer::getAdapter()
{
	std::vector<ComPtr<IDXGIAdapter>> adapters;
	ComPtr<IDXGIAdapter1> adapter;
	auto factory = getDXGIFactory();

	for (auto i = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, &adapter); ++i)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), FEATURE_LEVEL, _uuidof(ID3D12Device), nullptr)))
		{
			adapters.push_back(adapter);
		}
	}

	return adapters;
}

Renderer::DescriptorHeap::Ref Renderer::getDescriptorHeap(DescriptorHeapType type)
{
	return mDescriptorHeaps[type];
}

void Renderer::addResource(Resource::Ptr res)
{
	std::lock_guard<std::mutex> lock(mResourceMutex);
	mResources.push_back(res);
}
void Renderer::present()
{
	PROFILE("swapchain preset", {});

#if defined(D3D12ON7)
	ComPtr<ID3D12CommandQueueDownlevel> commandQueueDownlevel;

	CHECK(mCommandQueue.As(&commandQueueDownlevel));
	CHECK(commandQueueDownlevel->Present(
		mCommandLists[0]->get(),
		mBackbuffers[mCurrentFrame]->get(),
		mWindow,
		mVSync? D3D12_DOWNLEVEL_PRESENT_FLAG_WAIT_FOR_VBLANK: D3D12_DOWNLEVEL_PRESENT_FLAG_NONE));
#else
	CHECK(mSwapChain->Present(mVSync? 1: 0,0));
#endif

	
}

void Renderer::updateTimeStamp()
{
	PROFILE("udpate timestamp", {});

	if (mProfileCmdAlloc)
	{
		mProfileCmdAlloc->wait();

		UINT64 frequency;
		CHECK(mCommandQueue->GetTimestampFrequency(&frequency));
		double tickdelta = 1000.0 / (double)frequency;

		auto data = mProfileReadBack->map(0);
		struct TimeData
		{
			uint64_t begin;
			uint64_t end;
		};

		for (auto& p : mProfiles)
		{
			TimeData td;
			memcpy(&td, data, sizeof(TimeData));
			data += sizeof(TimeData);

			if (td.begin > td.end)
				continue;

			auto dtime = float(tickdelta * (td.end - td.begin));
			float weight = std::min(1.0f, std::abs(dtime - p->mGPUHistory) * 0.1f);
			p->mGPUHistory = p->mGPUHistory * (1.0f - weight) + dtime *  weight;
		}
		mProfileReadBack->unmap(0);
	}
	else
		mProfileCmdAlloc = CommandAllocator::Ptr(new CommandAllocator);

	mProfileCmdAlloc->reset();
	mResourceCommandList->reset();

	mResourceCommandList->get()->ResolveQueryData(
		mTimeStampQueryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP, 
		0, (UINT)mProfiles.size() * 2,mProfileReadBack->get(),0);

	mResourceCommandList->close();

	ID3D12CommandList* ppCommandLists[] = { mResourceCommandList->get() };
	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	mProfileCmdAlloc->signal();
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

UINT64 Renderer::DescriptorHeap::allocHeap()
{
	UINT stride = 32;
	for (UINT i = 0; i < mUsed.size(); ++i)
	{
		for (UINT j = 0; j < stride; ++j)
		{
			auto index = 1 << j;
			if ((mUsed[i] & index) == 0)
			{
				mUsed[i] |= index;
				return i * stride + j;
			}
		}
	}
	ASSERT(0, " cannot alloc from descriptor heap");
	return {};
}

Renderer::DescriptorHandle Renderer::DescriptorHeap::alloc()
{
	auto pos = allocHeap();
	auto c = mHeap->GetCPUDescriptorHandleForHeapStart();
	auto g= mHeap->GetGPUDescriptorHandleForHeapStart();
	c.ptr += (SIZE_T)(pos * mSize);
	g.ptr += pos * mSize;
	return {pos, c,g};
}

void Renderer::DescriptorHeap::dealloc(DescriptorHandle& handle)
{
	if (!handle.valid())
		return ;
	dealloc(handle.pos);
	handle.reset();
}


void Renderer::DescriptorHeap::dealloc(UINT64 pos)
{
	UINT stride = 32;
	UINT i = UINT(pos / stride);
	UINT j = pos % stride;

	mUsed[i] &= ~(1 << j);
}

ID3D12DescriptorHeap * Renderer::DescriptorHeap::get()
{
	return mHeap.Get();
}

void Renderer::Resource::createRenderTargetView(const D3D12_RENDER_TARGET_VIEW_DESC* desc, UINT index)
{
	auto& texdesc = getDesc();
	ASSERT(texdesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, "type invalid.");
	auto& handle = assignHandle(index, HT_RenderTarget);
	Renderer::getSingleton()->getDevice()->CreateRenderTargetView(get(), desc, handle.cpu);
}

void Renderer::Resource::createDepthStencilView(const D3D12_DEPTH_STENCIL_VIEW_DESC* desc, UINT index)
{
	auto& texdesc = getDesc();
	ASSERT(texdesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, "type invalid.");
	auto& handle = assignHandle(index, HT_DepthStencil);
	Renderer::getSingleton()->getDevice()->CreateDepthStencilView(get(), desc, handle.cpu);
}

void Renderer::Resource::createUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, UINT index)
{
	auto& texdesc = getDesc();
	ASSERT(texdesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, "type invalid.");
	auto& handle = assignHandle(index, HT_UnorderedAccess);
	Renderer::getSingleton()->getDevice()->CreateUnorderedAccessView(get(), nullptr,desc, handle.cpu);
}


void Renderer::Resource::createShaderResource(const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, UINT i)
{
	auto texdesc = getDesc();
	ASSERT((texdesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0, "resource cannot be a SRV");

	auto& handle = assignHandle(i, HT_ShaderResource);
	Renderer::getSingleton()->getDevice()->CreateShaderResourceView(get(), desc, handle.cpu);

}

void Renderer::Resource::createBuffer(DXGI_FORMAT format, UINT64 begin, UINT num, UINT stride, UINT index)
{
	auto desc = getDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = begin;
	srvDesc.Buffer.NumElements = num;
	srvDesc.Buffer.StructureByteStride = stride;
	srvDesc.Buffer.Flags = stride == 0 ? D3D12_BUFFER_SRV_FLAG_NONE : D3D12_BUFFER_SRV_FLAG_RAW;
	createShaderResource(&srvDesc, index);
}


Renderer::DescriptorHandle& Renderer::Resource::assignHandle(UINT i, HandleType type)
{
	DescriptorHeapType htype;
	switch (type)
	{
	case HT_UnorderedAccess:
	case HT_ShaderResource:
		htype = DHT_CBV_SRV_UAV;
		break;
	case HT_RenderTarget:
		htype = DHT_RENDERTARGET;
		break;
	case HT_DepthStencil:
		htype = DHT_DEPTHSTENCIL;
		break;
	default:
		ASSERT(0, "unsupported.");
		break;
	}
	auto& heap = Renderer::getSingleton()->mDescriptorHeaps[htype];
	if (i == -1)
		i = mHandles[type].size();
	if (mHandles[type].size() <= i)
		mHandles[type].resize(i+1);
	auto& handle = mHandles[type][i];
	if (handle.valid())
		heap->dealloc(handle);
	handle = heap->alloc();
	return handle; 
}


Renderer::CommandAllocator::CommandAllocator()
{
	mFence = Renderer::getSingleton()->createFence();
	auto device = Renderer::getSingleton()->getDevice();
	CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mAllocator)));
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
	mResource(res)
{
	mDesc = res->GetDesc();
	mState.resize(mDesc.MipLevels, state);
}

Renderer::Resource::Resource()
{
}

Renderer::Resource::~Resource()
{
	releaseAllHandle();
}

void Renderer::Resource::init(UINT64 size, D3D12_HEAP_TYPE heaptype, DXGI_FORMAT format)
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
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resdesc.Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	init(resdesc, heaptype, D3D12_RESOURCE_STATE_COMMON);
}

void Renderer::Resource::init(const D3D12_RESOURCE_DESC& resdesc, D3D12_HEAP_TYPE ht, D3D12_RESOURCE_STATES state)
{
	if (ht == D3D12_HEAP_TYPE_READBACK)
	{
		state = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	else if (ht == D3D12_HEAP_TYPE_UPLOAD)
	{
		state = D3D12_RESOURCE_STATE_GENERIC_READ;
	}
	else if (resdesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		state = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	else if (resdesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
	else if (resdesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapprop.Type = ht;

	auto device = Renderer::getSingleton()->getDevice();

	D3D12_CLEAR_VALUE* pcv = nullptr;
	D3D12_CLEAR_VALUE cv = {resdesc.Format, {}};

	if (resdesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		pcv = &cv;
	}
	else if (resdesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		cv.DepthStencil = {1.0f, 0};
		pcv = &cv;
	}
	
	cv.DepthStencil = {1.0f, 0};
	CHECK(device->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, state, pcv, IID_PPV_ARGS(&mResource)));

	mDesc = resdesc;
	mState.resize(mDesc.MipLevels, state);
}


void Renderer::Resource::blit(const void* data, UINT64 size, UINT subresource)
{
	D3D12_RANGE readrange = { 0,0 };
	char* dst = 0;
	CHECK(mResource->Map(subresource, &readrange, (void**)&dst));
	memcpy(dst, data, (size_t)size);
	D3D12_RANGE writerange = { 0, (SIZE_T)size };
	mResource->Unmap(subresource, &writerange);
}

char* Renderer::Resource::map(UINT sub)
{
	char* buffer = nullptr;
	D3D12_RANGE range;
	range.Begin = 0;
	range.End = getSize();
	CHECK(mResource->Map(sub,&range,(void**)&buffer));
	return buffer;
}

void Renderer::Resource::unmap(UINT sub)
{
	mResource->Unmap(sub,nullptr);
}

const D3D12_RESOURCE_STATES & Renderer::Resource::getState(UINT sub) const
{
	return mState[sub];
}

void Renderer::Resource::setName(const std::string& name)
{
	std::stringstream ss;
	ss << name << "(" << mResource.Get() << ")";
	mName = ss.str();
	mResource->SetName(M2U(mName).c_str());
}

void Renderer::Resource::releaseAllHandle()
{
	auto renderer = Renderer::getSingleton();
	{
		auto& heap = renderer->mDescriptorHeaps[DHT_CBV_SRV_UAV];
		for (auto&h:mHandles[HT_ShaderResource])
			heap->dealloc(h);
		for (auto&h:mHandles[HT_UnorderedAccess])
			heap->dealloc(h);
		mHandles[HT_ShaderResource].clear();
		mHandles[HT_UnorderedAccess].clear();
	}
	{
		auto& heap = renderer->mDescriptorHeaps[DHT_RENDERTARGET];
		for (auto& h : mHandles[HT_RenderTarget])
			heap->dealloc(h);
		mHandles[HT_RenderTarget].clear();
	}
	{
		auto& heap = renderer->mDescriptorHeaps[DHT_DEPTHSTENCIL];
		for (auto& h : mHandles[HT_DepthStencil])
			heap->dealloc(h);
		mHandles[HT_DepthStencil].clear();
	}
}

D3D12_GPU_VIRTUAL_ADDRESS Renderer::Resource::getVirtualAddress() const
{
	return get()->GetGPUVirtualAddress();
}

Renderer::ViewType Renderer::Resource::getViewType() const
{
	auto& texdesc = getDesc();
	if (texdesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
		return VT_RENDERTARGET;
	if (texdesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		return VT_DEPTHSTENCIL;
	if (texdesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
		return VT_UNORDEREDACCESS;

	return VT_UNKNOWN;
}


const D3D12_GPU_DESCRIPTOR_HANDLE& Renderer::Resource::getShaderResource(UINT i)
{
	return mHandles[HT_ShaderResource][i].gpu;
}

const D3D12_CPU_DESCRIPTOR_HANDLE& Renderer::Resource::getRenderTarget(UINT i)
{
	return mHandles[HT_RenderTarget][i].cpu;
}

const D3D12_CPU_DESCRIPTOR_HANDLE& Renderer::Resource::getDepthStencil(UINT i)
{
	return mHandles[HT_DepthStencil][i].cpu;
}

const D3D12_GPU_DESCRIPTOR_HANDLE& Renderer::Resource::getUnorderedAccess(UINT i)
{
	return mHandles[HT_UnorderedAccess][i].gpu;
}

void Renderer::Resource::init(UINT width, UINT height, D3D12_HEAP_TYPE ht, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags)
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

	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
	Resource::init(resdesc, ht, D3D12_RESOURCE_STATE_COMMON);

}


void Renderer::Resource::createTexture2D(UINT begin, UINT count, UINT i)
{
	auto desc = getDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = begin;
	srvDesc.Texture2D.MipLevels = count;
	createShaderResource(&srvDesc,i);
}

void Renderer::Resource::createTextureCube(UINT begin, UINT count,UINT i)
{
	auto desc = getDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = begin;
	srvDesc.TextureCube.MipLevels = count;
	createShaderResource(&srvDesc, i);
}

void Renderer::Resource::createTextureCubeArray(UINT begin, UINT count, UINT arrayBegin, UINT numCubes, UINT i)
{
	auto desc = getDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
	srvDesc.TextureCubeArray.MostDetailedMip = begin;
	srvDesc.TextureCubeArray.MipLevels = count;
	srvDesc.TextureCubeArray.First2DArrayFace = arrayBegin;
	srvDesc.TextureCubeArray.NumCubes = std::min(numCubes, (UINT)desc.DepthOrArraySize / 6);
	createShaderResource(&srvDesc, i);
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

	if (mFence->GetCompletedValue() != mFenceValue)
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

Renderer::CommandList::CommandList()
{
	auto renderer = Renderer::getSingleton();
	auto device = renderer->getDevice();
	mCurrentAllocator = 0;
	for (auto& a : mAllocators)
		a = CommandAllocator::Ptr(new CommandAllocator);
	CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mAllocators[0]->get(), nullptr, IID_PPV_ARGS(&mCmdList)));
}

Renderer::CommandList::~CommandList()
{
}

void Renderer::CommandList::transitionBarrier(Resource::Ref res, D3D12_RESOURCE_STATES  state, UINT subresource ,bool autoflush)
{
	if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES || state != res->getState(subresource))
	{
		addResourceTransition(res, state, subresource);
	}
	if (autoflush)
		flushResourceBarrier();
}

void Renderer::CommandList::uavBarrier(Resource::Ref res, bool autoflush)
{
	mUAVBarrier[res->get()] = res;

	if (autoflush)
		flushResourceBarrier();
}

void Renderer::CommandList::addResourceTransition(const Resource::Ref& res, D3D12_RESOURCE_STATES state, UINT subres)
{
	//Common::Assert(mResourceTransitions.find(res->get()) == mResourceTransitions.end(), "unexpected.");
	mTransitionBarrier[res->get()] = {res, state, subres};
}

void Renderer::CommandList::flushResourceBarrier()
{

	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	for (auto& t : mTransitionBarrier)
	{
		auto res = t.second.res;
		if (!res)
			continue;
		D3D12_RESOURCE_BARRIER b = {};
		b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		b.Transition.pResource = res->get();
		b.Transition.StateAfter = t.second.state;
		b.Transition.Subresource = t.second.subresource;

		if (t.second.subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
		{
			bool allthesame = true;
			const auto& desc = res->getDesc();
			auto state = res->getState();
			for (UINT i = 1; i < desc.MipLevels; ++i)
			{
				if (res->getState(i) != state)
				{
					allthesame = false;
					break;
				}
			}

			if (allthesame)
			{
				if (state != t.second.state)
				{
					b.Transition.StateBefore = state;
					barriers.emplace_back(b);
				}
			}
			else
			{
				for (UINT i = 0; i < desc.MipLevels; ++i)
				{
					auto substate = res->getState(i);
					if (substate == t.second.state)
						continue;
					b.Transition.StateBefore = res->getState(i);
					b.Transition.Subresource = i;
					barriers.emplace_back(b);
				}
			}

			for (UINT i = 0; i < desc.MipLevels;++i)
				res->mState[i] = t.second.state;
		}
		else
		{
			b.Transition.StateBefore = res->getState(t.second.subresource);
			barriers.emplace_back(b);
			res->mState[t.second.subresource] = t.second.state;
			ASSERT(b.Transition.StateAfter != b.Transition.StateBefore, "cannot be");
		}	
	}

	for (auto& uav : mUAVBarrier)
	{
		if (!uav.second)
			continue;
		D3D12_RESOURCE_BARRIER b;
		b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		b.UAV.pResource = uav.first;
		b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers.emplace_back(b);
	}

	if (!barriers.empty())
		mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());

	mTransitionBarrier.clear();
	mUAVBarrier.clear();
}

void Renderer::CommandList::copyBuffer(Resource::Ref dst, UINT dstStart, Resource::Ref src, UINT srcStart, UINT64 size)
{
	mCmdList->CopyBufferRegion(dst->get(), dstStart, src->get(), srcStart, size);
}

void Renderer::CommandList::copyTexture(Resource::Ref dst, UINT dstSub, const std::array<UINT, 3>& dstStart, Resource::Ref src, UINT srcSub, const D3D12_BOX* srcBox)
{
	bool dstIsBuffer = dst->getDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;
	bool srcIsBuffer = src->getDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_TEXTURE_COPY_LOCATION dstlocal = {dst->get(),dstIsBuffer? D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT:  D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX   , dstSub};
	D3D12_TEXTURE_COPY_LOCATION srclocal = {src->get(), srcIsBuffer ? D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT : D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX , srcSub};
	const auto& srcDesc = src->getDesc();
	const auto& dstDesc = dst->getDesc();

	UINT numRow = 0;
	UINT64 rowSize = 0;
	UINT64 totalSize = 0;
	Renderer::getSingleton()->getDevice()->GetCopyableFootprints(&dstDesc,dstSub,1,0, &srclocal.PlacedFootprint,&numRow,&rowSize,&totalSize);

	
	mCmdList->CopyTextureRegion(&dstlocal, dstStart[0], dstStart[1], dstStart[2],&srclocal,(const D3D12_BOX*)srcBox);
}

void Renderer::CommandList::copyResource(const Resource::Ref& dst, const Resource::Ref& src)
{
	mCmdList->CopyResource(dst->get(), src->get());
}

void Renderer::CommandList::discardResource(const Resource::Ref & rt)
{
	mCmdList->DiscardResource(rt->get(),nullptr);
}

void Renderer::CommandList::clearRenderTarget(const Resource::Ref & rt, const Color & color)
{
	mCmdList->ClearRenderTargetView(rt->getRenderTarget(), color.data(),0, nullptr);
}

void Renderer::CommandList::clearDepth(const Resource::Ref& rt, float depth)
{
	mCmdList->ClearDepthStencilView(rt->getDepthStencil(), D3D12_CLEAR_FLAG_DEPTH,depth, 0,0, 0);
}

void Renderer::CommandList::clearStencil(const Resource::Ref & rt, UINT8 stencil)
{
	mCmdList->ClearDepthStencilView(rt->getDepthStencil(), D3D12_CLEAR_FLAG_STENCIL, 1.0f, stencil, 0, 0);
}

void Renderer::CommandList::clearDepthStencil(const Resource::Ref & rt, float depth, UINT8 stencil)
{
	mCmdList->ClearDepthStencilView(rt->getDepthStencil(), D3D12_CLEAR_FLAG_STENCIL | D3D12_CLEAR_FLAG_DEPTH, depth, stencil, 0, 0);
}

void Renderer::CommandList::setViewport(const D3D12_VIEWPORT& vp)
{
	mCmdList->RSSetViewports(1, &vp);
}

void Renderer::CommandList::setViewportToScreen()
{
	auto size = Renderer::getSingleton()->getSize();
	setViewport({0,0, (float)size[0],(float)size[1],0.0f, 1.0f});
}

void Renderer::CommandList::setScissorRect(const D3D12_RECT& rect)
{
	mCmdList->RSSetScissorRects(1, &rect);
}

void Renderer::CommandList::setScissorRectToScreen()
{
	auto size = Renderer::getSingleton()->getSize();
	setScissorRect({0,0, size[0],size[1]});
}

void Renderer::CommandList::setRenderTarget(const Resource::Ref& rt, const Resource::Ref& ds)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE* dshandle = 0;
	if (ds)
		dshandle = & (ds->getDepthStencil());

	//Common::Assert(rt->getState() == D3D12_RESOURCE_STATE_RENDER_TARGET, "need transition to rendertarget");
	setRenderTargets({rt->getRenderTarget()}, dshandle);

}

void Renderer::CommandList::setRenderTargets(const std::vector<Resource::Ref>& rts, const Resource::Ref & ds)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE* dshandle = 0;
	if (ds)
		dshandle = &(ds->getDepthStencil());

	std::vector< D3D12_CPU_DESCRIPTOR_HANDLE> rtvs = {};
	for (auto& rt: rts)
	{
		if (rt)
		{
			//Common::Assert(rt->getState() == D3D12_RESOURCE_STATE_RENDER_TARGET, "need rt");
			rtvs.push_back(rt->getRenderTarget());
		}
		else
		{
			rtvs.push_back({});
		}
	}
	setRenderTargets(rtvs, dshandle);
}

void Renderer::CommandList::setRenderTargets(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& rts,const D3D12_CPU_DESCRIPTOR_HANDLE* ds)
{
	mCmdList->OMSetRenderTargets((UINT)rts.size(), rts.data(), FALSE,ds);
}
		

void Renderer::CommandList::setPipelineState(PipelineState::Ref ps)
{
	auto renderer = Renderer::getSingleton();
	mCurrentPipelineState = ps;
	mCmdList->SetPipelineState(ps->get());

	if (ps->getType() == PipelineState::PST_Graphic)
		mCmdList->SetGraphicsRootSignature(ps->getRootSignature());
	else
		mCmdList->SetComputeRootSignature(ps->getRootSignature());
}

void Renderer::CommandList::setVertexBuffer(const std::vector<Buffer::Ref>& vertices)
{
	std::vector<D3D12_VERTEX_BUFFER_VIEW> views;
	for (auto& v: vertices)
		views.push_back({v->getVirtualAddress(), v->getSize(), v->getStride()});
	if (!views.empty())
		mCmdList->IASetVertexBuffers(0,(UINT)views.size(), views.data());
}

void Renderer::CommandList::setVertexBuffer(const Buffer::Ref& vertices)
{
	D3D12_VERTEX_BUFFER_VIEW view = {vertices->getVirtualAddress(), vertices->getSize(), vertices->getStride()};
	mCmdList->IASetVertexBuffers(0, 1, &view);
}

void Renderer::CommandList::setIndexBuffer(const Buffer::Ref& indices)
{
	D3D12_INDEX_BUFFER_VIEW view = {indices->getVirtualAddress(), indices->getSize(), indices->getStride() == 2? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT };
	mCmdList->IASetIndexBuffer(&view);
}

void Renderer::CommandList::setPrimitiveType(D3D_PRIMITIVE_TOPOLOGY type)
{
	mCmdList->IASetPrimitiveTopology(type);
}

void Renderer::CommandList::setRootDescriptorTable(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE & handle)
{
	mCmdList->SetGraphicsRootDescriptorTable(slot, handle);
}

void Renderer::CommandList::setComputeRootDescriptorTable(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE & handle)
{
	mCmdList->SetComputeRootDescriptorTable(slot, handle);
}

void Renderer::CommandList::set32BitConstants(UINT slot, UINT num, const void* data, UINT offset)
{
	mCmdList->SetGraphicsRoot32BitConstants(slot, num, data, offset);
}

void Renderer::CommandList::setCompute32BitConstants(UINT slot, UINT num, const void* data, UINT offset)
{
	mCmdList->SetComputeRoot32BitConstants(slot, num, data, offset);
}

void Renderer::CommandList::drawInstanced(UINT vertexCount, UINT instanceCount, UINT startVertex, UINT startInstance)
{
	mCurrentPipelineState->setRootDescriptorTable(this);

	debugInfo.drawcallCount++;
	debugInfo.primitiveCount+= vertexCount / 3 * instanceCount;
	mCmdList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void Renderer::CommandList::drawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndex, INT startVertex, UINT startInstance)
{
	mCurrentPipelineState->setRootDescriptorTable(this);

	debugInfo.drawcallCount++;
	debugInfo.primitiveCount += indexCountPerInstance / 3 * instanceCount;
	mCmdList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndex,startVertex,startInstance);
}

void Renderer::CommandList::dispatch(UINT x, UINT y, UINT z)
{
	mCurrentPipelineState->setRootDescriptorTable(this);

	mCmdList->Dispatch(x,y,z);
}

void Renderer::CommandList::endQuery(ComPtr<ID3D12QueryHeap> queryheap, D3D12_QUERY_TYPE type, UINT queryidx)
{
	mCmdList->EndQuery(queryheap.Get(),type, queryidx);
}

void Renderer::CommandList::close()
{
	CHECK(mCmdList->Close());
}

void Renderer::CommandList::reset()
{
	mAllocators[mCurrentAllocator]->signal();
	//r->recycleCommandAllocator(mAllocator);
	mCurrentAllocator = (mCurrentAllocator + 1) % NUM_ALLOCATORS;
	auto& a = mAllocators[mCurrentAllocator];
	a->reset();
	CHECK(mCmdList->Reset(a->get(), nullptr));
}

ID3D12GraphicsCommandList * Renderer::CommandList::get()
{
	return mCmdList.Get();
}


const Renderer::RenderState Renderer::RenderState::Default([](Renderer::RenderState& self) {
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
		D3D12_CULL_MODE_NONE,
		FALSE,
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		FALSE,
		FALSE,
		FALSE,
		0,
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	});

	self.setRenderTargetFormat({ FRAME_BUFFER_FORMAT });
	self.setSample(1,0);
});

const Renderer::RenderState Renderer::RenderState::GeneralSolid([](Renderer::RenderState& self) {
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
	self.setDepthStencil({
		TRUE,
		D3D12_DEPTH_WRITE_MASK_ALL,
		D3D12_COMPARISON_FUNC_LESS,
		FALSE,
		0,
		0,
		{},
		{}
	});
	self.setDepthStencilFormat(DXGI_FORMAT_D24_UNORM_S8_UINT);
	self.setPrimitiveType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	self.setRasterizer({
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_NONE,
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

	self.setRenderTargetFormat({ FRAME_BUFFER_FORMAT });
	self.setSample(1, 0);
});


Renderer::RenderState::RenderState(std::function<void(RenderState&)> initializer)
{
	initializer(*this);
}

Renderer::RenderState::RenderState(DXGI_FORMAT targetfmt)
{
	*this = RenderState::Default;
	mRTFormats = {targetfmt};
}

Renderer::Shader::Shader(const MemoryData& data, ShaderType type) :
	mCodeBlob(data), mType(type)
{
}

void Renderer::Shader::registerStaticSampler( const D3D12_STATIC_SAMPLER_DESC& desc)
{
	mStaticSamplers.push_back( desc);
}

void Renderer::Shader::registerStaticSampler(const std::string& name, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE mode)
{
	mSamplerMap[name] = {
		filter,
		mode, 
		mode,
		mode,
		0,0,
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		0,
		D3D12_FLOAT32_MAX,
		0,0,
		D3D12_SHADER_VISIBILITY_ALL,
	};
}

void Renderer::Shader::enable32BitsConstants(bool b)
{
	mUse32BitsConstants = b;
}

void Renderer::Shader::enable32BitsConstantsByName(const std::string& name)
{
	mUse32BitsConstantsSet.insert(name);
}

void Renderer::Shader::enableStaticSampler(bool b)
{
	mUseStaticSamplers = b;
}

D3D12_SHADER_VISIBILITY Renderer::Shader::getShaderVisibility() const
{
	switch (mType)
	{
	case Shader::ST_VERTEX: return  D3D12_SHADER_VISIBILITY_VERTEX;
	case Shader::ST_HULL: return D3D12_SHADER_VISIBILITY_HULL;
	case Shader::ST_DOMAIN: return D3D12_SHADER_VISIBILITY_DOMAIN;
	case Shader::ST_GEOMETRY: return D3D12_SHADER_VISIBILITY_GEOMETRY;
	case Shader::ST_PIXEL: return D3D12_SHADER_VISIBILITY_PIXEL; 
	case Shader::ST_COMPUTE: break;
	default:
		ASSERT(0,"unknown type");
		break;
	}
	return D3D12_SHADER_VISIBILITY_ALL;
}

D3D12_DESCRIPTOR_RANGE_TYPE Renderer::Shader::getRangeType(D3D_SHADER_INPUT_TYPE type) const
{
	switch (type)
	{
	case D3D_SIT_CBUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	case D3D_SIT_TEXTURE: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	case D3D_SIT_SAMPLER: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	case D3D_SIT_UAV_RWTYPED: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	default:
		ASSERT(0, "unsupported shader input type");
		return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	}
}

void Renderer::Shader::createRootParameters()
{
	mRootParameters.clear();
	mRanges.clear();
	auto endsamplers = mSamplerMap.end();

	CHECK(D3DReflect(mCodeBlob->data(), mCodeBlob->size(), IID_PPV_ARGS(&mReflection)));

	ShaderDesc shaderdesc;
	mReflection->GetDesc(&shaderdesc);
	mRanges.resize(shaderdesc.BoundResources);
	
	for (UINT i = 0; i < shaderdesc.BoundResources; ++i)
	{
		ShaderInputBindDesc desc;
		mReflection->GetResourceBindingDesc(i,&desc);

		auto type = getRangeType(desc.Type);
		UINT slot = (UINT)mRootParameters.size();

		if (!(desc.Type == D3D_SIT_SAMPLER && mUseStaticSamplers))
		{
			mRootParameters.push_back({});
			auto& rootparam = mRootParameters.back();
			rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootparam.ShaderVisibility = getShaderVisibility();
			rootparam.DescriptorTable.NumDescriptorRanges = 1;

			rootparam.DescriptorTable.pDescriptorRanges = &mRanges[i];

			auto& range = mRanges[i];
			range.RangeType = type;
			range.BaseShaderRegister = desc.BindPoint;
			range.NumDescriptors = desc.BindCount;
			range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			UINT space = 0;
#ifndef D3D12ON7
			space = desc.Space;
#endif
			range.RegisterSpace = space;
		}



		switch (type)
		{
		case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
			{
				auto& rootparam = mRootParameters.back();
				auto& range = mRanges[i];

				auto cbuffer = mReflection->GetConstantBufferByName(desc.Name);
				ShaderBufferDesc bd;
				cbuffer->GetDesc(&bd);

				CBuffer* cbuffers;

				if (mUse32BitsConstants || 
					mUse32BitsConstantsSet.find(bd.Name) != mUse32BitsConstantsSet.end())
				{
					cbuffers = &mSemanticsMap.cbuffersBy32Bits[bd.Name];

					rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
					rootparam.Constants.Num32BitValues = bd.Size / 4;
					rootparam.Constants.ShaderRegister = desc.BindPoint;
					rootparam.Constants.RegisterSpace = range.RegisterSpace;

				}
				else
					cbuffers = &mSemanticsMap.cbuffers[bd.Name];

				cbuffers->size = bd.Size;
				cbuffers->slot = slot;
				for (UINT j = 0; j < bd.Variables; ++j)
				{
					auto var = cbuffer->GetVariableByIndex(j);
					ShaderVariableDesc vd;
					var->GetDesc(&vd);
					cbuffers->variables[vd.Name] = { vd.StartOffset, vd.Size };
				}

			}
			break;
		case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
			mSemanticsMap.textures[desc.Name] = slot;
			mSemanticsMap.texturesBySlot[desc.BindPoint] = slot;
			break;
		case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
			mSemanticsMap.uavs[desc.Name] = slot;
			break;
		case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
			mSemanticsMap.samplers[desc.Name] =  slot;
			if (mUseStaticSamplers)
			{
				auto ret = mSamplerMap.find(desc.Name);
				if (ret != endsamplers)
				{
					ret->second.RegisterSpace = desc.Space;
					ret->second.ShaderRegister = desc.BindPoint;
					mStaticSamplers.push_back(ret->second);
				}
			}
			break;
		}
	}




}

Renderer::PipelineState::PipelineState(const RenderState & rs, const std::vector<Shader::Ptr>& shaders)
{
	mType = PST_Graphic;
	auto device = Renderer::getSingleton()->getDevice();

	D3D12_ROOT_SIGNATURE_DESC rsd = {};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

	std::vector<D3D12_ROOT_PARAMETER> params;
	std::vector< D3D12_STATIC_SAMPLER_DESC> samplers;
	UINT offset = 0;
	for (auto& s : shaders)
	{
		s->createRootParameters();
		samplers.insert(samplers.end(), s->mStaticSamplers.begin(), s->mStaticSamplers.end());
		params.insert(params.end(), s->mRootParameters.begin(), s->mRootParameters.end());
		s->mSemanticsMap.offset = offset;
		offset += (UINT)s->mRootParameters.size();
		mSemanticsMap[s->mType] = s->mSemanticsMap;
		switch (s->mType)
		{
		case Shader::ST_VERTEX: {
			rsd.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; 
			desc.VS = {s->mCodeBlob->data(), (UINT)s->mCodeBlob->size()}; 
			break;}
		case Shader::ST_HULL:{
			desc.HS = { s->mCodeBlob->data(), (UINT)s->mCodeBlob->size()}; 
			break;}
		case Shader::ST_DOMAIN:{
			desc.DS = { s->mCodeBlob->data(), (UINT)s->mCodeBlob->size() }; 
			break;}
		case Shader::ST_GEOMETRY: {
			desc.GS = { s->mCodeBlob->data(), (UINT)s->mCodeBlob->size() }; 
			break; }
		case Shader::ST_PIXEL: {
			desc.PS = { s->mCodeBlob->data(), (UINT)s->mCodeBlob->size() }; 
			break; }
		default:
			break;
		}
	}

	UINT rootsigSize = 0;
	for (auto& p : params)
	{
		switch (p.ParameterType)
		{
		case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: rootsigSize += 1; break;
		case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: rootsigSize += p.Constants.Num32BitValues; break;
		case D3D12_ROOT_PARAMETER_TYPE_CBV : 
		case D3D12_ROOT_PARAMETER_TYPE_SRV :
		case D3D12_ROOT_PARAMETER_TYPE_UAV :
			rootsigSize += 2;
			break;
		}
	}

	ASSERT(rootsigSize <= 64, "Root Signature size exceeds maximum of 64 32-bit units.");

	rsd.NumParameters = (UINT)params.size();
	if (rsd.NumParameters > 0)
		rsd.pParameters = params.data();
	rsd.NumStaticSamplers = (UINT)samplers.size();
	if (rsd.NumStaticSamplers > 0)
		rsd.pStaticSamplers = samplers.data();

	ComPtr<ID3D10Blob> blob;
	ComPtr<ID3D10Blob> err;
	if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1,&blob,&err)))
	{
		ASSERT(false,((const char*)err->GetBufferPointer()));
	}
	CHECK(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	

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
	desc.SampleDesc = rs.mSample;
	
	CHECK(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineState)));

	mDesc = desc;
}

Renderer::PipelineState::PipelineState(const Shader::Ptr & shader)
{
	mType = PST_Compute;
	auto device = Renderer::getSingleton()->getDevice();

	D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {0};
	D3D12_ROOT_SIGNATURE_DESC rsd = {};

	shader->createRootParameters();
	rsd.NumParameters = (UINT)shader->mRootParameters.size();
	if (rsd.NumParameters > 0)
		rsd.pParameters = shader->mRootParameters.data();
	rsd.NumStaticSamplers = (UINT)shader->mStaticSamplers.size();
	if (rsd.NumStaticSamplers > 0)
		rsd.pStaticSamplers = shader->mStaticSamplers.data();

	ComPtr<ID3D10Blob> blob;
	ComPtr<ID3D10Blob> err;
	if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err)))
	{
		ASSERT(false, ((const char*)err->GetBufferPointer()));
	}

	CHECK(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	
	desc.pRootSignature = mRootSignature.Get();
	desc.CS = { shader->mCodeBlob->data(), (UINT)shader->mCodeBlob->size() };
	
	CHECK(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&mPipelineState)));

	mSemanticsMap[shader->mType] = shader->mSemanticsMap;
}

Renderer::PipelineState::~PipelineState()
{
}

void Renderer::PipelineState::setResource(Shader::ShaderType type, const std::string & name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle)
{
	auto& textures = mSemanticsMap[type].textures;
	auto ret = textures.find(name);
	//ASSERT(ret != textures.end(), "specify texture name is not existed.");
	if (ret == textures.end())
	{ 
		auto& uavs = mSemanticsMap[type].uavs;
		auto uav = uavs.find(name);
		if (uav == uavs.end())
		{
			LOG(name , " is not a bound resource in shader" );
			return;
		}
		mTextures[type][uav->second + mSemanticsMap[type].offset] = handle;
	}
	else
		mTextures[type][ret->second + mSemanticsMap[type].offset] = handle;
}

void Renderer::PipelineState::setVSResource( const std::string & name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle)
{
	setResource(Shader::ST_VERTEX, name, handle);
}

void Renderer::PipelineState::setPSResource( const std::string & name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle)
{
	setResource(Shader::ST_PIXEL, name, handle);
}

void Renderer::PipelineState::setCSResource(const std::string& name, const D3D12_GPU_DESCRIPTOR_HANDLE& handle)
{
	setResource(Shader::ST_COMPUTE, name, handle);
}

void Renderer::PipelineState::setResource(Shader::ShaderType type, UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE & handle)
{
	auto& textures = mSemanticsMap[type].texturesBySlot;
	auto ret = textures.find(slot);
	if (ret == textures.end())
		return;
	mTextures[type][ret->second + mSemanticsMap[type].offset] = handle;
}

void Renderer::PipelineState::setVSResource(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE & handle)
{
	return setResource(Shader::ST_VERTEX, slot, handle);
}

void Renderer::PipelineState::setPSResource(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE & handle)
{
	return setResource(Shader::ST_PIXEL, slot, handle);
}

void Renderer::PipelineState::setCSResource(UINT slot, const D3D12_GPU_DESCRIPTOR_HANDLE& handle)
{
	return setResource(Shader::ST_COMPUTE, slot, handle);
}

void Renderer::PipelineState::setConstant(Shader::ShaderType type, const std::string & name, const ConstantBuffer::Ptr& c)
{
	auto& cbuffers = mSemanticsMap[type].cbuffers;
	auto ret = cbuffers.find(name);
	if (ret == cbuffers.end())
		return;
	//ASSERT(ret != cbuffers.end(), "cannot find specify cbuffer at setConstant");

	mCBuffers[type][ret->second.slot + mSemanticsMap[type].offset] = c->getHandle();
}

void Renderer::PipelineState::setVSConstant(const std::string & name, const ConstantBuffer::Ptr& c)
{
	setConstant(Shader::ST_VERTEX, name, c);
}

void Renderer::PipelineState::setPSConstant(const std::string & name, const ConstantBuffer::Ptr& c)
{
	setConstant(Shader::ST_PIXEL, name, c);
}

void Renderer::PipelineState::setCSConstant(const std::string& name, const ConstantBuffer::Ptr& c)
{
	setConstant(Shader::ST_COMPUTE, name, c);
}

void Renderer::PipelineState::setVariable(Shader::ShaderType type, const std::string & name, const void * data)
{
	auto& cbuffers = mSemanticsMap[type].cbuffersBy32Bits;
	for (auto& cb : cbuffers)
	{
		auto ret = cb.second.variables.find(name);
		if (ret == cb.second.variables.end())
			continue;

		auto& buffer = mCBuffersBy32Bits[type][cb.second.slot + mSemanticsMap[type].offset];
		buffer.resize(cb.second.size);
		memcpy(buffer.data() + ret->second.offset, data, ret->second.size );
		return;
	}

	LOG("undefined variable ",name);
}

void Renderer::PipelineState::setVSVariable(const std::string & name, const void * data)
{
	setVariable(Shader::ST_VERTEX, name, data);
}

void Renderer::PipelineState::setPSVariable(const std::string & name, const void * data)
{
	setVariable(Shader::ST_PIXEL, name, data);
}

Renderer::ConstantBuffer::Ptr Renderer::PipelineState::createConstantBuffer(Shader::ShaderType type,const std::string& name)
{
	auto renderer = Renderer::getSingleton();

	auto shader = mSemanticsMap.find(type);
	ASSERT(shader != mSemanticsMap.end(), "cannot find specify shader");
	
	auto& cbuffers = shader->second.cbuffers;
	auto cbuffer = cbuffers.find(name);
	if (cbuffer == cbuffers.end())
	{
		ASSERT(shader != mSemanticsMap.end(), "cannot find specify shader");
		return {};
	}
	auto cb = renderer->createConstantBuffer(cbuffer->second.size);
	cb->setReflection(cbuffer->second.variables);
	return cb;
}

const D3D12_GRAPHICS_PIPELINE_STATE_DESC& Renderer::PipelineState::getDesc() const
{
	return mDesc;
}

void Renderer::PipelineState::setRootDescriptorTable(CommandList * cmdlist)
{
	using SetRDT = void(CommandList::*)(UINT, const D3D12_GPU_DESCRIPTOR_HANDLE&);
	SetRDT setrdt;
	using Set32Bits = void(CommandList::*)(UINT, UINT, const void*, UINT);
	Set32Bits set32bits;
	if (mType == PST_Graphic)
	{
		setrdt = &CommandList::setRootDescriptorTable;
		set32bits = &CommandList::set32BitConstants;
	}
	else
	{
		setrdt = &CommandList::setComputeRootDescriptorTable;
		set32bits = &CommandList::setCompute32BitConstants;
	}
	for (auto&texs : mTextures)
	{
		for (auto& t : texs.second)
		{
			(cmdlist->*setrdt)(t.first,t.second);
		}
	}

	Renderer::getSingleton()->mConstantBufferAllocator->sync();

	for (auto& cbs : mCBuffers)
	{
		for (auto& cb : cbs.second)
		{
			(cmdlist->*setrdt)(cb.first, cb.second);
		}
	}

	for (auto& cbs : mCBuffersBy32Bits)
	{
		for (auto& cb : cbs.second)
		{
			UINT size = (UINT)cb.second.size();
			UINT count = size / 4;
			if (count != 0)
				(cmdlist->*set32bits)(cb.first, count, cb.second.data(),0);
		}
	}
}


Renderer::Profile::Profile(UINT index)
{
	mIndex = index;
}

float Renderer::Profile::getCPUTime()
{
	return mCPUHistory;
}

float Renderer::Profile::getGPUTime()
{
	return mGPUHistory;
}

void Renderer::Profile::begin(Renderer::CommandList::Ref cl)
{
	auto renderer = Renderer::getSingleton();
	if (cl)
		cl->endQuery(
			renderer->mTimeStampQueryHeap,
			D3D12_QUERY_TYPE_TIMESTAMP,
			mIndex * 2);

	mCPUTime = std::chrono::high_resolution_clock::now();
}

void Renderer::Profile::end(Renderer::CommandList::Ref cl)
{
	float dtime = float((double)(std::chrono::high_resolution_clock::now() - mCPUTime).count() / 1000000.0);
	float weight = std::min(1.0f, std::abs(dtime - mCPUHistory) * 0.1f);
	mCPUHistory = mCPUHistory * (1 - weight) + dtime * ( weight);

	auto renderer = Renderer::getSingleton();
	if (cl)
		cl->endQuery(
			renderer->mTimeStampQueryHeap,
			D3D12_QUERY_TYPE_TIMESTAMP,
			mIndex * 2 + 1);
}


Renderer::ConstantBufferAllocator::ConstantBufferAllocator()
{
	mResource = Renderer::getSingleton()->createBufferBase(cache_size,false,D3D12_HEAP_TYPE_UPLOAD);
	mEnd = mCache.data();
}

UINT64 Renderer::ConstantBufferAllocator::alloc(UINT64 size)
{
	size = ALIGN(size, 256);
	auto count = size / 256;

	auto beg = count;
	while (mFree.size() > beg)
	{
		if (mFree[beg].empty())
			beg++;
		else
		{
			auto free = mFree[beg].back();
			mFree[beg].pop_back();

			auto res = beg - count;
			if (res != 0)
			{
				mFree[res].push_back(free + res * 256);
			}

			return  free;
		}
	}

	auto free = mEnd;
	mEnd += count * 256;
	ASSERT(mEnd <= mCache.data() + mCache.size(), "not enough constant buffer space");
	return free - mCache.data() ;
}

void Renderer::ConstantBufferAllocator::dealloc(UINT64 address, UINT64 size)
{
	auto count = size / 256;

	if (mFree.size() <= count)
	{
		mFree.resize(count + 1);
	}

	mFree[count].push_back(address);
}

D3D12_GPU_VIRTUAL_ADDRESS Renderer::ConstantBufferAllocator::getGPUVirtualAddress()
{
	return mResource->get()->GetGPUVirtualAddress();
}

void Renderer::ConstantBufferAllocator::blit(UINT64 offset, const void * buffer, UINT64 size)
{
	memcpy(mCache.data() + offset, buffer, size);
	mRefresh = true;
}

void Renderer::ConstantBufferAllocator::sync()
{
	if (!mRefresh)
		return;
	void* buff = mResource->map(0);

	memcpy(buff, mCache.data(), mEnd - mCache.data());

	mResource->unmap(0);
}

Renderer::ConstantBuffer::ConstantBuffer(size_t size, ConstantBufferAllocator::Ref allocator):
	mAllocator(allocator)
{
	const size_t minSizeRequired = 256;
	mSize = ALIGN(size, 256);

	mOffset = allocator->alloc(mSize);

	mView = Renderer::getSingleton()->getDescriptorHeap(DHT_CBV_SRV_UAV)->alloc();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbd = {};
	cbd.BufferLocation = allocator->getGPUVirtualAddress() + mOffset;
	cbd.SizeInBytes = (UINT)mSize;
	Renderer::getSingleton()->getDevice()->CreateConstantBufferView(&cbd, mView.cpu);
}

Renderer::ConstantBuffer::~ConstantBuffer()
{
	if (mAllocator)
		mAllocator->dealloc(mOffset, mSize);
}

void Renderer::ConstantBuffer::setReflection(const std::map<std::string, Shader::Variable>& rft)
{
	mVariables = rft;
}

void Renderer::ConstantBuffer::setVariable(const std::string& name,const void* data, size_t size)
{
	auto ret = mVariables.find(name);
	if (ret == mVariables.end())
		return;


	ASSERT(size == ret->second.size, "size is not matched");
	blit(data, ret->second.offset, ret->second.size );
}

void Renderer::ConstantBuffer::blit(const void * buffer, UINT64 offset , UINT64 size)
{
	mAllocator->blit(mOffset + offset, buffer, std::min(size, mSize));
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::ConstantBuffer::getHandle() const
{
	return mView.gpu;
}

