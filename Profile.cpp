#include "Profile.h"
#include "Thread.h"

ProfileMgr ProfileMgr::Singleton;
thread_local std::vector<ProfileMgr::Node*> ProfileMgr::nodeStack;
Renderer::Profile::Ref ProfileMgr::begin(const std::string& name, Renderer::CommandList::Ref cl)
{
	auto r = Renderer::getSingleton();
	auto tid = Thread::getId();
	mMutex.lock();

	std::string index;
	if (Thread::getId() == 0)
		index = "main";
	if (!nodeStack.empty())
	{
		for (auto& n: nodeStack)
			index += "_" + n->name;
		//while(auto& l: nondStack.)
		//index += "_" + nodeStack.top()->name;
	}
	index += "_" + name;

	Node* node = mProfiles[index];
	if (node == NULL)
	{
		node = new Node();
		node->name = name;
		if (Thread::getId() != 0)
			node->name += " on thread";
		node->profile = Renderer::getSingleton()->createProfile();

		if (!nodeStack.empty())
		{
			auto parent = nodeStack.back();
			node->parent = parent;
			parent->addChild(node);
		}

		mProfiles[index] = node;
	}

	mMutex.unlock();
	node->profile->begin(cl);

	nodeStack.push_back(node);
	node->timestamp = std::chrono::high_resolution_clock::now();
	return node->profile;
}

void ProfileMgr::end(Renderer::Profile::Ref p, Renderer::CommandList::Ref cl)
{
	p->end(cl);
	nodeStack.pop_back();
}


void ProfileMgr::reset()
{
}

void ProfileMgr::visit(const std::function<void(Node*, size_t)>& visitor)
{
	mMutex.lock();
	for (auto& n : mProfiles)
	{
		if (n.second->parent)
			continue;


		n.second->visit(visitor);
		//n.second->visit([&](Node* n, size_t depth) {
			//std::string blank(depth, '	');
			//mLastOutputs.push_back({ blank + n->name, n->profile->getCPUTime(), n->profile->getGPUTime() });
			//});
	}

	mMutex.unlock();

}



ProfileMgr::Auto::Auto(const std::string& name, Renderer::CommandList::Ref cl)
{
	profile = ProfileMgr::Singleton.begin(name, cl);
	cmdlist = cl;
}

ProfileMgr::Auto::~Auto()
{
	ProfileMgr::Singleton.end(profile, cmdlist);
}
