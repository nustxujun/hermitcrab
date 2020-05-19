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

	void addTask(std::function<void()>&& task,bool strand);

	void wait();
private:
	asio::io_context& mContext;
	Dispatcher mDispatcher;
	FenceObject mComplete;
	std::atomic_int mTaskCount = 0;

};