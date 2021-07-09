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
	class Task
	{
		friend class TaskExecutor;
	public:


		template<class F, class ... Args>
		explicit Task(F&& f, Args&& ... args ) :
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

		Task(const Task& t) 
		{

		}
		void operator()()
		{
			if (mCoroutine.isValid())
				mCoroutine.resume();
			else
				return;
			if (!mCoroutine.done())
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
	void addTask(std::function<void()>&& task, bool strand);

	template<class F, class ... Args>
	void addCoroutineTask(F&& task,bool strand, Args&& ... args)
	{
		mTaskCount.fetch_add(1, std::memory_order_relaxed);

		Task t(std::move(task), std::forward<Args>(args)...);
		t.mMove = [=](Task&& t)
		{
			addTask(std::move(t),strand);
		};
		t.mDone = [=]() 
		{
			mTaskCount.fetch_sub(1, std::memory_order_relaxed);
		};

		addTask(std::move(t), strand);
	}
	void addTask(Task&& task, bool strand);
	

	void wait();

	asio::io_context& getContext();
private:
	asio::io_context& mContext;
	Dispatcher mDispatcher;
	FenceObject mComplete;
	std::atomic_int mTaskCount = 0;

};