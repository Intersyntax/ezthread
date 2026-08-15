#include "semaphore.hpp"

namespace ezthread
{

#ifdef UNICODE
Semaphore::Semaphore(long initialCount, long maxCount, const wchar_t* name)
#else
Semaphore::Semaphore(long initialCount, long maxCount, const char* name)
#endif
{
#ifdef UNICODE
	m_handle = CreateSemaphoreW(nullptr, initialCount, maxCount, name);
#else
	m_handle = CreateSemaphoreA(nullptr, initialCount, maxCount, name);
#endif
	if (!m_handle)
		throw ThreadException("CreateSemaphore failed");
}

Semaphore::~Semaphore()
{
	if (m_handle)
		CloseHandle(m_handle);
}

Semaphore::Semaphore(Semaphore&& other) noexcept
	: m_handle(std::exchange(other.m_handle, nullptr)) { }

Semaphore& Semaphore::operator=(Semaphore&& other) noexcept
{
	if (this != &other)
	{
		if (m_handle)
			CloseHandle(m_handle);
		m_handle = std::exchange(other.m_handle, nullptr);
	}
	return *this;
}

void Semaphore::Acquire(DWORD timeoutMs) const
{
	DWORD result = WaitForSingleObject(m_handle, timeoutMs);
	if (result != WAIT_OBJECT_0)
	{
		if (result == WAIT_TIMEOUT)
			throw ThreadException("Semaphore acquire timeout", WAIT_TIMEOUT);
		throw ThreadException("WaitForSingleObject failed");
	}
}

bool Semaphore::TryAcquire() const
{
	DWORD result = WaitForSingleObject(m_handle, 0);
	if (result == WAIT_OBJECT_0)
		return true;
	if (result == WAIT_TIMEOUT)
		return false;
	throw ThreadException("WaitForSingleObject failed");
}

void Semaphore::Release(long count) const
{
	if (!ReleaseSemaphore(m_handle, count, nullptr))
		throw ThreadException("ReleaseSemaphore failed");
}

HANDLE Semaphore::GetNativeHandle() const noexcept
{
	return m_handle;
}

#ifdef UNICODE
Semaphore Semaphore::OpenExisting(const wchar_t* name)
#else
Semaphore Semaphore::OpenExisting(const char* name)
#endif
{
#ifdef UNICODE
	HANDLE handle = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, name);
#else
	HANDLE handle = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, name);
#endif
	if (!handle)
		throw ThreadException("OpenSemaphore failed");

	Semaphore semaphore(0, 1);
	CloseHandle(semaphore.m_handle);
	semaphore.m_handle = handle;
	return semaphore;
}

}

