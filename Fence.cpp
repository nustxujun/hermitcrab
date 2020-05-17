#include "Fence.h"



void FenceObject::signal(std::function<void()>&& dosomething)
{
	std::lock_guard<std::mutex> lock(mMutex);
	if (dosomething)
		dosomething();
	mCond.store(true, std::memory_order_relaxed);
	mCondVar.notify_one();
}

void FenceObject::wait(std::function<bool()>&& cond)
{
	std::unique_lock<std::mutex> lock(mMutex);

	if (cond)
		mCondVar.wait(lock,[this, cond = std::move(cond)]() {
				return cond() ;
		});
	else
	{
		mCondVar.wait(lock, [this](){
			return mCond.load(std::memory_order_relaxed);
		});
		mCond.store(false, std::memory_order_relaxed);
	}

}
