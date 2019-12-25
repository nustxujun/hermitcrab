#pragma once

#include "Common.h"

class SimpleIPC
{
	static const auto max_size = 1024;
public:
	class Channal
	{
	public:
		void create(const std::string& name);
		void open(const std::string& name);
		void close();

		size_t write(const void* data, size_t size);
		size_t read(void* data, size_t size);
	private:
		void map();
		void lock();
		void unlock();
	private:
		HANDLE mMutex = 0;
		HANDLE mHandle = 0;
		char* mMemory = 0;
		char* mData = 0;
		unsigned int* mCount = 0;
	};

	~SimpleIPC();
	void listen(const std::string& name);
	void connect(const std::string& name);
	void close();

	void send(const void* buffer, size_t size);
	void receive(void* buffer, size_t size);
private:
	size_t try_receive(void* buffer, size_t size);
private:
	Channal mReceiver;
	Channal mSender;
	HANDLE mSendWaiter;
	HANDLE mReceiveWaiter;
	std::list<std::vector<char>> mSendQueue;
};