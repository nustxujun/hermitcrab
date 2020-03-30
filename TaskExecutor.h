#pragma once
#include "Common.h"
#include "Dispatcher.h"

class TaskExecutor
{
public:
	TaskExecutor(int workerCount = std::thread::hardware_concurrency());
	~TaskExecutor();

	void addTask(Dispatcher::Handler&& task);
	void addQueuingTask(Dispatcher::Handler&& task);

	void complete();
protected:
	std::vector<std::thread> mWorkers;
	Dispatcher mDispatcher;

};