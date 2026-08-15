#include "mutex.hpp"

namespace ezthread
{

CriticalSection::CriticalSection()
{
	InitializeCriticalSection(&m_cs);
}

CriticalSection::~CriticalSection()
{
	DeleteCriticalSection(&m_cs);
}

void CriticalSection::Lock()
{
	EnterCriticalSection(&m_cs);
}

bool CriticalSection::TryLock()
{
	return TryEnterCriticalSection(&m_cs) != FALSE;
}

void CriticalSection::Unlock()
{
	LeaveCriticalSection(&m_cs);
}


CriticalSection::ScopedLock::ScopedLock(CriticalSection& cs)
	: m_cs(cs)
{
	m_cs.Lock();
}

CriticalSection::ScopedLock::~ScopedLock()
{
	m_cs.Unlock();
}


#ifdef UNICODE
Mutex::Mutex(bool initiallyOwned, const wchar_t* name)
#else
Mutex::Mutex(bool initiallyOwned, const char* name)
#endif
{
#ifdef UNICODE
	m_handle = CreateMutexW(nullptr, initiallyOwned ? TRUE : FALSE, name);
#else
	m_handle = CreateMutexA(nullptr, initiallyOwned ? TRUE : FALSE, name);
#endif
	if (!m_handle)
		throw ThreadException("CreateMutex failed");
}

Mutex::~Mutex()
{
	if (m_handle)
		CloseHandle(m_handle);
}

Mutex::Mutex(Mutex&& other) noexcept
	: m_handle(std::exchange(other.m_handle, nullptr)) { }

Mutex& Mutex::operator=(Mutex&& other) noexcept
{
	if (this != &other)
	{
		if (m_handle)
			CloseHandle(m_handle);
		m_handle = std::exchange(other.m_handle, nullptr);
	}
	return *this;
}

void Mutex::Lock(DWORD timeoutMs) const
{
	DWORD result = WaitForSingleObject(m_handle, timeoutMs);
	if (result != WAIT_OBJECT_0)
	{
		if (result == WAIT_TIMEOUT)
			throw ThreadException("Mutex lock timeout", WAIT_TIMEOUT);
		if (result == WAIT_ABANDONED)
			throw ThreadException("Mutex was abandoned", WAIT_ABANDONED);
		throw ThreadException("WaitForSingleObject failed");
	}
}

bool Mutex::TryLock() const
{
	DWORD result = WaitForSingleObject(m_handle, 0);
	if (result == WAIT_OBJECT_0)
		return true;
	if (result == WAIT_TIMEOUT)
		return false;
	throw ThreadException("WaitForSingleObject failed");
}

void Mutex::Unlock() const
{
	if (!ReleaseMutex(m_handle))
		throw ThreadException("ReleaseMutex failed");
}

HANDLE Mutex::GetNativeHandle() const noexcept
{
	return m_handle;
}

#ifdef UNICODE
Mutex Mutex::OpenExisting(const wchar_t* name)
#else
Mutex Mutex::OpenExisting(const char* name)
#endif
{
#ifdef UNICODE
	HANDLE handle = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, name);
#else
	HANDLE handle = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, name);
#endif
	if (!handle)
		throw ThreadException("OpenMutex failed");

	Mutex mutex;
	mutex.m_handle = handle;
	return mutex;
}


RWLock::RWLock() noexcept
{
	InitializeSRWLock(&m_lock);
}

void RWLock::LockRead()
{
	AcquireSRWLockShared(&m_lock);
}

bool RWLock::TryLockRead()
{
	return TryAcquireSRWLockShared(&m_lock) != FALSE;
}

void RWLock::UnlockRead()
{
	ReleaseSRWLockShared(&m_lock);
}

void RWLock::LockWrite()
{
	AcquireSRWLockExclusive(&m_lock);
}

bool RWLock::TryLockWrite()
{
	return TryAcquireSRWLockExclusive(&m_lock) != FALSE;
}

void RWLock::UnlockWrite()
{
	ReleaseSRWLockExclusive(&m_lock);
}


RWLock::ReadLock::ReadLock(RWLock& lock)
	: m_lock(lock)
{
	m_lock.LockRead();
}

RWLock::ReadLock::~ReadLock()
{
	m_lock.UnlockRead();
}

RWLock::WriteLock::WriteLock(RWLock& lock)
	: m_lock(lock)
{
	m_lock.LockWrite();
}

RWLock::WriteLock::~WriteLock()
{
	m_lock.UnlockWrite();
}

}

