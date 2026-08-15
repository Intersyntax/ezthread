#pragma once

#ifndef EZTHREAD_TLS_HPP
#define EZTHREAD_TLS_HPP 1

#include "core.hpp"
#include <type_traits>

namespace ezthread
{

template<typename T>
class ThreadLocal
{
public:
	ThreadLocal();
	~ThreadLocal();

	ThreadLocal(const ThreadLocal&) = delete;
	ThreadLocal& operator=(const ThreadLocal&) = delete;

	T& Get();
	T* operator->();
	const T& Get() const;
	const T* operator->() const;

	T& operator*();
	const T& operator*() const;

	bool HasValue() const;

private:
	DWORD m_tlsIndex;

	static void NTAPI DestructorCallback(PVOID value);

	T* GetPointer() const;
	void SetPointer(T* ptr);
};

template<typename T>
class ThreadLocal<T*>
{
public:
	ThreadLocal();
	~ThreadLocal();

	ThreadLocal(const ThreadLocal&) = delete;
	ThreadLocal& operator=(const ThreadLocal&) = delete;

	T* Get();
	T* operator->();
	const T* Get() const;
	const T* operator->() const;

	T*& operator*();
	T* const& operator*() const;

	bool HasValue() const;

private:
	DWORD m_tlsIndex;


	T** GetPointer() const;
	void SetPointer(T* ptr);
};

} // namespace ezthread

#define EZTHREAD_TLS(type, name) static ezthread::ThreadLocal<type> name

#endif // EZTHREAD_TLS_HPP
