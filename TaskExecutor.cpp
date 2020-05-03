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

void TaskExecutor::addTask(Dispatcher::Handler&& task)
{
	mDispatcher.invoke(std::move(task));
}

void TaskExecutor::addQueuingTask(Dispatcher::Handler&& task)
{
	mDispatcher.invoke_strand(std::move(task));
}

void TaskExecutor::complete()
{
	std::mutex m;
	std::unique_lock<std::mutex> lock;
	std::condition_variable cv;

	mDispatcher.invoke_strand([&](){
		cv.notify_one();
	});

	cv.wait(lock);
}


