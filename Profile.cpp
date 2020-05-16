#include "Profile.h"
#include "Thread.h"

ProfileMgr ProfileMgr::Singleton;
thread_local std::string ProfileMgr::table;

Renderer::Profile::Ref ProfileMgr::begin(const std::string& name, Renderer::CommandList::Ref cl)
{
	auto r = Renderer::getSingleton();
	std::string thdname = Thread::getCurrentName();
	mMutex.lock();
	auto& allocs = mAllocatteds[thdname];
	while (allocs.first.size() < allocs.second + 1)
	{
		allocs.first.emplace_back("",r->createProfile());
	}
	auto& p = allocs.first[allocs.second++];

	p.first = table + name + " on " + thdname;
	p.second->begin(cl);
	mMutex.unlock();

	table.push_back('\t');

	return p.second;
}

void ProfileMgr::end(Renderer::Profile::Ref p, Renderer::CommandList::Ref cl)
{
	table.pop_back();
	p->end(cl);
}


void ProfileMgr::reset()
{
	mLastOutputs.clear();
	mMutex.lock();
	for (auto& t : mAllocatteds)
	{
		for (auto i = 0; i < t.second.second; ++i)
		{
			auto& p = t.second.first[i];
			mLastOutputs.push_back({ p.first, p.second->getCPUTime(), p.second->getGPUTime() });
		}
		t.second.second = 0;
	}
	mMutex.unlock();


}

std::vector<ProfileMgr::Output> ProfileMgr::output()
{
	return mLastOutputs;
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
