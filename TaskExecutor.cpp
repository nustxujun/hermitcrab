#include "TaskExecutor.h"

TaskExecutor::TaskExecutor(asio::io_context& context):
	mContext(context), mDispatcher(context)
{
}

void TaskExecutor::wait()
{
	mComplete.wait([this](){
		return mTaskCount.load(std::memory_order_relaxed) == 0;
	});
}