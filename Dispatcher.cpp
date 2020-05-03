#include "Dispatcher.h"

asio::io_context Dispatcher::sharedContext;

Dispatcher::~Dispatcher()
{
	mContext.stop();
}

void Dispatcher::invoke(const Handler& handler)
{
	mContext.post(handler);
}

void Dispatcher::invoke_strand(const Handler& handler)
{
	mStrand.post(handler);
}

void Dispatcher::execute(const Handler& handler)
{
	mContext.dispatch(handler);
}

void Dispatcher::execute_strand(const Handler& handler)
{
	mStrand.dispatch(handler);
}

void Dispatcher::poll_one(bool block)
{
	if (block)
		sharedContext.run_one();
	else
		sharedContext.poll_one();
}

void Dispatcher::run()
{
	asio::io_context::work work(sharedContext);
	sharedContext.run();
}

void Dispatcher::stop()
{
	sharedContext.stop();
}
