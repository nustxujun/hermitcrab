#pragma once 

#include "Common.h"
#include "Renderer.h"

class ResourceHandle
{
public:
	using Ptr = std::shared_ptr<ResourceHandle>;

	static ResourceHandle::Ptr create(Renderer::ViewType type, int w, int h, DXGI_FORMAT format);

	ResourceHandle(Renderer::ViewType t, int w, int h, DXGI_FORMAT format);

	Renderer::ViewType getType()const;
	const std::wstring& getName()const;
	void setName(const std::wstring& n);
	void setClearColor(const Color& vec){mClearColor = vec;};
	const Color& getClearColor()const{return mClearColor;};

	void addRef();
	void release();

	void prepare();
	const Renderer::ResourceView::Ptr& getView() const {return mView;}
private:
	size_t mRefCount = 0;
	std::wstring mName;
	Renderer::ViewType mType;
	int mWidth;
	int mHeight;
	DXGI_FORMAT mFormat;
	Vector4 mClearValue = {};
	Renderer::ResourceView::Ptr mView;
	Color mClearColor = {};
};


class RenderGraph
{
public:

	class RenderPassVector;

	class Resources
	{
	public:
		ResourceHandle::Ptr operator[](size_t index)const;
		ResourceHandle::Ptr operator[](const std::wstring& str)const;
		ResourceHandle::Ptr find(Renderer::ViewType type, size_t index = 0)const;
		ResourceHandle::Ptr getRenderTarget(size_t index = 0) const;

		void add(ResourceHandle::Ptr res, UINT slot = 0);
		void clear();
		size_t size()const{return mResources.size();}

		std::vector<ResourceHandle::Ptr>::iterator begin(){return mResources.begin(); }
		std::vector<ResourceHandle::Ptr>::iterator end() { return mResources.end(); }
	private:
		std::vector<ResourceHandle::Ptr> mResources;
		std::unordered_map<std::wstring, ResourceHandle::Ptr> mResourceMap;
	};

	using Inputs = std::vector<const Resources*>;

	class RenderPass
	{
		friend class RenderGraph;
	public:
		using Ptr = std::shared_ptr<RenderPass>;
		enum InitialType
		{
			IT_NONE,
			IT_CLEAR,
			IT_DISCARD,
		};

		virtual void setup() = 0;
		virtual void compile(const Inputs& inputs) = 0;
		virtual void execute() = 0;

		virtual RenderPass& operator >> (RenderPass::Ptr pass);
		virtual RenderPass& operator >> (RenderPass& pass);

		void read(ResourceHandle::Ptr res, UINT slot = 0);
		void write(ResourceHandle::Ptr res, InitialType type = IT_CLEAR, UINT slot = 0);

		void visitOutputs(const std::function<void(RenderPass*)> & visitor);
		bool isPrepared()const;
		void clear();
	protected:
		virtual void addInput(RenderPass* p);
		virtual void addOutput(RenderPass* p);

		void prepareResources();

	protected:
		std::vector<ResourceHandle::Ptr> mShaderResources;
		Resources mRenderTargets;
		std::vector<InitialType> mInitialTypes;

		std::vector<RenderPass*> mInputs;
		std::vector<RenderPass*> mOutputs;
	};

	class LambdaRenderPass final : public RenderPass
	{
	public:
		LambdaRenderPass(LambdaRenderPass&& lrp);

		using SetupFunc = std::function <void(RenderPass*)>;
		using CompileFunc = std::function <void(RenderPass*, const Inputs&)>;
		using ExecuteFunc = std::function <void(void)>;
		LambdaRenderPass(const SetupFunc& setup, const CompileFunc& compile, const ExecuteFunc& exe);
		
		void setup() override;
		void compile(const Inputs& inputs) override;
		void execute()override;
	private:
		SetupFunc mSetup;
		CompileFunc mCompile;
		ExecuteFunc mExecute;
	};

	class RenderPassVector final:public RenderPass
	{
	public:
		using Ptr = std::shared_ptr<RenderPassVector>;
	private:
		std::vector<RenderPass> mPasses;
	};

	class BeginPass final :public RenderPass
	{
	public:
		BeginPass();
		void setup()override {};
		void compile(const Inputs& inputs) override;
		void execute() override{};
	private:
		ResourceHandle::Ptr mRenderTarget;
	};


public:

	RenderPass& begin();
	void setup();
	void compile();
	void execute();
private:
	std::vector<BeginPass> mPasses;
	std::vector<RenderPass*> mCurrents;
};

