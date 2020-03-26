#include "TaskExecutor.h"

TaskExecutor::TaskExecutor(int workerCount)
{
	for (int i = 0; i < workerCount; ++i)
		mWorkers.emplace_back([&dispatcher = mDispatcher]() {
			dispatcher.run();
		});
}

TaskExecutor::~TaskExecutor()
{
	mDispatcher.stop();
	for (auto& t: mWorkers)
		t.join();
}

void TaskExecutor::addTask(const Dispatcher::Handler& task, bool immediate)
{
	if (immediate)
		mDispatcher.execute(task);
	else
		mDispatcher.invoke(task);
}

void TaskExecutor::addTaskInQueue(const Dispatcher::Handler& task, bool immediate)
{
	if (immediate)
		mDispatcher.execute_strand(task);
	else
		mDispatcher.invoke_strand(task);
}
