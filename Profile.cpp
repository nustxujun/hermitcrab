#include "Profile.h"
#include "Thread.h"

ProfileMgr ProfileMgr::Singleton;
thread_local std::string ProfileMgr::table;

Renderer::Profile::Ref ProfileMgr::begin(const std::string& name, Renderer::CommandList::Ref cl)
{
	auto r = Renderer::getSingleton();
	mMutex.lock();
	while (mAllocatteds.size() < mCount + 1) 
	{
		mAllocatteds.emplace_back("",r->createProfile());
	}

	auto& p = mAllocatteds[mCount++];
	mMutex.unlock();
	p.first = table + name + " on " + Thread::getCurrentName();
	p.second->begin(cl);

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
	for (auto i = 0; i < mCount; ++i)
	{
		auto& p = mAllocatteds[i];
		mLastOutputs.push_back({ p.first, p.second->getCPUTime(), p.second->getGPUTime() });
	}
	mCount = 0;
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
