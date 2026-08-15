#pragma once

#ifndef EZTHREAD_ASYNC_HPP
#define EZTHREAD_ASYNC_HPP 1

#include "future.hpp"
#include "condition.hpp"
#include "mutex.hpp"

namespace ezthread
{

class AsyncOperation
{
public:
	AsyncOperation() = default;
	virtual ~AsyncOperation() = default;

	AsyncOperation(const AsyncOperation&) = delete;
	AsyncOperation& operator=(const AsyncOperation&) = delete;

	virtual void Cancel() = 0;
	virtual bool IsComplete() = 0;
	virtual void Wait(DWORD timeoutMs = INFINITE) = 0;
};

namespace async
{

	template<typename Func, typename... Args>
	auto Run(Func&& func, Args&&... args) -> Future<decltype(func(args...))>
	{
		using ReturnType = decltype(func(args...));

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
					func(std::forward<Args>(args)...);
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

		auto workContext = new std::function<void()>(std::move(boundFunc));

		PTP_WORK work = CreateThreadpoolWork(
			[](PTP_CALLBACK_INSTANCE instance, PVOID ctx, PTP_WORK work) {
				auto funcPtr = static_cast<std::function<void()>*>(ctx);
				try
				{
					(*funcPtr)();
				}
				catch (...)
				{ /*suppress in asnyc work*/ }
				delete funcPtr;
				CloseThreadpoolWork(work);
			},
			workContext,
			nullptr
		);

		if (!work)
		{
			delete workContext;
			throw ThreadException("CreateThreadpoolWork failed");
		}

		SubmitThreadpoolWork(work);

		return future;
	}

	template<typename Func, typename... Args>
	auto RunDetached(Func&& func, Args&&... args) -> void
	{
		auto boundFunc = [func = std::forward<Func>(func),
						  ...args = std::forward<Args>(args)]() mutable
		{
			try
			{
				func(std::forward<Args>(args)...);
			}
			catch (...)
			{/*suppress in detached work*/ }
		};

		auto workContext = new std::function<void()>(std::move(boundFunc));

		PTP_WORK work = CreateThreadpoolWork(
			[](PTP_CALLBACK_INSTANCE instance, PVOID ctx, PTP_WORK work) {
				auto funcPtr = static_cast<std::function<void()>*>(ctx);
				try
				{
					(*funcPtr)();
				}
				catch (...)
				{ /*suppress in asnyc work*/ }
				delete funcPtr;
				CloseThreadpoolWork(work);
			},
			workContext,
			nullptr
		);

		if (!work)
		{
			delete workContext;
			throw ThreadException("CreateThreadpoolWork failed");
		}

		SubmitThreadpoolWork(work);
	}

	inline void WaitAll(std::vector<Future<void>>& futures, DWORD timeoutMs)
	{
		for (const auto& future:futures)
			if (!future.WaitFor(timeoutMs))
				throw ThreadException("WaitAll timeout",WAIT_TIMEOUT);
	}

	inline bool WaitAll(std::vector<Future<void>>& futures, DWORD timeoutMs, std::vector<bool>& completed)
	{
		completed.clear();
		completed.resize(futures.size());

		bool allCompleted = true;
		for (size_t i=0; i<futures.size(); ++i)
		{
			completed[i] = futures[i].WaitFor(timeoutMs);
			if (!completed[i])
				allCompleted = false;
		}

		return allCompleted;
	}

	template<typename Iterator>
	void WaitAll(Iterator begin, Iterator end, DWORD timeoutMs)
	{
		for (Iterator it=begin; it!=end; ++it)
			if (!it->WaitFor(timeoutMs))
				throw ThreadException("WaitAll timeout", WAIT_TIMEOUT);
	}

	template<typename Iterator>
	bool WaitAll(Iterator begin, Iterator end, DWORD timeoutMs, std::vector<bool>& completed)
	{
		completed.clear();
		size_t c = std::distance(begin, end);
		completed.resize(c);

		bool allCompleted = true;
		size_t idx = 0;
		for (Iterator it=begin; it!=end; ++it, ++idx)
		{
			completed[idx] = it->WaitFor(timeoutMs);
			if (!completed[idx])
				allCompleted = false;
		}

		return allCompleted;
	}


	inline void WaitAny(std::vector<Future<void>>& futures, DWORD timeoutMs)
	{
		if (futures.empty())
			return;

#ifdef _M_X64
		ULONGLONG startTime = GetTickCount64();
#else
		DWORD startTime = GetTickCount();
#endif

		while (true)
		{
			for (const auto& future: futures)
				if (future.IsReady())
					return;

#ifdef _M_X64
			ULONGLONG elapsed = GetTickCount64()-startTime;
#else
			DWORD elapsed = GetTickCount64()-startTime;
#endif
			if (static_cast<DWORD>(elapsed) >= timeoutMs)
				throw ThreadException("WaitAny timeout", WAIT_TIMEOUT);
			
			Thread::Sleep(1);
		}
	}

	inline int WaitAny(std::vector<Future<void>>& futures, DWORD timeoutMs, std::vector<bool>& completed)
	{
		if (futures.empty())
		{
			completed.clear();
			return -1;
		}

		completed.clear();
		completed.resize(futures.size());

#ifdef _M_X64
		ULONGLONG startTime = GetTickCount64();
#else
		DWORD startTime = GetTickCount();
#endif

		while (true)
		{
			for (size_t i=0; i<futures.size(); ++i)
			{
				if (futures[i].IsReady())
				{
					completed[i] = true;
					return static_cast<int>(i);
				}
			}

#ifdef _M_X64
			ULONGLONG elapsed = GetTickCount64()-startTime;
#else
			DWORD elapsed = GetTickCount64()-startTime;
#endif
			if (static_cast<DWORD>(elapsed) >= timeoutMs)
				return -1;

			Thread::Sleep(1);
		}
	}

	template<typename Iterator>
	void WaitAny(Iterator begin, Iterator end, DWORD timeoutMs)
	{
		if (begin==end)return;

#ifdef _M_X64
		ULONGLONG startTime = GetTickCount64();
#else
		DWORD startTime = GetTickCount();
#endif

		while (true)
		{
			for (Iterator it=begin; it!=end; ++it)
				if (it->IsReady())
					return;

#ifdef _M_X64
			ULONGLONG elapsed = GetTickCount64()-startTime;
#else
			DWORD elapsed = GetTickCount64()-startTime;
#endif
			if (static_cast<DWORD>(elapsed) >= timeoutMs)
				throw ThreadException("WaitAny timeout", WAIT_TIMEOUT);

			Thread::Sleep(1);
		}
	}

	template<typename Iterator>
	int WaitAny(Iterator begin, Iterator end, DWORD timeoutMs, std::vector<bool>& completed)
	{
		if (begin==end)
		{
			completed.clear();
			return -1;
		}

		size_t c = std::distance(begin, end);
		completed.clear();
		completed.resize(c);

#ifdef _M_X64
		ULONGLONG startTime = GetTickCount64();
#else
		DWORD startTime = GetTickCount();
#endif

		while (true)
		{
			size_t i = 0;
			for (Iterator it=begin; it!=end;++it,++i)
			{
				if (it->IsReady())
				{
					completed[i] = true;
					return static_cast<int>(i);
				}
			}

#ifdef _M_X64
			ULONGLONG elapsed = GetTickCount64()-startTime;
#else
			DWORD elapsed = GetTickCount64()-startTime;
#endif
			if (static_cast<DWORD>(elapsed) >= timeoutMs)
				return -1;

			Thread::Sleep(1);
		}
	}


	class Timer
	{
	public:
		Timer();
		~Timer();

		Timer(const Timer&) = delete;
		Timer& operator=(const Timer&) = delete;

		Timer(Timer&& other) noexcept;
		Timer& operator=(Timer&& other) noexcept;

		void Start(DWORD dueTimeMs, DWORD periodMs, std::function<void()> callback);
		void Stop();
		bool IsRunning();
		void SetOneShot(DWORD dueTimeMs, std::function<void()> callback);

	private:
		struct TimerContext
		{
			std::function<void()> cb;
			Timer* timer;
			bool isOneShot;
		};

		static void CALLBACK TimerCallback(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_TIMER timer);

		PTP_TIMER m_timer;
		std::unique_ptr<TimerContext> m_context;
		CriticalSection m_mutex;
		bool m_running;
	};

	class Work
	{
	public:
		Work();
		~Work();

		Work(const Work&) = delete;
		Work& operator=(const Work&) = delete;

		Work(Work&& other) noexcept;
		Work& operator=(Work&& other) noexcept;

		void Submit(std::function<void()> work);
		void Cancel();
		bool IsComplete();
		void Wait(DWORD timeoutMs = INFINITE);

	private:
		struct WorkContext
		{
			std::function<void()> work;
			Work* self;
		};

		static void CALLBACK WorkCallback(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WORK work);

		PTP_WORK m_work;
		std::unique_ptr<WorkContext> m_context;
		mutable CriticalSection m_mutex;
		mutable ConditionVariable m_cv;
		bool m_complete;
	};

} // namespace async

} // namespace ezthread

#endif // !EZTHREAD_ASYNC_HPP

