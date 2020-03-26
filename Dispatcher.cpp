#include "Dispatcher.h"

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
		mContext.run_one();
	else
		mContext.poll_one();
}

void Dispatcher::run()
{
	mContext.run();
}

void Dispatcher::stop()
{
	mContext.stop();
}
