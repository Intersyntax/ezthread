#pragma once

#ifndef EZTHREAD_EVENT_HPP
#define EZTHREAD_EVENT_HPP 1

#include "core.hpp"

namespace ezthread
{

class Event
{
public:
	enum class Type
	{
		AutoReset,
		ManualReset
	};

	Event(Type type = Type::AutoReset, bool initiallySignaled = false,
#ifdef UNICODE
		const wchar_t* name
#else
		const char* name
#endif
		= nullptr);
	~Event();

	Event(Event&& other) noexcept;
	Event& operator=(Event&& other) noexcept;

	Event(const Event&) = delete;
	Event& operator=(const Event&) = delete;

	void Set() const;
	void Reset() const;
	bool Wait(DWORD timeoutMs = INFINITE) const;
	bool IsSignaled() const;

	HANDLE GetNativeHandle() const noexcept;

#ifdef UNICODE
	static Event OpenExisting(const wchar_t* name);
#else
	static Event OpenExisting(const char* name);
#endif

private:
	HANDLE m_handle;
};

} // namespace ezthread

#endif // !EZTHREAD_EVENT_HPP

