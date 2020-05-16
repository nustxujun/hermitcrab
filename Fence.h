#pragma once

#include "Common.h"
#include <mutex>
#include <atomic>

class FenceObject
{
public:
	using Ptr = std::shared_ptr<FenceObject>;

	void signal(std::function<void()>&& dosomething = {});
	void wait(std::function<bool()>&& cond = {});
private:
	std::mutex mMutex;
	std::condition_variable mCondVar;
	std::atomic_bool mCond = false;
};