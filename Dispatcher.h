#pragma once

#define ASIO_STANDALONE
#include "Common.h"
#include "asio/asio.hpp"
#include "asio/asio/strand.hpp"


class Dispatcher
{
public:
	using Handler = std::function<void(void)>;
	using CoroutineHandler = std::function<size_t(size_t)>;

	class Coroutine
	{
	public:
		static constexpr size_t END = -1;

		bool operator()()
		{
			mCurrent = call(mCurrent);
			return mCurrent == END;
		}

	protected:
		virtual size_t call(size_t index) = 0;
	private:
		size_t mCurrent = 0;
	};

	class LambdaCoroutine : public Coroutine
	{
		size_t call(size_t index) override
		{
			return mFunction(index);
		}
		std::function<size_t(size_t)> mFunction;
	};

	class DefaultCoroutine : public Coroutine
	{
		size_t call(size_t index) override
		{
			mFunction();
			return END;
		}
		std::function<void()> mFunction;
	};
	
public:
	Dispatcher::Dispatcher() = default;
	
	~Dispatcher();
	void invoke(const Handler& handler);
	void invoke_strand(const Handler& handler);
	void execute(const Handler& handler);
	void execute_strand(const Handler& handler);

	void invoke(const CoroutineHandler& handler);
	void invoke_strand(const CoroutineHandler& handler);
	void execute(const CoroutineHandler& handler);
	void execute_strand(const CoroutineHandler& handler);

	static void poll_one(bool block);
	static void run();
	static void stop();
private:
	static asio::io_context sharedContext;
	asio::io_context& mContext = sharedContext;
	asio::io_context::strand mStrand{ mContext };
	asio::io_context::work mWork{ mContext };

};