#pragma once

#ifndef EZTHREAD_POOL_HPP
#define EZTHREAD_POOL_HPP 1

#include "future.hpp"
#include <vector>
#include <queue>
#include <functional>
#include "event.hpp"


namespace ezthread
{

class ThreadPool
{
public:
	struct Config
	{
		size_t minThreads = 0;
		size_t maxThreads = std::thread::hardware_concurrency();
		DWORD threadTimeoutMs = 60000;
		bool useWorkStealing = true;
	};

	explicit ThreadPool(const Config& config = Config());
	~ThreadPool();

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	template<typename Func, typename... Args>
	auto Submit(Func&& func, Args&&... args) -> Future<typename std::invoke_result<Func, Args...>::type>
	{
		using ReturnType = typename std::invoke_result<Func, Args...>::type;

		auto promise = std::make_shared<Promise<ReturnType>>();
		Future<ReturnType> future = promise->GetFuture();

		auto boundFunc = [func = std::forward<Func>(func),
						  ...args = std::forward<Args>(args),
						  promise]() mutable
		{
			try
			{
				if constexpr (std::is_void_v<ReturnType>)
				{
					func(std::forward<args>...);
					promise->SetValue();
				}
				else
					promise->SetValue(func(std::forward<Args>(args)...));
			}
			catch (...)
			{
				promise->SetException(std::current_exception());
			}
		};

		auto task = std::make_unique<Task<decltype(boundFunc)>>(std::move(boundFunc));

		{
			CriticalSection::ScopedLock guard(m_queueMutex);
			m_taskQueue.push(std::move(task));
			m_pendingTasks.fetch_add(1, std::memory_order_relaxed);
		}

		m_queueCV.NotifyOne();

		return future;
	}


	void SubmitDetached(std::function<void()> task);

	void ParallelFor(size_t begin, size_t end,
					 const std::function<void(size_t)>& func);

	void Pause();
	void Resume();
	void Stop(bool waitForTasks = true);

	size_t GetPendingTaskCount() const;
	size_t GetThreadCount() const;

private:
	struct TaskBase
	{
		virtual ~TaskBase() = default;
		virtual void Execute() = 0;
	};

	template<typename Func>
	struct Task : TaskBase
	{
		Func func;

		explicit Task(Func&& f) : func(std::move(f)) {}

		void Execute() override { func(); }
	};

	struct WorkerContext
	{
		ThreadPool* pool;
		size_t threadIndex;
	};

	void WorkerLoop(size_t threadIndex);
	void CreateWorker();
	void CleanupIdleWorkers();
	//bool TryExecuteTask();

	mutable CriticalSection m_queueMutex;
	ConditionVariable m_queueCV;
	std::queue<std::unique_ptr<TaskBase>> m_taskQueue;

	mutable CriticalSection m_workersMutex;
	std::vector<std::unique_ptr<Thread>> m_workers;
	std::vector<bool> m_workerActive;

	Config m_config;
	std::atomic<bool> m_stopping;
	std::atomic<bool> m_paused;
	std::atomic<size_t> m_pendingTasks;
	std::atomic<size_t> m_idleWorkers;
};

} // namespace ezthread

#endif // !EZTHREAD_POOL_HPP