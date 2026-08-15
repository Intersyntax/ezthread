#pragma once

#ifndef EZTHREAD_CONDITION_HPP
#define EZTHREAD_CONDITION_HPP 1


#include "mutex.hpp"

namespace ezthread
{

class ConditionVariable
{
public:
	ConditionVariable() noexcept;
	~ConditionVariable() = default;

	ConditionVariable(const ConditionVariable&) = delete;
	ConditionVariable& operator=(const ConditionVariable&) = delete;

	void Wait(CriticalSection& cs);
	bool WaitFor(CriticalSection& cs, DWORD timeoutMs);

	void Wait(RWLock& lock, bool exclusive);
	bool WaitFor(RWLock& lock, bool exclusive, DWORD timeoutMs);

	void NotifyOne();
	void NotifyAll();

private:
	RTL_CONDITION_VARIABLE m_cv;
};

} // namespace ezthread

#endif // !EZTHREAD_CONDITION_HPP
