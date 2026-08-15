#include "tls.hpp"

namespace ezthread
{

template<typename T>
ThreadLocal<T>::ThreadLocal()
{
	m_tlsIndex = TlsAlloc();
	if (m_tlsIndex == TLS_OUT_OF_INDEXES)
		throw ThreadException("TlsAlloc failed");
}

template<typename T>
ThreadLocal<T>::~ThreadLocal() {
	if (m_tlsIndex != TLS_OUT_OF_INDEXES)
		TlsFree(m_tlsIndex);
}

template<typename T>
T& ThreadLocal<T>::Get()
{
	T* ptr = GetPointer();
	if (!ptr)
	{
		ptr = new T();
		SetPointer(ptr);
	}
	return *ptr;
}

template<typename T>
T* ThreadLocal<T>::operator->()
{
	return &Get();
}

template<typename T>
const T& ThreadLocal<T>::Get() const
{
	T* ptr = GetPointer();
	if (!ptr)
		throw ThreadException("Thread-local value not initialized");
	return *ptr;
}

template<typename T>
const T* ThreadLocal<T>::operator->() const
{
	return &Get();
}

template<typename T>
T& ThreadLocal<T>::operator*()
{
	return Get();
}

template<typename T>
const T& ThreadLocal<T>::operator*() const
{
	return Get();
}

template<typename T>
bool ThreadLocal<T>::HasValue() const
{
	return GetPointer() != nullptr;
}

template<typename T>
void NTAPI ThreadLocal<T>::DestructorCallback(PVOID value)
{
	T* ptr = static_cast<T*>(value);
	delete ptr;
}

template<typename T>
T* ThreadLocal<T>::GetPointer() const
{
	return static_cast<T*>(TlsGetValue(m_tlsIndex));
}

template<typename T>
void ThreadLocal<T>::SetPointer(T* ptr)
{
	if (!TlsSetValue(m_tlsIndex, ptr))
	{
		delete ptr;
		throw ThreadException("TlsSetValue failed");
	}
}



template<typename T>
ThreadLocal<T*>::ThreadLocal()
{
	m_tlsIndex = TlsAlloc();
	if (m_tlsIndex == TLS_OUT_OF_INDEXES)
		throw ThreadException("TlsAlloc failed");
}

template<typename T>
ThreadLocal<T*>::~ThreadLocal()
{
	if (m_tlsIndex != TLS_OUT_OF_INDEXES)
		TlsFree(m_tlsIndex);
}

template<typename T>
T* ThreadLocal<T*>::Get()
{
	T** slot = static_cast<T**>(TlsGetValue(m_tlsIndex));
	if (!slot)
	{
		slot = new T * (nullptr);
		TlsSetValue(m_tlsIndex, slot);
	}
	return *slot;
}

template<typename T>
T* ThreadLocal<T*>::operator->()
{
	return Get();
}

template<typename T>
const T* ThreadLocal<T*>::Get() const
{
	T** slot = static_cast<T**>(TlsGetValue(m_tlsIndex));
	if (!slot || !*slot)
		throw ThreadException("Thread-local value not initialized");
	return *slot;
}

template<typename T>
const T* ThreadLocal<T*>::operator->() const
{
	return Get();
}

template<typename T>
T*& ThreadLocal<T*>::operator*()
{
	T** slot = static_cast<T**>(TlsGetValue(m_tlsIndex));
	if (!slot)
	{
		slot = new T * (nullptr);
		TlsSetValue(m_tlsIndex, slot);
	}
	return *slot;
}

template<typename T>
T* const& ThreadLocal<T*>::operator*() const
{
	T** slot = static_cast<T**>(TlsGetValue(m_tlsIndex));
	if (!slot)
		throw ThreadException("Thread-local value not initialized");
	return *slot;
}

template<typename T>
bool ThreadLocal<T*>::HasValue() const
{
	T** slot = static_cast<T**>(TlsGetValue(m_tlsIndex));
	return slot != nullptr && *slot != nullptr;
}

template class ThreadLocal<int>;
template class ThreadLocal<long>;
template class ThreadLocal<long long>;
template class ThreadLocal<unsigned int>;
template class ThreadLocal<unsigned long>;
template class ThreadLocal<unsigned long long>;
template class ThreadLocal<float>;
template class ThreadLocal<double>;
template class ThreadLocal<bool>;
template class ThreadLocal<std::string>;
template class ThreadLocal<std::wstring>;

template class ThreadLocal<int*>;
template class ThreadLocal<long*>;
template class ThreadLocal<long long*>;
template class ThreadLocal<unsigned int*>;
template class ThreadLocal<unsigned long*>;
template class ThreadLocal<unsigned long long*>;
template class ThreadLocal<float*>;
template class ThreadLocal<double*>;
template class ThreadLocal<bool*>;

}
