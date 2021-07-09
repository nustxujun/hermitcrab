#pragma once 

#include <coroutine>
#include <functional>

template<class Promise>
class Future
{
public:
	using promise_type = Promise;
	using CoroutineHandle = std::coroutine_handle<promise_type>;
	Future(CoroutineHandle h):mHandle(h) {};
	Future(){};
	CoroutineHandle getHandle()const{return mHandle;}
private:
	CoroutineHandle mHandle;
};

class Promise
{
public:
	inline Future<Promise> get_return_object(){return Future(std::coroutine_handle<Promise>::from_promise(*this));};
	constexpr std::suspend_never initial_suspend()noexcept { return {}; }
	constexpr std::suspend_always final_suspend() noexcept { return {}; }
	inline void unhandled_exception() { throw std::exception("unhandled_exception in coroutine"); }
	inline void return_void() {}
};



template<class promise_type = Promise>
class Coroutine
{
public:
	using Future = Future<promise_type>;

	Coroutine(const Coroutine&) = delete;
	void operator=(const Coroutine&) = delete;

	template<class F, class ... Args>
	Coroutine(F&& func, Args && ... args)
	{
		mFuture = func(std::forward<Args>(args) ...);
	}

	Coroutine()
	{
	}

	Coroutine(Coroutine&& co)
	{
		*this = std::move(co);
	}

	~Coroutine()
	{
		if (!!mFuture.getHandle())
		{
			mFuture.getHandle().destroy();
		}
	}

	Coroutine& operator = (Coroutine&& co)
	{
		mFuture = co.mFuture;
		co.mFuture = {};
		return *this;
	}


	void resume()const
	{
		mFuture.getHandle().resume();
	}

	bool isValid()const
	{
		return !!mFuture.getHandle();
	}

	bool done()const
	{
		return mFuture.getHandle().done();
	}

	Promise& getPromise()const
	{
		return mFuture.getHandle().promise();
	}
private:
	Future mFuture;
};