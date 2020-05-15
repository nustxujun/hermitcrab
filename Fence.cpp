#include "Fence.h"

void FenceObject::prepare(size_t count )
{
	std::unique_lock<std::mutex> lock(mMutex);
	mSignalCount = count;
}

void FenceObject::signal()
{
	std::unique_lock<std::mutex> lock(mMutex);
	mSignalCount = std::min( (size_t)0, mSignalCount - 1);
	mCondVar.notify_one();
}

void FenceObject::wait(std::function<bool()>&& cond)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mCondVar.wait(lock,[this, cond = std::move(cond)]() {
		if (cond)
			return cond() && mSignalCount == 0;
		return mSignalCount == 0;
	});
}
