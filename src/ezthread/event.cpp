#include "event.hpp"

namespace ezthread
{

Event::Event(Type type, bool initiallySignaled,
#ifdef UNICODE
		const wchar_t* name
#else
		const char* name
#endif
)
{
	m_handle = 
#ifdef UNICODE
	CreateEventW(
#else
	CreateEventA(
#endif
		nullptr,
		type == Type::ManualReset ? TRUE:FALSE,
		initiallySignaled ? TRUE:FALSE,
		name
	);

	if (!m_handle)
		throw ThreadException("CreateEvent failed");
}

Event::~Event()
{
	if (m_handle)
		CloseHandle(m_handle);
}

Event::Event(Event&& other) noexcept
	: m_handle(std::exchange(other.m_handle, nullptr)) { }

Event& Event::operator=(Event&& other) noexcept
{
	if (this != &other)
	{
		if (m_handle)
			CloseHandle(m_handle);
		m_handle = std::exchange(other.m_handle, nullptr);
	}
	return *this;
}

void Event::Set() const
{
	if (!SetEvent(m_handle))
		throw ThreadException("SetEvent failed");
}

void Event::Reset() const
{
	if (!ResetEvent(m_handle))
		throw ThreadException("ResetEvent failed");
}

bool Event::Wait(DWORD timeoutMs) const
{
	DWORD result = WaitForSingleObject(m_handle, timeoutMs);
	if (result == WAIT_OBJECT_0)
		return true;
	if (result == WAIT_TIMEOUT)
		return false;
	throw ThreadException("WaitForSingleObject failed");
}

bool Event::IsSignaled() const
{
	DWORD result = WaitForSingleObject(m_handle, 0);
	if (result == WAIT_OBJECT_0)
		return true;
	if (result == WAIT_TIMEOUT)
		return false;
	throw ThreadException("WaitForSingleObject failed");
}

HANDLE Event::GetNativeHandle() const noexcept
{
	return m_handle;
}

#ifdef UNICODE
Event Event::OpenExisting(const wchar_t* name)
#else
Event Event::OpenExisting(const char* name)
#endif
{
#ifdef UNICODE
	HANDLE handle = OpenEventW(EVENT_ALL_ACCESS, FALSE, name);
#else
	HANDLE handle = OpenEventA(EVENT_ALL_ACCESS, FALSE, name);
#endif
	if (!handle)
		throw ThreadException("OpenEvent failed");

	Event event;
	event.m_handle = handle;
	return event;
}

}
