#include "core.hpp"

namespace ezthread
{

ThreadException::ThreadException(const char* message, DWORD error)
	: std::runtime_error(message)
	, m_errorCode(error) { }

DWORD ThreadException::ErrorCode() const noexcept
{
	return m_errorCode;
}

inline void CheckResult(BOOL result, const char* operation)
{
	if (!result)
		throw ThreadException(operation);
}

inline void CheckResult(HANDLE result, const char* operation)
{
	if (result == nullptr || result == INVALID_HANDLE_VALUE)
		throw ThreadException(operation);
}

inline void CheckResult(DWORD result, const char* operation)
{
	if (result == 0)
		throw ThreadException(operation);
}


Thread::Thread(Thread&& other) noexcept
	: m_handle(std::exchange(other.m_handle, nullptr))
	, m_id(std::exchange(other.m_id, 0)) { }


Thread& Thread::operator=(Thread&& other) noexcept
{
	if (this != &other)
	{
		Cleanup();
		m_handle = std::exchange(other.m_handle, nullptr);
		m_id = std::exchange(other.m_id, 0);
	}
	return *this;
}

Thread::~Thread() { Cleanup(); }

void Thread::Cleanup()
{
	if (Joinable())
		Join();
}

void Thread::Join()
{
	if (!Joinable())
		throw ThreadException("Thread is not joinable");

	DWORD result = WaitForSingleObject(m_handle, INFINITE);
	if (result != WAIT_OBJECT_0)
		throw ThreadException("WaitForSingleObject failed");

	CloseHandle(m_handle);
	m_handle = nullptr;
	m_id = 0;
}

bool Thread::TryJoin(DWORD timeoutMs)
{
	if (!Joinable())
		throw ThreadException("Thread is not joinable");

	DWORD result = WaitForSingleObject(m_handle, timeoutMs);
	if (result == WAIT_OBJECT_0)
	{
		CloseHandle(m_handle);
		m_handle = nullptr;
		m_id = 0;
		return true;
	}
	if (result == WAIT_TIMEOUT)
		return false;

	throw ThreadException("WaitForSingleObject failed");
}

void Thread::Detach()
{
	if (!Joinable())
		throw ThreadException("Thread is not joinable");

	CloseHandle(m_handle);
	m_handle = nullptr;
	m_id = 0;
}

bool Thread::Joinable() const noexcept
{
	return m_handle != nullptr;
}

Thread::Id Thread::GetId() const noexcept
{
	return m_id;
}

Thread::NativeHandle Thread::GetNativeHandle() const noexcept
{
	return m_handle;
}

void Thread::SetPriority(PriorityLevel priority) const
{
	if (!m_handle)
		throw ThreadException("Thread handle is invalid");

	if (!SetThreadPriority(m_handle, static_cast<int>(priority)))
		throw ThreadException("SetThreadPriority failed");
}

Thread::PriorityLevel Thread::GetPriority() const
{
	if (!m_handle)
		throw ThreadException("Thread handle is invalid");

	int priority = GetThreadPriority(m_handle);
	if (priority == THREAD_PRIORITY_ERROR_RETURN)
		throw ThreadException("GetThreadPriority failed");

	return static_cast<PriorityLevel>(priority);
}

void Thread::SetAffinity(uint64_t mask) const
{
	if (!m_handle)
		throw ThreadException("Thread handle is invalid");

	DWORD_PTR affinityMask = static_cast<DWORD_PTR>(mask);
	DWORD_PTR result = SetThreadAffinityMask(m_handle, affinityMask);
	if (result == 0)
		throw ThreadException("SetThreadAffinityMask failed");
}

#ifdef UNICODE
void Thread::SetName(const wchar_t* name) const
#else
void Thread::SetName(const char* name) const
#endif
{
	if (!m_handle)
		throw ThreadException("Thread handle is invalid");

#ifndef UNICODE
	wchar_t buffer[128];
	MultiByteToWideChar(CP_UTF8, 0, name, 0, buffer, 0);
	SetThreadDescription(m_handle, buffer);
#else
	SetThreadDescription(m_handle, name);
#endif

	/*typedef struct _THREAD_NAME_INFO
	{
		DWORD dwType;
		LPCSTR szName;
		DWORD dwThreadID;
		DWORD dwFlags;
	} THREAD_NAME_INFO;

	THREAD_NAME_INFO info;
	info.dwType = 0x1000;
	info.szName =
#ifdef UNICODE
		reinterpret_cast<LPCSTR>(name);
#else
		name;
#endif
	info.dwThreadID = m_id;
	info.dwFlags = 0;

	__try
	{
		RaiseException(0x406D1388, 0, sizeof(info)/sizeof(ULONG_PTR),
					   reinterpret_cast<PULONG_PTR>(&info));
	}
	__except(1) {}*/
}

void Thread::Sleep(DWORD ms) { ::Sleep(ms); }

void Thread::Yield() { SwitchToThread(); }

uint32_t Thread::GetHardwareConcurrency()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return static_cast<uint32_t>(si.dwNumberOfProcessors);
}

}

