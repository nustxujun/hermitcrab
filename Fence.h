#pragma once

#include "Common.h"
#include <mutex>
#include <atomic>
class FenceObject
{
public:
	void prepare();
	void signal(bool all = true);
	void wait(std::function<bool()>&& cond = {});
private:
	std::mutex mMutex;
	std::condition_variable mCondVar;
	std::atomic_bool mSignal;
};