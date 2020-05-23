#include "Dispatcher.h"

asio::io_context Dispatcher::sharedContext;

Dispatcher::Dispatcher(asio::io_context& context):
	mContext(context), mStrand(context), mWork(context)
{
}

Dispatcher::~Dispatcher()
{
	mContext.stop();
}

void Dispatcher::invoke(Handler&& handler)
{
	mContext.post(std::move(handler));
}

void Dispatcher::invoke_strand(Handler&& handler)
{
	mStrand.post(std::move(handler));
}

void Dispatcher::execute( Handler&& handler)
{
	mContext.dispatch(std::move(handler));
}

void Dispatcher::execute_strand( Handler&& handler)
{
	mStrand.dispatch(std::move(handler));
}

void Dispatcher::poll_one(bool block)
{
	if (block)
		sharedContext.run_one();
	else
		sharedContext.poll_one();
}

void Dispatcher::run(asio::io_context& c)
{
	asio::io_context::work work(c);
	c.run();
}

void Dispatcher::stop(asio::io_context& c )
{
	c.stop();
}
