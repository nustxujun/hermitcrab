#pragma once

#define ASIO_STANDALONE
#include "Common.h"
#include "asio.hpp"
#include "asio/strand.hpp"


class Dispatcher
{
public:
public:
	Dispatcher(asio::io_context& context = sharedContext);
	
	~Dispatcher();
	template<class Handler>
	void invoke(Handler&& handler)
	{
		mContext.post(std::move(handler));
	}

	template<class Handler>
	void invoke_strand(Handler&& handler)
	{
		mStrand.post(std::move(handler));
	}

	template<class Handler>
	void execute(Handler&& handler)
	{
		mContext.dispatch(std::move(handler));
	}

	template<class Handler>
	void execute_strand(Handler&& handler)
	{
		mStrand.dispatch(std::move(handler));
	}

	static void poll_one(bool block);
	static void run(asio::io_context& context);
	static void stop(asio::io_context& context);

	static asio::io_context& getSharedContext(){return sharedContext;}
private:
	static asio::io_context sharedContext;
	asio::io_context& mContext = sharedContext;
	asio::io_context::strand mStrand;
	asio::io_context::work mWork;

};