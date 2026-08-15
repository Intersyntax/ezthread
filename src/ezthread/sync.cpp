#include "sync.hpp"
#include <algorithm>

namespace ezthread
{

Barrier::Barrier(size_t count)
	: m_count(count)
	, m_generation(0)
	, m_waiting(0)
{
	if (count == 0)
		throw ThreadException("Barrier count must be greater than 0");
}

void Barrier::Wait()
{
	CriticalSection::ScopedLock guard(m_mutex);

	size_t currentGeneration = m_generation;
	m_waiting++;

	if (m_waiting == m_count)
	{
		m_waiting = 0;
		m_generation++;
		m_cv.NotifyAll();
		return;
	}

	while (m_generation == currentGeneration)
		m_cv.Wait(m_mutex);
}

bool Barrier::WaitFor(DWORD timeoutMs)
{
	CriticalSection::ScopedLock guard(m_mutex);

	size_t currentGeneration = m_generation;
	m_waiting++;

	if (m_waiting == m_count)
	{
		m_waiting = 0;
		m_generation++;
		m_cv.NotifyAll();
		return true;
	}

	while (m_generation == currentGeneration)
	{
		if (!m_cv.WaitFor(m_mutex, timeoutMs))
		{
			m_waiting--;
			return false;
		}
	}

	return true;
}

Latch::Latch(size_t count)
	: m_count(count) { }

void Latch::CountDown(size_t count)
{
	CriticalSection::ScopedLock guard(m_mutex);

	if (count > m_count)
		m_count = 0;
	else
		m_count -= count;

	if (m_count == 0)
		m_cv.NotifyAll();
}

void Latch::Wait()
{
	CriticalSection::ScopedLock guard(m_mutex);

	while (m_count > 0)
		m_cv.Wait(m_mutex);
}

bool Latch::WaitFor(DWORD timeoutMs)
{
	CriticalSection::ScopedLock guard(m_mutex);

	while (m_count > 0)
		if (!m_cv.WaitFor(m_mutex, timeoutMs))
			return false;

	return true;
}

bool Latch::IsReady() const
{
	CriticalSection::ScopedLock guard(m_mutex);
	return m_count == 0;
}

}
