#include "async.hpp"

namespace ezthread
{
namespace async
{

Timer::Timer()
	: m_timer(nullptr)
	, m_context(nullptr)
	, m_running(false) { }

Timer::~Timer()
{
	Stop();
}

Timer::Timer(Timer&& other) noexcept
	: m_timer(std::exchange(other.m_timer, nullptr))
	, m_context(std::move(other.m_context))
	, m_running(other.m_running)
{
	other.m_running = false;
}

Timer& Timer::operator=(Timer&& other) noexcept
{
	if (this != &other)
	{
		Stop();
		m_timer = std::exchange(other.m_timer, nullptr);
		m_context = std::move(other.m_context);
		m_running = other.m_running;
		other.m_running = false;
	}
	return *this;
}

void Timer::Start(DWORD dueTimeMs, DWORD periodMs, std::function<void()> callback)
{
	Stop();

	m_context = std::make_unique<TimerContext>();
	m_context->cb = std::move(callback);
	m_context->timer = this;
	m_context->isOneShot = (periodMs==0);

	m_timer = CreateThreadpoolTimer(
		TimerCallback,
		m_context.get(),
		nullptr
	);

	if (!m_timer)
	{
		m_context.reset();
		throw ThreadException("CreateThreadpoolTimer failed");
	}

	FILETIME due;
	LARGE_INTEGER li;
	li.QuadPart = -static_cast<LONGLONG>(dueTimeMs) * 10000;
	due.dwLowDateTime = li.LowPart;
	due.dwHighDateTime = li.HighPart;

	SetThreadpoolTimer(m_timer, &due, periodMs, 0);

	CriticalSection::ScopedLock guard(m_mutex);
	m_running = true;
}

void Timer::Stop()
{
	if (m_timer)
	{
		SetThreadpoolTimer(m_timer, nullptr, 0, 0);
		WaitForThreadpoolTimerCallbacks(m_timer, TRUE);
		CloseThreadpoolTimer(m_timer);
		m_timer = nullptr;
	}

	CriticalSection::ScopedLock guard(m_mutex);
	m_running = false;
	m_context.reset();
}

bool Timer::IsRunning()
{
	CriticalSection::ScopedLock guard(m_mutex);
	return m_running;
}

void Timer::SetOneShot(DWORD dueTimeMs, std::function<void()> callback)
{
	Start(dueTimeMs,0,std::move(callback));
}

void CALLBACK Timer::TimerCallback(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_TIMER timer)
{
	auto ctx = static_cast<TimerContext*>(context);
	if (ctx && ctx->cb)
	{
		try
		{
			ctx->cb();
		}
		catch(...){}

		if (ctx->isOneShot)
		{
			CriticalSection::ScopedLock guard(ctx->timer->m_mutex);
			ctx->timer->m_running = false;
		}
	}
}

Work::Work()
	: m_work(nullptr)
	, m_context(nullptr)
	, m_complete(false) { }

Work::~Work()
{
	if (m_work)
		CloseThreadpoolWork(m_work);
}

Work::Work(Work&& other) noexcept
	: m_work(std::exchange(other.m_work, nullptr))
	, m_context(std::move(other.m_context))
	, m_complete(other.m_complete) { }

Work& Work::operator=(Work&& other) noexcept
{
	if (this != &other)
	{
		if (m_work)
			CloseThreadpoolWork(m_work);
		m_work = std::exchange(other.m_work, nullptr);
		m_context = std::move(other.m_context);
		m_complete = other.m_complete;
	}
	return *this;
}

void Work::Submit(std::function<void()> work)
{
	if (m_work)
		throw ThreadException("Work already submitted");

	m_context = std::make_unique<WorkContext>();
	m_context->work = std::move(work);
	m_context->self = this;

	m_complete = false;

	m_work = CreateThreadpoolWork(
		WorkCallback,
		m_context.get(),
		nullptr
	);

	if (!m_work)
	{
		m_context.reset();
		throw ThreadException("CreateThreadpoolWork failed");
	}

	SubmitThreadpoolWork(m_work);
}

void Work::Cancel()
{
	if (m_work)
	{
		WaitForThreadpoolWorkCallbacks(m_work, TRUE);
		CloseThreadpoolWork(m_work);
		m_work = nullptr;
		m_context.reset();
	}

	CriticalSection::ScopedLock guard(m_mutex);
	m_complete = true;
	m_cv.NotifyAll();
}

bool Work::IsComplete()
{
	CriticalSection::ScopedLock guard(m_mutex);
	return m_complete;
}

void Work::Wait(DWORD timeoutMs)
{
	CriticalSection::ScopedLock guard(m_mutex);

	while (!m_complete)
		if (!m_cv.WaitFor(m_mutex, timeoutMs))
			throw ThreadException("Work wait timeout", WAIT_TIMEOUT);
}

void CALLBACK Work::WorkCallback(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WORK work)
{
	auto ctx = static_cast<WorkContext*>(context);
	if (ctx && ctx->self)
	{
		try
		{
			if (ctx->work)
				ctx->work();
		}
		catch(...){}

		CriticalSection::ScopedLock guard(ctx->self->m_mutex);
		ctx->self->m_complete = true;
		ctx->self->m_cv.NotifyAll();
	}
}

}
}

