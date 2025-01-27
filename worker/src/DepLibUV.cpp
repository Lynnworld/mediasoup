#define MS_CLASS "DepLibUV"
// #define MS_LOG_DEV_LEVEL 3

#include "DepLibUV.hpp"
#include "Logger.hpp"

/* Static variables. */

thread_local uv_loop_t* DepLibUV::loop{ nullptr };
thread_local DepLibUV::AsyncTaskQueue* DepLibUV::taskQueue{ nullptr };

/* Static methods for UV callbacks. */

inline static void onCloseLoop(uv_handle_t* handle)
{
	delete reinterpret_cast<uv_loop_t*>(handle);
}

inline static void onWalk(uv_handle_t* handle, void* /*arg*/)
{
	// Must use MS_ERROR_STD since at this point the Channel is already closed.
	MS_ERROR_STD(
	  "alive UV handle found (this shouldn't happen) [type:%s, active:%d, closing:%d, has_ref:%d]",
	  uv_handle_type_name(handle->type),
	  uv_is_active(handle),
	  uv_is_closing(handle),
	  uv_has_ref(handle));

	if (!uv_is_closing(handle))
	{
		uv_close(handle, onCloseLoop);
	}
}

void onCloseAsync(uv_handle_t* handle)
{
	delete handle;
}

static void runTasks(uv_async_t* handle)
{
	auto taskQueue = reinterpret_cast<DepLibUV::AsyncTaskQueue*>(handle->data);
	taskQueue->RunTask();
}

/* Static methods. */

void DepLibUV::ClassInit()
{
	// NOTE: Logger depends on this so we cannot log anything here.

	DepLibUV::loop = new uv_loop_t;

	const int err = uv_loop_init(DepLibUV::loop);

	if (err != 0)
	{
		MS_ABORT("libuv loop initialization failed");
	}

	uv_async_t* aync    = new uv_async_t();
	DepLibUV::taskQueue = new DepLibUV::AsyncTaskQueue(aync);
	aync->data          = static_cast<void*>(DepLibUV::taskQueue);
}

void DepLibUV::ClassDestroy()
{
	MS_TRACE();

	// Here we should not have any UV handle left. All them should have been
	// already closed+freed. However, in order to not introduce regressions
	// in the future, we check this anyway.
	// More context: https://github.com/versatica/mediasoup/pull/576

	int err;

	delete DepLibUV::taskQueue;

	uv_stop(DepLibUV::loop);
	uv_walk(DepLibUV::loop, onWalk, nullptr);

	while (true)
	{
		err = uv_loop_close(DepLibUV::loop);

		if (err != UV_EBUSY)
		{
			break;
		}

		uv_run(DepLibUV::loop, UV_RUN_NOWAIT);
	}

	if (err != 0)
	{
		MS_ERROR_STD("failed to close libuv loop: %s", uv_err_name(err));
	}

	delete DepLibUV::loop;
}

void DepLibUV::PrintVersion()
{
	MS_TRACE();

	MS_DEBUG_TAG(info, "libuv version: \"%s\"", uv_version_string());
}

void DepLibUV::RunLoop()
{
	MS_TRACE();

	// This should never happen.
	MS_ASSERT(DepLibUV::loop != nullptr, "loop unset");

	const int ret = uv_run(DepLibUV::loop, UV_RUN_DEFAULT);

	MS_ASSERT(ret == 0, "uv_run() returned %s", uv_err_name(ret));
}

/* Instance methods. */

DepLibUV::AsyncTaskQueue::AsyncTaskQueue(uv_async_t* async) : uvAsyncHandle_(async)
{
	uv_async_init(DepLibUV::loop, this->uvAsyncHandle_, runTasks);
}

DepLibUV::AsyncTaskQueue::~AsyncTaskQueue()
{
	uv_close(reinterpret_cast<uv_handle_t*>(this->uvAsyncHandle_), onCloseAsync);
}

void DepLibUV::AsyncTaskQueue::PostTask(std::function<void()> task)
{
	std::lock_guard<std::mutex> lock(this->lock_);

	this->tasks_.push(task);

	uv_async_send(this->uvAsyncHandle_);
}

void DepLibUV::AsyncTaskQueue::RunTask()
{
	std::lock_guard<std::mutex> lock(this->lock_);

	if (this->tasks_.empty())
	{
		return;
	}
	while (!this->tasks_.empty())
	{
		auto task = this->tasks_.front();
		this->tasks_.pop();
		task();
	}
}