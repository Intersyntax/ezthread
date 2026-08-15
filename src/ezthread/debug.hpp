#pragma once

#ifndef EZTHREAD_DEBUG_HPP
#define EZTHREAD_DEBUG_HPP 1

#include "mutex.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace ezthread
{

struct DeadlockInfo
{
	void* lockAddress1;
	void* lockAddress2;
	const char* thread1Name;
	const char* thread2Name;
	DWORD thread1Id;
	DWORD thread2Id;
};

class DeadlockDetector
{
public:
	using DeadlockCallback = std::function<void(const DeadlockInfo&)>;

	static void Enable(bool enable = true);
	static bool IsEnabled();
	static void SetCallback(DeadlockCallback callback);

	static void OnLockAcquired(void* lockAddress, const char* lockName = nullptr);
	static void OnLockReleased(void* lockAddress);

private:
	struct LockInfo
	{
		void* address;
		const char* name;
		DWORD threadId;
#ifdef _M_X64
		ULONGLONG acquireTime;
#else
		DWORD acquireTime;
#endif
	};

	static bool m_enabled;
	static DeadlockCallback m_callback;
	static CriticalSection m_mutex;
	static std::unordered_map<void*, LockInfo> m_locks;
	static std::unordered_map<DWORD, std::vector<void*>> m_threadLocks;

	static void CheckForDeadlock(void* lockAddress, DWORD threadId);
	static const char* GetThreadName(DWORD threadId);
};

class ScopedProfiler
{
public:
	explicit ScopedProfiler(const char* name);
	~ScopedProfiler();

	ScopedProfiler(const ScopedProfiler&) = delete;
	ScopedProfiler& operator=(const ScopedProfiler&) = delete;

private:
	const char* m_name;
	LARGE_INTEGER m_startTime;
	LARGE_INTEGER m_frequency;
};

class PerformanceCounter
{
public:
	PerformanceCounter();
	~PerformanceCounter() = default;

	void Start();
	void Stop();
	void Reset();

	double GetElapsedMilliseconds() const;
	double GetElapsedSeconds() const;
	double GetElapsedMicroseconds() const;

	static double GetFrequency();

private:
	LARGE_INTEGER m_startTime;
	LARGE_INTEGER m_endTime;
	LARGE_INTEGER m_frequency;
	bool m_running;
};

#ifdef UNICODE
void SetThreadName(const wchar_t* name);
void SetThreadName(DWORD threadId, const wchar_t* name);
#else
void SetThreadName(const char* name);
void SetThreadName(DWORD threadId, const char* name);
#endif
const char* GetThreadName(DWORD threadId);

void DebugPrint(const char* format, ...);
void DebugPrint(const wchar_t* format, ...);

#ifdef _DEBUG
	#define EZTHREAD_DEBUG_PRINT(...) ezthread::DebugPrint(__VA_ARGS__)
	#define EZTHREAD_PROFILE_SCOPE(name) ezthread::ScopedProfiler profiler##__LINE__(name)
#else
	#define EZTHREAD_DEBUG_PRINT(...)
	#define EZTHREAD_PROFILE_SCOPE(name)
#endif

} // namespace ezthread

#endif // !EZTHREAD_DEBUG_HPP
