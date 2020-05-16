#include "Fence.h"



void FenceObject::signal(std::function<void()>&& dosomething)
{
	std::unique_lock<std::mutex> lock(mMutex);
	if (dosomething)
		dosomething();
	mCondVar.notify_one();
}

void FenceObject::wait(std::function<bool()>&& cond)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mCondVar.wait(lock,[this, cond = std::move(cond)]() {
		if (cond)
			return cond() ;
		return true;
	});
}
