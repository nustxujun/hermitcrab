#pragma once

#include "Common.h"

#include "imgui/imgui.h"
#include "RenderGraph.h"

class ImGuiObject;
class ImGuiPass 
{
public:
	using Ptr = std::shared_ptr<ImGuiPass>;
public:
	ImGuiPass();
	~ImGuiPass();

	static Renderer::RenderTask execute(ImGuiPass::Ptr pass) ;
	void update();
	void resize(HWND win, int width, int height);

private:
	void initImGui();
	void initRendering();
	void initFonts();

private:
	Renderer::PipelineState::Ref mPipelineState;
	Renderer::Buffer::Ref mVertexBuffer[Renderer::NUM_BACK_BUFFERS];
	Renderer::Buffer::Ref mIndexBuffer[Renderer::NUM_BACK_BUFFERS];
	Renderer::Resource::Ref mFonts;
	int mWidth = 0;
	int mHeight = 0;

	FenceObject mFence;
};

namespace ImGuiOverlay
{
class ImGuiObject
{
public:
	void framemove() 
	{
		if (visible)
			update();

		if (!mUpdate.load(std::memory_order_relaxed))
			return;

		std::lock_guard<std::mutex> lock(mMutex);
		for (auto& f : mFencingQueue)
			f();
		mFencingQueue.clear();
	}
	virtual void update() {
		for (auto& i : children)
			i->framemove();
	};
	template<class T, class ... Args>
	T* createChild(const Args& ... args)
	{
		auto c = new T(args...);
		c->parent = this;
		//children.push_back(c);
		mUpdate.store(true, std::memory_order_relaxed);
		std::lock_guard<std::mutex> lock(mMutex);
		mFencingQueue.push_back([=](){
			children.push_back(c);
		});
		return c;
	}

	virtual ~ImGuiObject()
	{
		destroy();
	}

	void destroy()
	{
		std::vector<ImGuiObject*> temp;
		temp.swap(children);
		for (auto& i : temp)
		{
			i->destroy();
		}

		if (parent)
		{
			auto endi = parent->children.end();
			for (auto i = parent->children.begin(); i != endi; ++i)
			{
				if (*i == this)
				{
					parent->children.erase(i);
					parent = nullptr;
					return;
				}
			}
		}
	}

	ImGuiObject* operator[](size_t index)const 
	{
		return children[index];
	}



	std::vector<ImGuiObject*> children;
	ImGuiObject* parent = nullptr;

	static void init()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsClassic();
	}

	static ImGuiObject* root() 
	{
		static ImGuiObject r;
		return &r;
	}

	static void close()
	{
		std::vector<ImGuiObject*> tmp;
		tmp.swap(root()->children);
		for (auto& c: tmp)
			delete c;

		ImGui::DestroyContext();
	}


	using Cmd = std::function<void(void)>;

	void setCommand(int index, Cmd cmd)
	{
		mCmdMaps[index] = cmd;
	}

	bool visible = true;
	float width = 0;
	float height = 0;
	std::function<bool(ImGuiObject*)> drawCallback;
protected:
	std::map<int, std::function<void(void)>> mCmdMaps;
	std::vector<std::function<void()>> mFencingQueue;
	std::mutex mMutex;
	std::atomic_bool mUpdate = false;
};

struct ImGuiWindow: public ImGuiObject
{
	std::string text;
	ImGuiWindowFlags flags;

	ImGuiWindow(const char* t = "", bool b = true, ImGuiWindowFlags f = 0): 
		text(t),  flags(f)
	{
		visible = b;
	}
	void update() override {
		for (auto& i : mCmdMaps)
			i.second();
		mCmdMaps.clear();

		if (ImGui::Begin(text.c_str(), &visible, flags) && (!drawCallback || drawCallback(this)))
		{
			ImGuiObject::update();
		}
		ImGui::End();
	}

	enum {
		POSITION,
		SIZE,
	};

	void setSize(float x, float y)
	{
		setCommand(SIZE, [=]() {
			ImGui::SetNextWindowSize({ x, y });
			});
	}

	void setPosition(float x, float y)
	{
		setCommand(POSITION, [=]() {
			ImGui::SetNextWindowPos({ x, y });
			});
	}
private:

};

struct ImGuiText :public ImGuiObject
{
	std::string text;

	void update() override {
		ImGui::Text(text.c_str());
		ImGuiObject::update();
	}

	ImGuiText(const char* c = "") : text(c)
	{
	}
};

struct ImGuiMenuItem : public ImGuiObject
{
	std::string text;
	bool selected = true;
	bool enabled;

	using Callback = std::function<void(ImGuiMenuItem*)>;
	Callback callback;

	ImGuiMenuItem(const char* t, Callback cb, bool e = true):
		text(t), callback(cb), enabled(e)
	{
	}

	void update() override
	{
		if (ImGui::MenuItem(text.c_str(), nullptr, &selected, enabled) && callback)
			callback(this);

		ImGuiObject::update();
	}

};

struct ImGuiMenu :public ImGuiObject
{
	std::string text;
	bool enabled = true;
	
	void update() override
	{
		if (ImGui::BeginMenu(text.c_str(), enabled))
		{
			ImGuiObject::update();
			ImGui::EndMenu();
		}
	}


	ImGuiMenu(const char* t, bool e):
		text(t), enabled(e)
	{
	}

	ImGuiMenuItem* createMenuItem(const char* t, ImGuiMenuItem::Callback c, bool enable = true)
	{
		return createChild<ImGuiMenuItem>(t, c, enable);
	}

};

struct ImGuiMenuBar: public ImGuiObject
{
	bool main = false;
	void update() override {
		if (main)
		{
			if (ImGui::BeginMainMenuBar())
			{
				ImGuiObject::update();
				ImGui::EndMainMenuBar();
			}
		}
		else
		{
			if (ImGui::BeginMenuBar())
			{
				ImGuiObject::update();
				ImGui::EndMenuBar();
			}
		}
	}

	ImGuiMenuBar(bool m = false) :main(m)
	{
	}

	ImGuiMenu* createMenu(const char*  text, bool enable)
	{
		return createChild<ImGuiMenu>(text, enable);
	}
};

struct ImGuiSlider :public ImGuiObject
{
	std::string text;
	float value;
	float valMin;
	float valMax;
	std::string display = "%.3f";
	using Callback = std::function<void(ImGuiSlider*)>;
	Callback callback;


	void update() override
	{
		if (ImGui::SliderFloat(text.c_str(), &value, valMin, valMax, display.c_str()) && callback)
		{
			callback(this);
		}

		ImGuiObject::update();

	}
	ImGuiSlider(const char* t, float v, float vmin, float vmax, Callback cb):
		text(t), value(v),valMin(vmin), valMax(vmax),callback(cb)
	{

	}
};

struct ImGuiButton : public ImGuiObject
{
	std::string text;

	using Callback = std::function<void(ImGuiButton*)>;
	Callback callback;

	ImGuiButton(const std::string& t, float w = 0, float h = 0):
		text(t)
	{
		width = w;
		height = h;
	}

	void update()override
	{
		if (ImGui::Button(text.c_str(),{width, height}))
			callback(this);

		ImGuiObject::update();
	}
};


struct ImGuiSelectable : public ImGuiObject
{
	std::string text;
	bool selection = false;
	ImGuiSelectableFlags flags = 0;

	using Callback = std::function<void(ImGuiSelectable*)>;
	Callback callback;
	ImGuiSelectable(const std::string& c, float w = 0, float h = 0, bool selected = false):
		text(c), selection(selected)
	{
		width = w;
		height = h;
	}

	void update() override
	{
		if (ImGui::Selectable(text.c_str(),&selection,flags, {width, height}))
		{
			callback(this);
			ImGuiObject::update();
		}

	}
};

struct ImGuiImage : public ImGuiObject
{
	Renderer::Resource::Ref texture;
	void update() override
	{
		if (!texture)
			return;
		auto& desc = texture->getDesc();
		ImGui::Image((ImTextureID)texture->getShaderResource().ptr, { (float)desc.Width,  (float)desc.Height });
		drawCallback(this);
	}
};
}