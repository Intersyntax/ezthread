#include "debug.hpp"

namespace ezthread
{

bool DeadlockDetector::m_enabled = false;
DeadlockDetector::DeadlockCallback DeadlockDetector::m_callback;
CriticalSection DeadlockDetector::m_mutex;
std::unordered_map<void*, DeadlockDetector::LockInfo> DeadlockDetector::m_locks;
std::unordered_map<DWORD, std::vector<void*>> DeadlockDetector::m_threadLocks;


void DeadlockDetector::Enable(bool enable)
{
	m_enabled = enable;
}

bool DeadlockDetector::IsEnabled()
{
	return m_enabled;
}

void DeadlockDetector::SetCallback(DeadlockCallback callback)
{
	m_callback = std::move(callback);
}

void DeadlockDetector::OnLockAcquired(void* lockAddress, const char* lockName)
{
	if (!m_enabled || !lockAddress)
		return;

	CriticalSection::ScopedLock guard(m_mutex);

	DWORD tid = GetCurrentThreadId();
#ifdef _M_X64
	ULONGLONG currentTime = GetTickCount64();
#else
	DWORD currentTime = GetTickCount();
#endif

	LockInfo info;
	info.address = lockAddress;
	info.name = lockName;
	info.threadId = tid;
	info.acquireTime = currentTime;

	m_locks[lockAddress] = info;
	m_threadLocks[tid].push_back(lockAddress);

	CheckForDeadlock(lockAddress, tid);
}

void DeadlockDetector::OnLockReleased(void* lockAddress)
{
	if (!m_enabled || !lockAddress)
		return;

	CriticalSection::ScopedLock guard(m_mutex);

	DWORD tid = GetCurrentThreadId();

	auto lockIt = m_locks.find(lockAddress);
	if (lockIt != m_locks.end())
		m_locks.erase(lockIt);

	auto threadIt = m_threadLocks.find(tid);
	if (threadIt != m_threadLocks.end())
	{
		auto& locks = threadIt->second;
		locks.erase(std::remove(locks.begin(), locks.end(), lockAddress), locks.end());
		if (locks.empty())
			m_threadLocks.erase(threadIt);
	}
}

void DeadlockDetector::CheckForDeadlock(void* lockAddress, DWORD threadId)
{
	auto lockIt = m_locks.find(lockAddress);
	if (lockIt == m_locks.end() || lockIt->second.threadId == threadId)
		return;

	DWORD ownerTid = lockIt->second.threadId;
	auto ownerLocksIt = m_threadLocks.find(ownerTid);

	if (ownerLocksIt == m_threadLocks.end())
		return;

	auto currentThreadLocksIt = m_threadLocks.find(threadId);
	if (currentThreadLocksIt == m_threadLocks.end())
		return;

	for (void* ownerLock : ownerLocksIt->second)
	{
		for (void* currentLock : currentThreadLocksIt->second)
		{
			if (ownerLock == currentLock)
			{
				DeadlockInfo info;
				info.lockAddress1 = lockAddress;
				info.lockAddress2 = ownerLock;
				info.thread1Name = GetThreadName(threadId);
				info.thread2Name = GetThreadName(ownerTid);
				info.thread1Id = threadId;
				info.thread2Id = ownerTid;

				if (m_callback)
					m_callback(info);

				DebugPrint("DEADLOCK DETECTED: Thread %lu (%s) and Thread %lu (%s) are deadlocked on locks %p and %p\n",
						   threadId, info.thread1Name, ownerTid, info.thread2Name, lockAddress, ownerLock);
				return;
			}
		}
	}
}


const char* DeadlockDetector::GetThreadName(DWORD threadId)
{
	return ezthread::GetThreadName(threadId);
}


ScopedProfiler::ScopedProfiler(const char* name)
	: m_name(name)
{
	QueryPerformanceFrequency(&m_frequency);
	QueryPerformanceCounter(&m_startTime);
}

ScopedProfiler::~ScopedProfiler()
{
	LARGE_INTEGER endTime;
	QueryPerformanceCounter(&endTime);

	double elapsedMs = (endTime.QuadPart - m_startTime.QuadPart)*1000. / m_frequency.QuadPart;

	DebugPrint("[PROFILER] %s: %.3f ms \n", m_name, elapsedMs);
}

PerformanceCounter::PerformanceCounter()
	: m_running(false)
{
	QueryPerformanceFrequency(&m_frequency);
	m_startTime.QuadPart = 0;
	m_endTime.QuadPart = 0;
}

void PerformanceCounter::Start()
{
	QueryPerformanceCounter(&m_startTime);
	m_running = true;
}

void PerformanceCounter::Stop()
{
	if (m_running)
	{
		QueryPerformanceCounter(&m_endTime);
		m_running = false;
	}
}

void PerformanceCounter::Reset()
{
	m_startTime.QuadPart = 0;
	m_endTime.QuadPart = 0;
	m_running = false;
}

double PerformanceCounter::GetElapsedMilliseconds() const
{
	if (m_running)
	{
		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);
		return (currentTime.QuadPart - m_startTime.QuadPart)*1000. / m_frequency.QuadPart;
	}
	return (m_endTime.QuadPart - m_startTime.QuadPart)*1000. / m_frequency.QuadPart;
}

double PerformanceCounter::GetElapsedSeconds() const
{
	return GetElapsedMilliseconds() / 1000.;
}

double PerformanceCounter::GetElapsedMicroseconds() const
{
	return GetElapsedMilliseconds() * 1000.;
}

double PerformanceCounter::GetFrequency()
{
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	return static_cast<double>(frequency.QuadPart);
}

#ifdef UNICODE
void SetThreadName(const wchar_t* name)
#else
void SetThreadName(const char* name)
#endif
{
	SetThreadName(GetCurrentThreadId(), name);
}

#ifdef UNICODE
void SetThreadName(DWORD threadId, const wchar_t* name)
#else
void SetThreadName(DWORD threadId, const char* name)
#endif
{
//	typedef struct _THREAD_NAME_INFO
//	{
//		DWORD dwType;
//		LPCSTR szName;
//		DWORD dwThreadID;
//		DWORD dwFlags;
//	} THREAD_NAME_INFO;
//
//	THREAD_NAME_INFO info;
//	info.dwType = 0x1000;
//	info.szName =
//#ifdef UNICODE
//		reinterpret_cast<LPCSTR>(name);
//#else
//		name;
//#endif
//	info.dwThreadID = threadId;
//	info.dwFlags = 0;
//
//	__try
//	{
//		RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR),
//					   reinterpret_cast<PULONG_PTR>(&info));
//	}
//	__except (1) {}
	HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, threadId);
	if (!hThread || hThread == INVALID_HANDLE_VALUE)
		throw ThreadException("OpenThread Failed");
#ifdef UNICODE
	SetThreadDescription(hThread, name);
#else
	wchar_t buffer[128];
	MultiByteToWideChar(CP_UTF8, 0, name, -1, buffer, sizeof(buffer));
	SetThreadDescription(hThread, buffer);
#endif
	CloseHandle(hThread);
}


const char* GetThreadName(DWORD threadId)
{
	static thread_local char buffer[256];
	static thread_local std::wstring wideBuffer;

	HANDLE hThread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threadId);
	if (!hThread || hThread == INVALID_HANDLE_VALUE)
	{
		snprintf(buffer, sizeof(buffer), "Thread-%lu", threadId);
		return buffer;
	}

	PWSTR description = nullptr;
	HRESULT hr = GetThreadDescription(hThread, &description);
	CloseHandle(hThread);

	if (SUCCEEDED(hr) && description)
	{
#ifdef UNICODE
		snprintf(buffer, sizeof(buffer), "%ls", description);
#else
		WideCharToMultiByte(CP_UTF8, 0, description, -1, buffer, sizeof(buffer), nullptr, nullptr);
#endif
		LocalFree(description);
	}
	else
		snprintf(buffer, sizeof(buffer), "Thread-%lu", threadId);

	return buffer;
}

void DebugPrint(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buffer[4096];
	vsnprintf_s(buffer, sizeof(buffer)/sizeof(char), 4096, format, args);
	va_end(args);

	OutputDebugStringA(buffer);
}

void DebugPrint(const wchar_t* format, ...)
{
	va_list args;
	va_start(args, format);
	wchar_t buffer[4096];
	_vsnwprintf_s(buffer, sizeof(buffer)/sizeof(wchar_t), 4096, format, args);
	va_end(args);

	OutputDebugStringW(buffer);
}

}
