#pragma once 

#include "Common.h"
#include "Renderer.h"

class ResourceHandle
{
public:
	using Ptr = std::shared_ptr<ResourceHandle>;
	union ClearValue
	{
		Color color;
		struct {
			float depth;
			UINT8 stencil;
		};
	};

	static ResourceHandle::Ptr create(Renderer::ViewType type, int w, int h, DXGI_FORMAT format);
	static ResourceHandle::Ptr clone(ResourceHandle::Ptr res);

	ResourceHandle(Renderer::ViewType t, int w, int h, DXGI_FORMAT format);

	Renderer::ViewType getType()const;
	const std::wstring& getName()const;
	void setName(const std::wstring& n);
	void setClearValue(const ClearValue& vec){mClearValue = vec;};
	const ClearValue& getClearValue()const{return mClearValue;};

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
	Renderer::ResourceView::Ptr mView;
	ClearValue mClearValue = {};
};


class RenderGraph
{
public:

	class RenderPassVector;

	class Resources
	{
	public:
		ResourceHandle::Ptr getRenderTarget(size_t index = 0) const;
		ResourceHandle::Ptr getDepthStencil()const;

		void add(ResourceHandle::Ptr res, UINT slot = 0);
		void clear();
		size_t getNumRenderTargets()const {return mRenderTargets.size();}
	private:
		std::vector<ResourceHandle::Ptr> mRenderTargets;
		ResourceHandle::Ptr mDepthStencil;

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
		void write(ResourceHandle::Ptr res, InitialType type = IT_NONE, UINT slot = 0);

		void visitOutputs(const std::function<void(RenderPass*)> & visitor);
		bool isPrepared()const;
		void release();
		void clear();
		void setName(const std::string& name){mName = name;};
		const std::string& getName()const{return mName;}
	protected:
		virtual void addInput(RenderPass* p);
		virtual void addOutput(RenderPass* p);

		void prepareResources();

	protected:
		std::vector<ResourceHandle::Ptr> mShaderResources;
		Resources mResources;
		std::map<ResourceHandle*, InitialType> mInitialTypes;

		std::vector<RenderPass*> mInputs;
		std::vector<RenderPass*> mOutputs;
		std::string mName;
	};

	class LambdaRenderPass final : public RenderPass
	{
	public:
		LambdaRenderPass() = default;
		LambdaRenderPass(LambdaRenderPass&& lrp);
		void operator=(LambdaRenderPass&& lrp);

		using SetupFunc = std::function <void(RenderPass*)>;
		using CompileFunc = std::function <void(RenderPass*, const Inputs&)>;
		using ExecuteFunc = std::function <void(const std::vector<ResourceHandle::Ptr>&)>;
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
		ResourceHandle::Ptr mDepthStencil;
	};

	using Visitor = std::function<void(RenderPass*)>;
public:
	RenderPass& begin();
	void setup(const Visitor& prev = {}, const Visitor& post = {});
	void compile(const Visitor& prev = {}, const Visitor& post = {});
	void execute(const Visitor& prev = {}, const Visitor& post = {});
private:
	std::vector<BeginPass> mPasses;
	std::vector<RenderPass*> mCurrents;
};

