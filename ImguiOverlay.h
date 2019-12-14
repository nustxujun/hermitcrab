#pragma once

#include "Common.h"

#include "imgui/imgui.h"
#include "RenderGraph.h"

class ImGuiObject;
class ImGuiPass final: public RenderGraph::RenderPass
{
	void operator=(RenderPass&) = delete;
public:
	using Ptr = std::shared_ptr<ImGuiPass>;
public:
	ImGuiPass();
	~ImGuiPass();

	void setup();
	void compile(const RenderGraph::Inputs& inputs);
	void execute() ;


	void resize(HWND win, int width, int height);

private:
	void initImGui();
	void initRendering();
	void initFonts();
	void draw(ImDrawData* data);

	LRESULT process(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
private:
	Renderer::PipelineState::Ref mPipelineState;
	Renderer::Buffer::Ptr mVertexBuffer;
	Renderer::Buffer::Ptr mIndexBuffer;
	Renderer::Texture::Ref mFonts;
	int mWidth = 0;
	int mHeight = 0;
};


class ImGuiObject
{
public:
	virtual void update() {
		for (auto& i : children)
			i->update();
	};
	template<class T, class ... Args>
	T* createChild(const Args& ... args)
	{
		auto c = new T(args...);
		c->parent = this;
		children.push_back(c);
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


	static ImGuiObject* root() 
	{
		static ImGuiObject r;
		return &r;
	}

	using Cmd = std::function<void(void)>;

	void setCommand(int index, Cmd cmd)
	{
		mCmdMaps[index] = cmd;
	}
protected:
	std::map<int, std::function<void(void)>> mCmdMaps;

};

struct ImGuiWindow: public ImGuiObject
{
	std::string text;
	bool visible;
	ImGuiWindowFlags flags;

	ImGuiWindow(const char* t = "", bool b = true, ImGuiWindowFlags f = 0): 
		text(t), visible(b), flags(f)
	{}
	void update() {
		if (!visible)
			return;

		for (auto& i : mCmdMaps)
			i.second();
		mCmdMaps.clear();

		ImGui::Begin(text.c_str(),&visible, flags);
		ImGuiObject::update();
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

	void update() {
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

	void update()
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
	
	void update()
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
	void update() {
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


	void update()
	{
		if (ImGui::SliderFloat(text.c_str(), &value, valMin, valMax, display.c_str()) && callback)
		{
			callback(this);
		}
	}
	ImGuiSlider(const char* t, float v, float vmin, float vmax, Callback cb):
		text(t), value(v),valMin(vmin), valMax(vmax),callback(cb)
	{

	}
};