#include "Profile.h"


Profile Profile::Singleton;
std::string Profile::table;

Renderer::Profile::Ref Profile::begin(const std::string& name, Renderer::CommandList::Ref cl)
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

void Profile::end(Renderer::Profile::Ref p, Renderer::CommandList::Ref cl)
{
	table.pop_back();
	p->end(cl);
}


void Profile::reset()
{
	mCount = 0;
}

std::vector<Profile::Output> Profile::output()
{
	std::vector<Profile::Output> o;
	for (auto i = 0; i < mCount; ++i)
	{
		auto& p = mAllocatteds[i];
		o.push_back({p.first, p.second->getCPUTime(), p.second->getGPUTime()});
	}
	return o;
}
