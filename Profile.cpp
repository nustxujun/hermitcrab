#include "Profile.h"
#include "Thread.h"

ProfileMgr ProfileMgr::Singleton;
thread_local std::string ProfileMgr::table;

Renderer::Profile::Ref ProfileMgr::begin(const std::string& name, Renderer::CommandList::Ref cl)
{
	auto r = Renderer::getSingleton();
	auto tid = Thread::getId();
	mMutex.lock();
	auto& allocs = mAllocatteds[tid];
	while (allocs.first.size() < allocs.second + 1)
	{
		allocs.first.emplace_back("",r->createProfile());
	}
	auto& p = allocs.first[allocs.second++];

	p.first = Common::format(table , name , " on " , tid);
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
	PROFILE("export profile", {});
	mLastOutputs.clear();
	std::map<std::string, std::vector<Output>> temp;
	mMutex.lock();
	for (auto& t : mAllocatteds)
	{
		if (t.second.second == 0)
			continue;
		auto& vec = temp[t.second.first[0].first];
		
		for (auto i = 0; i < t.second.second; ++i)
		{
			auto& p = t.second.first[i];
			vec.push_back({ p.first, p.second->getCPUTime(), p.second->getGPUTime() });
		}
		t.second.second = 0;
	}
	mMutex.unlock();

	for (auto& t : temp)
	{
		for (auto& v : t.second)
		{
			mLastOutputs.push_back(v);
		}
	}

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
