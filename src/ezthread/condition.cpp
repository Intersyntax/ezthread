#include "condition.hpp"

#define RTL_CONDITION_VARIABLE_LOCKMODE_EXCLUSIVE 0x0

namespace ezthread
{

ConditionVariable::ConditionVariable() noexcept
{
	InitializeConditionVariable(&m_cv);
}

void ConditionVariable::Wait(CriticalSection& cs)
{
	BOOL result = SleepConditionVariableCS(&m_cv, &cs.m_cs, INFINITE);
	if (!result)
	{
		DWORD error = GetLastError();
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "SleepConditionVariableCS failed with error %lu", error);
		throw ThreadException(buffer, error);
	}
}

bool ConditionVariable::WaitFor(CriticalSection& cs, DWORD timeoutMs)
{
	if (!SleepConditionVariableCS(&m_cv, &cs.m_cs, timeoutMs))
	{
		DWORD error = GetLastError();
		if (error == ERROR_TIMEOUT)
			return false;
		throw ThreadException("SleepConditionVariableCS failed", error);
	}
	return true;
}

void ConditionVariable::Wait(RWLock& lock, bool exclusive)
{
	if (!SleepConditionVariableSRW(&m_cv, &lock.m_lock, INFINITE,
								   exclusive ? RTL_CONDITION_VARIABLE_LOCKMODE_EXCLUSIVE :
											   RTL_CONDITION_VARIABLE_LOCKMODE_SHARED))
		throw ThreadException("SleepConditionVariableSRW failed");
}

bool ConditionVariable::WaitFor(RWLock& lock, bool exclusive, DWORD timeoutMs)
{
	if (!SleepConditionVariableSRW(&m_cv, &lock.m_lock, INFINITE,
								   exclusive ? RTL_CONDITION_VARIABLE_LOCKMODE_EXCLUSIVE :
											   RTL_CONDITION_VARIABLE_LOCKMODE_SHARED))
	{
		DWORD error = GetLastError();
		if (error == ERROR_TIMEOUT)
			return false;
		throw ThreadException("SleepConditionVariableSRW failed", error);
	}
	return true;
}

void ConditionVariable::NotifyOne()
{
	WakeConditionVariable(&m_cv);
}

void ConditionVariable::NotifyAll()
{
	WakeAllConditionVariable(&m_cv);
}

}
