#include "SimpleIPC.h"

SimpleIPC::~SimpleIPC()
{
	close();
}

void SimpleIPC::listen(const std::string& name)
{
	mSender.create(name + "_send");
	mReceiver.create(name + "_receive");

	mSendWaiter = CreateSemaphoreA(NULL, 0,1, (name + "_send_waiter").c_str());
	mReceiveWaiter = CreateSemaphoreA(NULL, 0,1, (name + "_receive_waiter").c_str());
	::ReleaseSemaphore(mSendWaiter,1,0);
}

void SimpleIPC::connect(const std::string& name)
{
	mSender.create(name + "_receive");
	mReceiver.create(name + "_send");

	mReceiveWaiter = CreateSemaphoreA(NULL, 0, 1, (name + "_send_waiter").c_str());
	mSendWaiter = CreateSemaphoreA(NULL, 0, 1, (name + "_receive_waiter").c_str());
	::ReleaseSemaphore(mSendWaiter, 1, 0);
}

void SimpleIPC::close()
{
	mSender.close();
	mReceiver.close();
}

void SimpleIPC::send(const void* buffer, size_t size)
{
	char* beg = (char*)buffer;
	while (size > 0)
	{
		//::WaitForSingleObject(mSendWaiter, -1);
		auto writesize = mSender.write(beg, size);
		//::ReleaseMutex(mReceiveWaiter);

		size -= writesize;
		beg += writesize;
	}
	
}

void SimpleIPC::receive(void* buffer, size_t size)
{
	char* beg = (char*) buffer;
	while (size > 0)
	{
		//::WaitForSingleObject(mReceiveWaiter, -1);
		auto readsize = mReceiver.read(beg, size);
		//::ReleaseMutex(mSendWaiter);

		size -= readsize;
		beg += readsize;
	}
}


void SimpleIPC::Channal::map()
{
	Common::Assert(mHandle != NULL, "ipc need init");
	mMemory = (char*)::MapViewOfFile(mHandle, FILE_MAP_ALL_ACCESS,0,0,0);
	Common::Assert(mMemory != NULL, "fail to get shared memory");
	mCount = (unsigned int*)mMemory;
	mData = mMemory + sizeof(unsigned int);
}

void SimpleIPC::Channal::lock()
{
	::WaitForSingleObject(mMutex, -1);
}


void SimpleIPC::Channal::unlock()
{
	::ReleaseMutex(mMutex);
}

void SimpleIPC::Channal::create(const std::string& name)
{
	mMutex = ::CreateMutexA(NULL,FALSE,(name + "_mutex").c_str() );
	
	mHandle = ::CreateFileMappingA(
		INVALID_HANDLE_VALUE,
		NULL, 
		PAGE_READWRITE,
		0,
		max_size,
		(name + "_sm").c_str());

	if (::GetLastError() != 0)
	{
		open(name);
	}
	else	
	//Common::checkResult();
	//Common::Assert(mHandle != NULL, "faild to create file mapping");
		map();


}

void SimpleIPC::Channal::open(const std::string& name)
{
	mMutex = ::CreateMutexA(NULL, FALSE, (name + "_mutex").c_str());

	mHandle = ::OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, (name + "_sm").c_str());
	Common::Assert(mHandle != NULL, "faild to create file mapping");

	map();
}

void SimpleIPC::Channal::close()
{
	if (mMemory)
	{
		::UnmapViewOfFile(mMemory);
		mMemory = 0;
	}

	if (mHandle)
	{
		::CloseHandle(mHandle);
		mHandle = 0;
	}

	if (mMutex)
	{
		::CloseHandle(mMutex);
	}
}

size_t SimpleIPC::Channal::write(const void* data, size_t size)
{
	size_t use =size_t( *mCount);
	if (use == max_size)
		return 0;

	lock();
	size = std::min(size, max_size - use);
	use = size_t(*mCount);

	memcpy(mData + use, data, size);
	*mCount +=(unsigned int)size;
	unlock();
	return size;
}

size_t SimpleIPC::Channal::read(void* data, size_t size)
{
	size_t use = *mCount;
	if (use == 0)
		return 0;

	lock();
	use = *mCount;
	size = std::min(size, use);

	memcpy(data, mData,  size);

	auto left = use - (unsigned int)size;
	*mCount = left;
	memmove(mData, mData + size, left);

	unlock();
	return size;
}