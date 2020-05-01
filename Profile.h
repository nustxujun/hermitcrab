#pragma once

#include "Renderer.h"

class Profile
{
public:
	static Profile Singleton;

	class Auto
	{
		std::string name;
		Renderer::CommandList::Ref cmdlist;
		Renderer::Profile::Ref profile;
	public:
		Auto(const std::string& name, Renderer::CommandList::Ref cl)
		{
			profile = Profile::Singleton.begin(name, cl);
			this->name = table + name;
			cmdlist = cl;
		}

		~Auto()
		{
			Profile::Singleton.end(profile, cmdlist);
		}
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
	static std::string table;

	std::vector<std::pair<std::string, Renderer::Profile::Ref>> mAllocatteds;
	size_t mCount = 0;
};

#define PROFILE(name, cl) Profile::Auto __autoProfile(name, cl)