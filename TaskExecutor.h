#pragma once
#include "Common.h"
#include "Dispatcher.h"
#include "Fence.h"
#include "Thread.h"
#include <atomic>

class TaskExecutor
{
public:
	using Ptr = std::shared_ptr<TaskExecutor>;

	TaskExecutor(asio::io_context& context);

	template<class Task, class ... Args>
	void addTask(Task&& task,bool strand, Args&& ...args)
	{
		mTaskCount.fetch_add(1, std::memory_order_relaxed);
		auto lambda = [this, task = std::move(task), args ...](){
			task(args...);
			mComplete.signal([this](){
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

	void wait();
private:
	asio::io_context& mContext;
	Dispatcher mDispatcher;
	FenceObject mComplete;
	std::atomic_int mTaskCount = 0;

};