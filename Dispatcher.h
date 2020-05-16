#pragma once

#define ASIO_STANDALONE
#include "Common.h"
#include "asio/asio.hpp"
#include "asio/asio/strand.hpp"


class Dispatcher
{
public:
	using Handler = std::function<void(void)>;
public:
	Dispatcher(asio::io_context& context = sharedContext);
	
	~Dispatcher();
	void invoke(const Handler& handler);
	void invoke_strand(const Handler& handler);
	void execute(const Handler& handler);
	void execute_strand(const Handler& handler);

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