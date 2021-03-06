#pragma once

#include "Common.h"
#include <thread>
#include <functional>
class Thread
{
public:
	using Ptr = std::shared_ptr<Thread>;
	static std::string getCurrentName();
	static size_t getId();
	static bool isMainThread();
	Thread(const std::string& name, size_t id, std::function<void()>&& f);
	~Thread();
	void join();

	const std::string& getName()const ;
private:
	static thread_local Thread* CurrentThread;
	
	std::shared_ptr<std::thread> mThread;
	std::string mName;
	size_t mID;
};