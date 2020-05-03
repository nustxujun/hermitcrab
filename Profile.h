#pragma once

#include "Renderer.h"

class ProfileMgr
{
public:
	static ProfileMgr Singleton;

	class Auto
	{
		Renderer::CommandList::Ref cmdlist;
		Renderer::Profile::Ref profile;
	public:
		Auto(const std::string& name, Renderer::CommandList::Ref cl);

		~Auto();
	};
	
	struct Output
	{
		std::string name;
		float cpu;
		float gpu;
	};

	Renderer::Profile::Ref begin(const std::string& name, Renderer::CommandList::Ref cl);
	void end(Renderer::Profile::Ref p, Renderer::CommandList::Ref cl);
	void reset();

	std::vector<Output> output();
	
private:
	static thread_local std::string table;
	std::vector<std::pair<std::string, Renderer::Profile::Ref>> mAllocatteds;
	std::vector<Output> mLastOutputs;
	size_t mCount = 0;
	std::mutex mMutex;
};

#define PROFILE(name, cl) ProfileMgr::Auto __autoProfile(name, cl)