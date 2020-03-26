#pragma once

#include "Dispatcher.h"

class TaskExecutor
{
public:
	TaskExecutor(int workerCount = std::thread::hardware_concurrency());
	~TaskExecutor();

	void addTask(const Dispatcher::Handler& task, bool immediate = false);
	void addTaskInQueue(const Dispatcher::Handler& task, bool immediate = false);

private:
	std::vector<std::thread> mWorkers;
	Dispatcher mDispatcher;
};