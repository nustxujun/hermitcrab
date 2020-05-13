#include "Thread.h"

thread_local Thread* Thread::CurrentThread = nullptr;


std::string Thread::getCurrentName()
{
	if (CurrentThread)
		return CurrentThread->getName();
	else
		return "main thread";
}

size_t Thread::getId()
{
	if (CurrentThread)
		return (size_t)CurrentThread;
	else
		return 0;
}

Thread::Thread(const std::string& name, std::function<void()>&& f):
	mName(name)
{
	mThread = std::make_shared<std::thread>([curthread = this, func = std::move(f)](){
		Thread::CurrentThread = curthread;
		func();
	});
}

Thread::~Thread()
{
	join();
}

void Thread::join()
{
	if (!mThread)
		return;

	mThread->join();
	mThread.reset();
}

const std::string& Thread::getName() const
{
	return mName;
}
