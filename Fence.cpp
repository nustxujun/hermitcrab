#include "Fence.h"

void FenceObject::prepare()
{
	std::unique_lock<std::mutex> lock(mMutex);
	mSignal.store(false);
}

void FenceObject::signal(bool all)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mSignal.store(true);
	if (all)
		mCondVar.notify_all();
	else
		mCondVar.notify_one();
}

void FenceObject::wait(std::function<bool()>&& cond)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mCondVar.wait(lock,[this, cond = std::move(cond)]() {
		if (cond)
			return cond() && mSignal.load();
		return mSignal.load();
	});
}
