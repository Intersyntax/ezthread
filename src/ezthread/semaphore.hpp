#pragma once

#ifndef EZTHREAD_SEMAPHORE_HPP
#define EZTHREAD_SEMAPHORE_HPP 1

#include "core.hpp"

namespace ezthread
{

class Semaphore
{
public:
#ifdef UNICODE
	Semaphore(long initialCount, long maxCount, const wchar_t* name = nullptr);
#else
	Semaphore(long initialCount, long maxCount, const char* name = nullptr);
#endif
	~Semaphore();

	Semaphore(Semaphore&& other) noexcept;
	Semaphore& operator=(Semaphore&& other) noexcept;

	Semaphore(const Semaphore&) = delete;
	Semaphore& operator=(const Semaphore&) = delete;

	void Acquire(DWORD timeoutMs = INFINITE) const;
	bool TryAcquire() const;
	void Release(long count = 1) const;

	HANDLE GetNativeHandle() const noexcept;

#ifdef UNICODE
	static Semaphore OpenExisting(const wchar_t* name);
#else
	static Semaphore OpenExisting(const char* name);
#endif

private:
	HANDLE m_handle;
};

} // namespace ezthread

#endif // !EZTHREAD_SEMAPHORE_HPP

