#pragma once
#include "Common.h"
#include "Dispatcher.h"
#include "Fence.h"
#include "Thread.h"
#include "Coroutine.h"
#include <atomic>
class TaskExecutor
{
public:
	using Future = Future<Promise>;
	using Coroutine = Coroutine<Promise>;
	template<class ... Args>
	class Task
	{
	public:
		using Process = Future(*)(Args ... );

		Task(	Process f,Args&& ... args ) :
			mCoroutine(std::move(f), std::forward<Args>(args) ...)
		{
		}

		Task(Task&& task):
			mCoroutine(std::move(task.mCoroutine)),
			mMove(std::move(task.mMove)),
			mDone(std::move(task.mDone))
		{
			task.mMove = {};
			task.mDone = {};
		}

		Task(const Task& t) :
			mCoroutine({}),
			mMove({}),
			mDone({})
		{

		}
		void operator()()
		{
			if (mCoroutine.isValid())
				mCoroutine.resume();

			if (mCoroutine.isValid())
				mMove(std::move(*this));
			else
				mDone();
		}
	private:
		Coroutine mCoroutine;
		std::function<void(Task&&)> mMove;
		std::function<void()> mDone;
	};


	using Ptr = std::shared_ptr<TaskExecutor>;

	TaskExecutor(asio::io_context& context);
	void addTask(std::function<void()>&& task);

	template<class ... Args>
	void addCoroutineTask(Task<Args ...>::Process&& task, Args&& ... args)
	{
		mTaskCount.fetch_add(1, std::memory_order_relaxed);

		Task<Args ...> t(std::move(task), std::forward<Args>(args)...);
		t.mMove = [=](Task&& t)
		{
			addTask(std::move(t));
		};
		t.mDone = [=]() 
		{
			mTaskCount.fetch_sub(1, std::memory_order_relaxed);
		};

		addTask(std::move(t));
	}
	template<class T>
	void addTask(T&& task)
	{
		mDispatcher.invoke_strand(std::move(task));
	}

	void wait();

	asio::io_context& getContext();
private:
	asio::io_context& mContext;
	Dispatcher mDispatcher;
	FenceObject mComplete;
	std::atomic_int mTaskCount = 0;

};