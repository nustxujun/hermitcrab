#include "SimpleIPC.h"

SimpleIPC::~SimpleIPC()
{
	close();
}

void SimpleIPC::listen(const std::string& name)
{
	mSender.create(name + "_send");
	mReceiver.create(name + "_receive");
}

void SimpleIPC::connect(const std::string& name)
{
	mSender.create(name + "_receive");
	mReceiver.create(name + "_send");
}

void SimpleIPC::close()
{
	mSender.close();
	mReceiver.close();
}

void SimpleIPC::send(const void* buffer, size_t size)
{
	std::vector<char> data(size);
	memcpy(data.data(), buffer, size);
	mSendQueue.push_back([b = std::move(data), &sender = mSender]() {
		return sender.write(b.data(), b.size());
	});
}

size_t SimpleIPC::receive(void* buffer, size_t size)
{
	return size_t();
}

void SimpleIPC::Channal::map()
{
	Common::Assert(mHandle != NULL, "ipc need init");
	mMemory = (char*)::MapViewOfFile(mHandle, FILE_MAP_ALL_ACCESS,0,0,0);
	Common::Assert(mMemory != NULL, "fail to get shared memory");
	mCount = (unsigned int*)mMemory;
	mData = mMemory + sizeof(unsigned int);
	mEnd = mData;
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
	mMutex = ::CreateMutexA(NULL,FALSE,name.c_str());
	
	mHandle = ::CreateFileMappingA(
		INVALID_HANDLE_VALUE,
		NULL, PAGE_READWRITE | SEC_COMMIT,
		0,
		max_size,
		name.c_str());

	Common::Assert(mHandle != NULL, "faild to create file mapping");
	map();


}

void SimpleIPC::Channal::open(const std::string& name)
{
	mMutex = ::CreateMutexA(NULL, FALSE, name.c_str());

	mMutex = ::OpenMutexA(NULL, FALSE, name.c_str());

	mHandle = ::OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
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

bool SimpleIPC::Channal::write(const void* data, size_t size)
{
	if (mEnd - mData < size)
		return false;

	lock();
	memcpy(mEnd, data, size);
	*mCount +=(unsigned int)size;
	mEnd += size;
	unlock();
	return true;
}

size_t SimpleIPC::Channal::read(void* data, size_t size)
{
	if (mEnd == mData)
		return 0;

	lock();
	size = std::min((unsigned int)size, *mCount);
	memcpy(data, mData,  size);
	if (size != *mCount)
	{	
		memmove(mData, mData + size, 0);
		*mCount = *mCount - (unsigned int)size;
	}
	unlock();
	return size;
}