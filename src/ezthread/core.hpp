#pragma once

#ifndef EZTHREAD_CORE_HPP
#define EZTHREAD_CORE_HPP 1


#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
	#define NOMINMAX
#endif

#include <Windows.h>
#include <stdexcept>
#include <string>
#include <atomic>
#include <functional>

namespace ezthread
{

class ThreadException : public std::runtime_error
{
public:
	ThreadException(const char* message, DWORD error = GetLastError());
	DWORD ErrorCode() const noexcept;

private:
	DWORD m_errorCode;
};

inline void CheckResult(BOOL result, const char* operation);
inline void CheckResult(HANDLE result, const char* operation);
inline void CheckResult(DWORD result, const char* operation);

#undef Yield

class Thread
{
public:
	using NativeHandle = HANDLE;
	using Id = DWORD;

	enum class PriorityLevel
	{
		Idle = THREAD_PRIORITY_IDLE,
		Lowest = THREAD_PRIORITY_LOWEST,
		BelowNormal = THREAD_PRIORITY_BELOW_NORMAL,
		Normal = THREAD_PRIORITY_NORMAL,
		AboveNormal = THREAD_PRIORITY_ABOVE_NORMAL,
		Highest = THREAD_PRIORITY_HIGHEST,
		TimeCritical = THREAD_PRIORITY_TIME_CRITICAL
	};

	Thread() noexcept = default;

	template<typename Func, typename... Args>
	explicit Thread(Func&& func, Args&&... args)
	{
		auto boundFunc = [func = std::forward<Func>(func),
						  ...args = std::forward<Args>(args)]() mutable
		{
			func(std::forward<Args>(args)...);
		};

		using BoundFuncType = decltype(boundFunc);
		auto funcPtr = new BoundFuncType(std::move(boundFunc));
		//auto funcPtr = new std::function<void()>(std::forward<Func>(func));

		m_handle = CreateThread(
			nullptr,
			0,
			[](LPVOID param) -> DWORD
			{
				auto ptr = static_cast<BoundFuncType*>(param);
				try
				{
					(*ptr)();
					delete ptr;
					return 0;
				}
				catch (...)
				{
					delete ptr;
					return 1;
				}
				/*delete ptr;
				return 0;*/
			},
			funcPtr,
			0,
			&m_id
		);

		if (!m_handle)
		{
			delete funcPtr;
			throw ThreadException("CreateThread failed");
		}
	}

	//template<typename Func, typename Arg1>
	//explicit Thread(Func&& func, Arg1&& arg1)
	//{
	//	auto funcPtr = new std::function<void()>(
	//		[func = std::forward<Func>(func), arg1 = std::forward<Arg1>(arg1)]() {
	//			func(arg1);
	//		}
	//	);

	//	m_handle = CreateThread(
	//		nullptr,
	//		0,
	//		[](LPVOID param) -> DWORD {
	//			auto ptr = static_cast<std::function<void()>*>(param);
	//			try
	//			{
	//				(*ptr)();
	//			}
	//			catch (...) {}
	//			delete ptr;
	//			return 0;
	//		},
	//		funcPtr,
	//		0,
	//		&m_id
	//	);

	//	if (!m_handle)
	//	{
	//		delete funcPtr;
	//		throw ThreadException("CreateThread failed");
	//	}
	//}

	Thread(Thread&& other) noexcept;
	Thread& operator=(Thread&& other) noexcept;

	Thread(const Thread&) = delete;
	Thread& operator=(const Thread&) = delete;

	~Thread();

	void Join();
	bool TryJoin(DWORD timeoutMs);
	void Detach();
	bool Joinable() const noexcept;
	Id GetId() const noexcept;
	NativeHandle GetNativeHandle() const noexcept;

	void SetPriority(PriorityLevel priority) const;
	PriorityLevel GetPriority() const;
	void SetAffinity(uint64_t mask) const;
#ifdef UNICODE
	void SetName(const wchar_t* name) const;
#else
	void SetName(const char* name) const;
#endif

	static void Sleep(DWORD ms);
	static void Yield();
	Id GetCurrentId() const { return m_id; };
	static uint32_t GetHardwareConcurrency();

private:
	//struct ThreadContext
	//{
	//	void* funcPtr;
	//	void* argsPtr;
	//};

	//template<typename Func, typename... Args>
	//static DWORD WINAPI ThreadProc(LPVOID param);

	void Cleanup();

	NativeHandle m_handle = nullptr;
	Id m_id = 0;
};

} // namespace ezthread


#endif // !EZTHREAD_CORE_HPP
