#include "Profile.h"


ProfileMgr ProfileMgr::Singleton;
std::string ProfileMgr::table;

Renderer::Profile::Ref ProfileMgr::begin(const std::string& name, Renderer::CommandList::Ref cl)
{
	auto r = Renderer::getSingleton();
	while (mAllocatteds.size() < mCount + 1) 
	{
		mAllocatteds.emplace_back("",r->createProfile());
	}

	auto& p = mAllocatteds[mCount++];
	p.first = table + name;
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
	mCount = 0;
}

std::vector<ProfileMgr::Output> ProfileMgr::output()
{
	std::vector<ProfileMgr::Output> o;
	for (auto i = 0; i < mCount; ++i)
	{
		auto& p = mAllocatteds[i];
		o.push_back({p.first, p.second->getCPUTime(), p.second->getGPUTime()});
	}
	return o;
}
