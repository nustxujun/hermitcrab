#pragma once

#include "Common.h"

class SimpleIPC
{
	static const size_t max_size = 1024 ;
	static const size_t max_use_size = max_size - sizeof(unsigned int);
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

	template<class T>
	SimpleIPC& operator << (T&& v)
	{
		send(&v, sizeof(T));

		return *this;
	}

	SimpleIPC& operator << (const char* str)
	{
		unsigned int len = (unsigned int)::strlen(str);
		send(&len, sizeof(len));
		send(str, len);
		return *this;
	}

	SimpleIPC& operator << (const std::string& str)
	{
		unsigned int len = (unsigned int)str.size();
		send(&len, sizeof(len));
		send(str.c_str(), len);
		return *this;
	}

	template<class T>
	SimpleIPC& operator >> (T& v)
	{
		receive(&v, sizeof(T));

		return *this;
	}

	template<>
	SimpleIPC& operator >> (std::string& str)
	{
		unsigned int len;
		*this >> len;
		str.resize(len);
		receive(&str[0], str.size());
		return *this;
	}

	void invalid(){ mVaild = false;}
private:
	size_t try_receive(void* buffer, size_t size);
private:
	Channal mReceiver;
	Channal mSender;
	HANDLE mSendWaiter;
	HANDLE mReceiveWaiter;
	std::list<std::vector<char>> mSendQueue;
	bool mVaild = true;
};