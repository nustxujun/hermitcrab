#include "TaskExecutor.h"


void TaskExecutor::addTask(std::function<void()>&& task, bool strand)
{
	mTaskCount.fetch_add(1, std::memory_order_relaxed);
	auto lambda = [this, task = std::move(task)](){
		task();
		mComplete.signal([this]() {
			mTaskCount.fetch_sub(1, std::memory_order_relaxed);
		});
	};
	if (strand)
	{
		mDispatcher.invoke_strand(std::move(lambda));
	}
	else
	{
		mDispatcher.invoke(std::move(lambda));
	}
}

TaskExecutor::TaskExecutor(asio::io_context& context):
	mContext(context), mDispatcher(context)
{
}

void TaskExecutor::wait()
{
	mComplete.wait([this](){
		return mTaskCount.load(std::memory_order_relaxed) == 0;
	});
}