#pragma once

#include "Common.h"
#include <mutex>
#include <atomic>

class FenceObject
{
public:
	using Ptr = std::shared_ptr<FenceObject>;

	void prepare(size_t count = 1);
	void signal();
	void wait(std::function<bool()>&& cond = {});
private:
	std::mutex mMutex;
	std::condition_variable mCondVar;
	size_t mSignalCount = 0;
};