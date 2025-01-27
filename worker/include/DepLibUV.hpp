#ifndef MS_DEP_LIBUV_HPP
#define MS_DEP_LIBUV_HPP

#include "common.hpp"
#include <uv.h>
#include <mutex>
#include <queue>

class DepLibUV
{
public:
	class AsyncTaskQueue
	{
	public:
		AsyncTaskQueue(uv_async_t* async);
		~AsyncTaskQueue();
		void PostTask(std::function<void()> task);
		void RunTask();

	private:
		std::queue<std::function<void()>> tasks_;
		std::mutex lock_;
		uv_async_t* uvAsyncHandle_{ nullptr };
	};

public:
	static void ClassInit();
	static void ClassDestroy();
	static void PrintVersion();
	static void RunLoop();
	static uv_loop_t* GetLoop()
	{
		return DepLibUV::loop;
	}
	static uint64_t GetTimeMs()
	{
		return static_cast<uint64_t>(uv_hrtime() / 1000000u);
	}
	static uint64_t GetTimeUs()
	{
		return static_cast<uint64_t>(uv_hrtime() / 1000u);
	}
	static uint64_t GetTimeNs()
	{
		return uv_hrtime();
	}
	// Used within libwebrtc dependency which uses int64_t values for time
	// representation.
	static int64_t GetTimeMsInt64()
	{
		return static_cast<int64_t>(DepLibUV::GetTimeMs());
	}
	// Used within libwebrtc dependency which uses int64_t values for time
	// representation.
	static int64_t GetTimeUsInt64()
	{
		return static_cast<int64_t>(DepLibUV::GetTimeUs());
	}

	static AsyncTaskQueue* GetTaskQueue();

private:
	thread_local static uv_loop_t* loop;
	thread_local static AsyncTaskQueue* taskQueue;
};

#endif
