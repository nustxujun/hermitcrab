#pragma once 

#include "Common.h"
#include "Renderer.h"
#include "Fence.h"

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
	~ResourceHandle();

	Renderer::ViewType getType()const;
	const std::wstring& getName()const;
	void setName(const std::wstring& n);
	void setClearValue(const ClearValue& vec){mClearValue = vec;};
	const ClearValue& getClearValue()const{return mClearValue;};

	void prepare();
	const Renderer::Resource::Ref& getView() ;
private:
	std::wstring mName;
	Renderer::ViewType mType;
	int mWidth;
	int mHeight;
	DXGI_FORMAT mFormat;
	Renderer::Resource::Ref mView;
	ClearValue mClearValue = {};
	size_t mHashValue = 0;
	std::mutex mViewMutex;
};


class RenderGraph
{
public:
	using RenderTask = std::function<void(Renderer::CommandList::Ref)>;

	class Builder
	{
	public:
		enum InitialType
		{
			IT_NONE,
			IT_CLEAR,
			IT_DISCARD,
		};
	
		void read(const ResourceHandle::Ptr& res, D3D12_RESOURCE_STATES state);
		void write(const ResourceHandle::Ptr& res, InitialType type, D3D12_RESOURCE_STATES state);

		void prepare(Renderer::CommandList::Ref cmdlist)const;
		bool empty(){return mTransitions.empty();}
	private:
		struct Transition
		{
			ResourceHandle::Ptr res;
			D3D12_RESOURCE_STATES state;
			InitialType type;
		};
		std::vector<Transition> mTransitions;

	};

	using RenderPass = std::function<RenderTask(Builder&)>;


	class Barrier
	{
	public:
		using Ptr = std::shared_ptr<Barrier>;
		using BarrierTask = std::function<void(Barrier*)>;
		Barrier();

		void execute();
		void signal();

		void addRenderTask(const std::string& name, RenderPass&& callback);
	private:
		FenceObject::Ptr mFence ;
		using Passes = std::vector<std::pair<std::string, RenderPass>>;
		std::shared_ptr<Passes> mPasses;
		std::mutex mMutex;
	};

public:


	void addPass(const std::string& name, RenderPass&& callback );
	Barrier::Ptr addBarrier(const std::string& name);
	void execute();
private:

	std::vector<std::pair<std::string,RenderPass>> mPasses;
};

