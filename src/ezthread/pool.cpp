#include "pool.hpp"

namespace ezthread
{

ThreadPool::ThreadPool(const Config& config)
	: m_config(config)
	, m_stopping(false)
	, m_paused(false)
	, m_pendingTasks(0)
	, m_idleWorkers(0)
{
	if (m_config.maxThreads == 0)
		m_config.maxThreads = std::thread::hardware_concurrency();
	if (m_config.minThreads > m_config.maxThreads)
		m_config.minThreads = m_config.maxThreads;

	for (size_t i=0; i<m_config.minThreads; ++i)
		CreateWorker();
}

ThreadPool::~ThreadPool() { Stop(); }



void ThreadPool::SubmitDetached(std::function<void()> task)
{
	auto wrappedTask = [task = std::move(task)]()
	{
		try
		{
			task();
		}
		catch (...) { /* shallow exceptions in detached threads */ }
	};

	auto taskPtr = std::make_unique<Task<decltype(wrappedTask)>>(std::move(wrappedTask));

	{
		CriticalSection::ScopedLock guard(m_queueMutex);
		m_taskQueue.push(std::move(taskPtr));
		m_pendingTasks.fetch_add(1, std::memory_order_relaxed);
	}

	m_queueCV.NotifyOne();
}

void ThreadPool::ParallelFor(size_t begin, size_t end,
							 const std::function<void(size_t)>& func)
{
	if (begin >= end)
		return;

	size_t count = end - begin;
	size_t numThreads = std::min(m_config.maxThreads, count);

	if (numThreads <= 1)
	{
		for (size_t i=begin; i<end; ++i)
			func(i);
		return;
	}

	std::vector<Future<void>> futures;
	futures.reserve(numThreads);

	size_t chunkSize = (count+numThreads-1) / numThreads;

	for (size_t i=0; i<numThreads; ++i)
	{
		size_t chunkBegin = begin+i*chunkSize;
		size_t chunkEnd = std::min(end, chunkBegin + chunkSize);

		if (chunkBegin >= chunkEnd)
			break;

		futures.push_back(Submit([chunkBegin, chunkEnd, &func]()
		{
			for (size_t j=chunkBegin; j<chunkEnd; ++j)
				func(j);
		}));
	}

	for (auto& future : futures)
		future.Wait();
}

void ThreadPool::Pause()
{
	m_paused.store(true, std::memory_order_release);
}

void ThreadPool::Resume()
{
	m_paused.store(false, std::memory_order_release);
	m_queueCV.NotifyAll();
}

void ThreadPool::Stop(bool waitForTasks)
{
	if (m_stopping.exchange(true, std::memory_order_acq_rel))
		return;

	m_queueCV.NotifyAll();

	if (waitForTasks)
		while (m_pendingTasks.load(std::memory_order_acquire) > 0)
			Thread::Sleep(1);

	std::vector<std::unique_ptr<Thread>> workers;
	{
		CriticalSection::ScopedLock guard(m_workersMutex);
		workers = std::move(m_workers);
		m_workers.clear();
		m_workerActive.clear();
	}

	for (auto& worker : workers)
		if (worker && worker->Joinable())
			worker->Join();

	{
		CriticalSection::ScopedLock guard(m_queueMutex);
		while (!m_taskQueue.empty())
			m_taskQueue.pop();
	}

	m_pendingTasks.store(0, std::memory_order_release);
	m_stopping.store(true, std::memory_order_seq_cst);
}

size_t ThreadPool::GetPendingTaskCount() const
{
	return m_pendingTasks.load(std::memory_order_acquire);
}

size_t ThreadPool::GetThreadCount() const
{
	CriticalSection::ScopedLock guard(m_workersMutex);
	return m_workers.size();
}

void ThreadPool::WorkerLoop(size_t threadIndex)
{
	while (!m_stopping.load())
	{
		if (m_paused.load())
		{
			Thread::Sleep(1);
			continue;
		}

		std::unique_ptr<TaskBase> task;

		{
			CriticalSection::ScopedLock guard(m_queueMutex);

			//m_idleWorkers.fetch_add(1, std::memory_order_relaxed);

			while (m_taskQueue.empty() && !m_stopping.load())
				m_queueCV.WaitFor(m_queueMutex, 100);

			if (!m_taskQueue.empty() && !m_stopping.load())
			{
				task = std::move(m_taskQueue.front());
				m_taskQueue.pop();
				m_pendingTasks.fetch_sub(1, std::memory_order_relaxed);
			}
		}

		if (task)
		{
			{
				CriticalSection::ScopedLock guard(m_workersMutex);
				if (threadIndex < m_workerActive.size())
					m_workerActive[threadIndex] = true;
			}

			task->Execute();

			{
				CriticalSection::ScopedLock guard(m_workersMutex);
				if (threadIndex < m_workerActive.size())
					m_workerActive[threadIndex] = false;
			}
		}
	}
}

void ThreadPool::CreateWorker()
{
	CriticalSection::ScopedLock guard(m_workersMutex);

	size_t threadIndex = m_workers.size();

	auto worker = std::make_unique<Thread>([this, threadIndex]()
	{
		WorkerLoop(threadIndex);
	});

	m_workers.push_back(std::move(worker));
	m_workerActive.push_back(false);
}

void ThreadPool::CleanupIdleWorkers()
{
	CriticalSection::ScopedLock guard(m_workersMutex);

	for (size_t i=m_config.minThreads; i<m_workers.size(); ++i)
	{
		if (m_workers[i] && !m_workers[i]->Joinable())
		{
			m_workers[i].reset();
			m_workerActive[i] = false;
		}
	}
}

}
