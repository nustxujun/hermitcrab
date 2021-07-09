#pragma once

#include "Renderer.h"
#include <stack>
class ProfileMgr
{
public:
	static ProfileMgr Singleton;

	class Auto
	{
		Renderer::CommandList * cmdlist;
		Renderer::Profile::Ref profile;
	public:
		Auto(const std::string& name, Renderer::CommandList * cl);

		~Auto();
	};

	struct Node
	{
		std::string name;
		Renderer::Profile::Ref profile;
		Node* parent = 0;
		Node* children= 0;
		Node* next = 0;
		std::chrono::high_resolution_clock::time_point timestamp;

		~Node()
		{
			abort();
		}

		void addChild(Node* node)
		{
			if (children)
			{
				Node* beg = children;
				while(beg->next)
					beg = beg->next;
					
				beg->next = node;
			}
			else
				children = node;
		}

		void visit(const std::function<void(Node*, size_t)>& visitor, size_t depth = 0)
		{
			visitor(this, depth);
			if (children)
				children->visit(visitor, depth + 1);

			if (next)
				next->visit(visitor,depth);
		}
	};
	

	Renderer::Profile::Ref begin(const std::string& name, Renderer::CommandList * cl);
	void end(Renderer::Profile::Ref p, Renderer::CommandList * cl);
	void reset();

	void visit(const std::function<void(Node*, size_t)>& visitor);
	
private:
	static thread_local std::vector<Node*> nodeStack;
	std::map<std::string, Node*> mProfiles;
	size_t mCount = 0;
	std::mutex mMutex;
};

#define PROFILE(name, cl) ProfileMgr::Auto __auto_profile(name, cl)

//#define PROFILE(name, cl)