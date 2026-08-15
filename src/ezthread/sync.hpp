#pragma once

#ifndef EZTHREAD_SYNC_HPP
#define EZTHREAD_SYNC_HPP 1

#include "pool.hpp"
#include <algorithm>

namespace ezthread
{

class Barrier
{
public:
	explicit Barrier(size_t count);
	~Barrier() = default;

	Barrier(const Barrier&) = delete;
	Barrier& operator=(const Barrier&) = delete;

	void Wait();
	bool WaitFor(DWORD timeoutMs);

private:
	size_t m_count;
	size_t m_generation;
	size_t m_waiting;
	CriticalSection m_mutex;
	ConditionVariable m_cv;
};

class Latch
{
public:
	explicit Latch(size_t count);
	~Latch() = default;

	Latch(const Latch&) = delete;
	Latch& operator=(const Latch&) = delete;

	void CountDown(size_t count = 1);
	void Wait();
	bool WaitFor(DWORD timeoutMs);
	bool IsReady() const;

private:
	size_t m_count;
	mutable CriticalSection m_mutex;
	mutable ConditionVariable m_cv;
};

class OnceFlag
{
public:
	OnceFlag() = default;
	~OnceFlag() = default;

	OnceFlag(const OnceFlag&) = delete;
	OnceFlag& operator=(const OnceFlag&) = delete;

	template<typename Func>
	void CallOnce(Func&& func)
	{
		if (m_called.load(std::memory_order_acquire))
			return;

		CriticalSection::ScopedLock guard(m_mutex);

		if (!m_called.load(std::memory_order_relaxed))
		{
			func();
			m_called.store(true, std::memory_order_release);
		}
	}

private:
	std::atomic<bool> m_called{ false };
	CriticalSection m_mutex;
};

template<typename Func>
void CallOnce(OnceFlag& flag, Func&& func)
{
	flag.CallOnce(std::forward<Func>(func));
}

namespace parallel
{

	template<typename Iterator, typename Func>
	void ForEach(Iterator begin, Iterator end, Func func, ThreadPool* pool)
	{
		size_t distance = std::distance(begin, end);

		if (distance == 0)
			return;

		if (pool)
			pool->ParallelFor(0, distance, [&](size_t index)
			{
				Iterator it = begin;
				std::advance(it, index);
				func(*it);
			});
		else
			for (Iterator it=begin; it!=end; ++it)
				func(*it);
	}

	template<typename Iterator, typename T, typename Func>
	T Reduce(Iterator begin, Iterator end, T initial, Func func, ThreadPool* pool)
	{
		size_t distance = std::distance(begin, end);

		if (distance == 0)
			return initial;

		size_t numThreads = 1;
		if (pool)
			numThreads = std::max(size_t(1), std::min(size_t(std::thread::hardware_concurrency()), distance));

		if (numThreads <= 1)
		{
			T result = initial;
			for (Iterator it=begin; it!=end; ++it)
				result = func(result, *it);
			return result;
		}

		std::vector<T> partialResults(numThreads, initial);
		std::vector<Future<void>> futures;
		futures.reserve(numThreads);

		size_t chunkSize = (distance + numThreads-1) / numThreads;

		for (size_t i=0; i<numThreads; ++i)
		{
			size_t chunkBegin = i * chunkSize;
			size_t chunkEnd = std::min(distance, chunkBegin + chunkSize);

			if (chunkBegin >= chunkEnd)
				break;

			futures.push_back(pool->Submit([&, i, chunkBegin, chunkEnd]()
			{
				Iterator it = begin;
				std::advance(it, chunkBegin);

				T localResult = initial;
				for (size_t j=chunkBegin; j<chunkEnd; ++j, ++it)
					localResult = func(localResult, *it);
				partialResults[i] = localResult;
			}));
		}

		for (auto& future : futures)
			future.Wait();

		T finalResult = initial;
		for (const T& partial : partialResults)
			finalResult = func(finalResult, partial);

		return finalResult;
	}

	template<typename Container, typename Comparator>
	void Sort(Container& container, Comparator comp, ThreadPool* pool)
	{
		if (container.size() <= 1)
			return;

		if (!pool || container.size() < 10000)
		{
			std::sort(container.begin(), container.end(), comp);
			return;
		}

		size_t numThreads = std::min(static_cast<size_t>(std::thread::hardware_concurrency()),
									 container.size()/1000);

		if (numThreads <= 1)
		{
			std::sort(container.begin(), container.end(), comp);
			return;
		}

		std::vector<Container> chunks(numThreads);
		size_t chunkSize = (container.size() + numThreads-1) / numThreads;

		std::vector<Future<void>> futures;
		futures.reserve(numThreads);

		for (size_t i=0; i<numThreads; ++i)
		{
			size_t chunkBegin = i * chunkSize;
			size_t chunkEnd = std::min(container.size(), chunkBegin + chunkSize);

			if (chunkBegin >= chunkEnd)
				break;

			futures.push_back(pool->Submit([&, i, chunkBegin, chunkEnd]()
			{
				chunks[i].assign(container.begin() + chunkBegin,
								 container.begin() + chunkEnd);
				std::sort(chunks[i].begin(), chunks[i].end(), comp);
			}));
		}

		for (auto& future : futures)
			future.Wait();

		Container result;
		result.reserve(container.size());

		std::vector<size_t> indices(chunks.size(), 0);
		std::vector<bool> exhausted(chunks.size(), false);

		while (result.size() < container.size())
		{
			size_t bestChunk = SIZE_MAX;
			bool found = false;

			for (size_t i=0; i<chunks.size(); ++i)
			{
				if (!exhausted[i] && indices[i] < chunks[i].size())
				{
					if (!found || comp(chunks[i][indices[i]], chunks[bestChunk][indices[bestChunk]]))
					{
						bestChunk = i;
						found = true;
					}
				}
			}

			if (!found)break;

			result.push_back(std::move(chunks[bestChunk][indices[bestChunk]]));
			indices[bestChunk]++;

			if (indices[bestChunk] >= chunks[bestChunk].size())
				exhausted[bestChunk] = true;
		}

		container = std::move(result);
	}

}
 // namespace parallel

} // namespace ezthread

#endif // !EZTHREAD_SYNC_HPP

