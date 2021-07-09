#include "TaskExecutor.h"
#include <iostream>
struct Test
{
	int index;
	Test()
	{
		index = i++;
		std::cout << "ctor" << index << std::endl;
	}
	Test(const Test& t)
	{
		index = t.index;
		std::cout << "copy lv" << index << std::endl;
	}

	Test(Test&& t)
	{
		index= t.index;
		std::cout << "copy rv" << index << std::endl;
	}
	~Test()
	{
		std::cout << "dctor" << index << std::endl;
		index = 0xffffffff;
	}

	void f()const
	{

	}
	static int i;
};

int Test::i = 0;

void TaskExecutor::addTask(std::function<void()>&& task)
{
	addTask(std::move(task));
}




TaskExecutor::TaskExecutor(asio::io_context& context):
	mContext(context), mDispatcher(context)
{
}

void TaskExecutor::wait()
{
	while(mTaskCount.load(std::memory_order_relaxed) != 0)
		getContext().poll_one();
}

asio::io_context& TaskExecutor::getContext()
{
	return mContext;
}
