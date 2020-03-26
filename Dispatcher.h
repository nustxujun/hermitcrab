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
	Dispatcher::Dispatcher() = default;
	
	~Dispatcher();
	void invoke(const Handler& handler);
	void invoke_strand(const Handler& handler);
	void execute(const Handler& handler);
	void execute_strand(const Handler& handler);

	void poll_one(bool block);
	void run();
	void stop();
private:
	asio::io_context mContext;
	asio::io_context::strand mStrand{mContext};
	asio::io_context::work mWork{mContext};

};