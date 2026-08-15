#pragma once

#ifndef EZTHREAD_MUTEX_HPP
#define EZTHREAD_MUTEX_HPP 1

#include "core.hpp"
#include <mutex>

namespace ezthread
{

class CriticalSection
{
public:
	CriticalSection();
	~CriticalSection();

	CriticalSection(const CriticalSection&) = delete;
	CriticalSection& operator=(const CriticalSection&) = delete;

	void Lock();
	bool TryLock();
	void Unlock();

	class ScopedLock
	{
	public:
		explicit ScopedLock(CriticalSection& cs);
		~ScopedLock();

		ScopedLock(const ScopedLock&) = delete;
		ScopedLock& operator=(const ScopedLock&) = delete;

	private:
		CriticalSection& m_cs;
	};

private:
	friend class ConditionVariable;

	CRITICAL_SECTION m_cs;
};

using CriticalSectionLock = CriticalSection::ScopedLock;

class Mutex
{
public:
#ifdef UNICODE
	explicit Mutex(bool initiallyOwned = false, const wchar_t* name = nullptr);
#else
	explicit Mutex(bool initiallyOwned = false, const char* name = nullptr);
#endif
	~Mutex();

	Mutex(Mutex&& other) noexcept;
	Mutex& operator=(Mutex&& other) noexcept;

	Mutex(const Mutex&) = delete;
	Mutex& operator=(const Mutex&) = delete;

	void Lock(DWORD timeoutMs = INFINITE) const;
	bool TryLock() const;
	void Unlock() const;

	void unlock() const { Unlock(); }
	void lock() const { Lock(); }

	HANDLE GetNativeHandle() const noexcept;

#ifdef UNICODE
	static Mutex OpenExisting(const wchar_t* name);
#else
	static Mutex OpenExisting(const char* name);
#endif

	//using Lock = std::lock_guard<Mutex>;
	//using UniqueLock = std::unique_lock<Mutex>;

private:
	HANDLE m_handle;
};

class RWLock
{
public:
	RWLock() noexcept;
	~RWLock() = default;

	RWLock(const RWLock&) = delete;
	RWLock& operator=(const RWLock&) = delete;

	void LockRead();
	bool TryLockRead();
	void UnlockRead();

	void LockWrite();
	bool TryLockWrite();
	void UnlockWrite();

	class ReadLock
	{
	public:
		explicit ReadLock(RWLock& lock);
		~ReadLock();

		ReadLock(const ReadLock&) = delete;
		ReadLock& operator=(const ReadLock&) = delete;

	private:
		RWLock& m_lock;
	};

	class WriteLock
	{
	public:
		explicit WriteLock(RWLock& lock);
		~WriteLock();

		WriteLock(const WriteLock&) = delete;
		WriteLock& operator=(const WriteLock&) = delete;

	private:
		RWLock& m_lock;
	};

private:
	friend class ConditionVariable;

	RTL_SRWLOCK m_lock;
};

} // namespace ezthread


#endif // !EZTHREAD_MUTEX_HPP
