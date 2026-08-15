#include "future.hpp"

namespace ezthread
{

template<typename T>
Future<T>::Future(Future&& other) noexcept
	: m_state(std::move(other.m_state)) {}

template<typename T>
Future<T>& Future<T>::operator=(Future&& other) noexcept
{
	if (this != &other)
		m_state = std::move(other.m_state);
	return *this;
}

template<typename T>
Future<T>::Future(std::shared_ptr<SharedState> state)
	: m_state(std::move(state)) {}

template<typename T>
T Future<T>::Get()
{
	if (!m_state)
		throw ThreadException("Future has no state");

	CriticalSection::ScopedLock guard(m_state->mutex);

	while (!m_state->ready)
		m_state->cv.Wait(m_state->mutex);

	if (m_state->hasException)
		std::rethrow_exception(m_state->exception);

	return m_state->value;
}

template<typename T>
bool Future<T>::TryGet(T& result, DWORD timeoutMs)
{
	if (!m_state)
		throw ThreadException("Future has no state");

	CriticalSection::ScopedLock guard(m_state->mutex);

	if (!m_state->ready)
		if (!m_state->cv.WaitFor(m_state->mutex, timeoutMs))
			return false;

	if (m_state->hasException)
		std::rethrow_exception(m_state->exception);

	result = m_state->value;
	return true;
}

template<typename T>
bool Future<T>::IsReady() const
{
	if (!m_state)
		return false;

	CriticalSection::ScopedLock guard(m_state->mutex);
	return m_state->ready;
}

template<typename T>
void Future<T>::Wait() const
{
	if (!m_state)
		throw ThreadException("Future has no state");

	CriticalSection::ScopedLock guard(m_state->mutex);

	while (!m_state->ready)
		m_state->cv.Wait(m_state->mutex);
}

template<typename T>
bool Future<T>::WaitFor(DWORD timeoutMs) const
{
	if (!m_state)
		throw ThreadException("Future has no state");

	CriticalSection::ScopedLock guard(m_state->mutex);

	if (!m_state->ready)
		return m_state->cv.WaitFor(m_state->mutex, timeoutMs);

	return true;
}

template<typename T>
template<typename Func>
auto Future<T>::Then(Func&& func) -> Future<typename std::invoke_result<Func, T>::type>
{
	using ReturnType = typename std::invoke_result<Func, T>::type;

	auto promise = std::make_shared<Promise<ReturnType>>();
	Future<ReturnType> future = promise->GetFuture();

	auto state = m_state;
	auto task = [state, func = std::forward<Func>(func), promise]() mutable
	{
		try
		{
			T value;
			{
				CriticalSection::ScopedLock guard(state->mutex);
				while (!state->ready)
					state->cv.Wait(state->mutex);
				if (state->hasException)
					std::rethrow_exception(state->exception);
				value = state->value;
			}

			if constexpr (std::is_void_v<ReturnType>)
			{
				func(value);
				promise->SetValue();
			}
			else
				promise->SetValue(func(value));
		}
		catch (...)
		{
			promise->SetException(std::current_exception());
		}
	};

	std::thread([task = std::move(task)]() mutable
	{
		task();
	}).detach();

	return future;
}

template<typename T>
Promise<T>::Promise()
	: m_state(std::make_shared<typename Future<T>::SharedState>()) {}

template<typename T>
Promise<T>::Promise(Promise&& other) noexcept
	: m_state(std::move(other.m_state)) {}

template<typename T>
Promise<T>& Promise<T>::operator=(Promise&& other) noexcept
{
	if (this != &other)
		m_state = std::move(other.m_state);
	return *this;
}

template<typename T>
void Promise<T>::SetValue(const T& value)
{
	if (!m_state)
		throw ThreadException("Promise has no state");

	{
		CriticalSection::ScopedLock guard(m_state->mutex);
		if (m_state->ready)
			throw ThreadException("Promise already satisfied");
		m_state->value = value;
		m_state->ready = true;
	}

	m_state->cv.NotifyAll();
}

template<typename T>
void Promise<T>::SetValue(T&& value)
{
	if (!m_state)
		throw ThreadException("Promise has no state");

	{
		CriticalSection::ScopedLock guard(m_state->mutex);
		if (m_state->ready)
			throw ThreadException("Promise already satisfied");
		m_state->value = std::move(value);
		m_state->ready = true;
	}

	m_state->cv.NotifyAll();
}

template<typename T>
void Promise<T>::SetException(std::exception_ptr ex)
{
	if (!m_state)
		throw ThreadException("Promise has no state");

	{
		CriticalSection::ScopedLock guard(m_state->mutex);
		if (m_state->ready)
			throw ThreadException("Promise already satisfied");
		m_state->exception = ex;
		m_state->hasException = true;
		m_state->ready = true;
	}

	m_state->cv.NotifyAll();
}

template<typename T>
Future<T> Promise<T>::GetFuture()
{
	if (!m_state)
		throw ThreadException("Promise has no state");
	return Future<T>(m_state);
}

// void specializations
Future<void>::Future(Future&& other) noexcept
	: m_state(std::move(other.m_state)) {}

Future<void>& Future<void>::operator=(Future&& other) noexcept
{
	if (this != &other)
		m_state = std::move(other.m_state);
	return *this;
}

Future<void>::Future(std::shared_ptr<SharedState> state)
	: m_state(std::move(state)) {}

void Future<void>::Get()
{
	if (!m_state)
		throw ThreadException("Future has no state");

	CriticalSection::ScopedLock guard(m_state->mutex);

	while (!m_state->ready)
		m_state->cv.Wait(m_state->mutex);
	if (m_state->hasException)
		std::rethrow_exception(m_state->exception);
}

bool Future<void>::TryGet(DWORD timeoutMs)
{
	if (!m_state)
		throw ThreadException("Future has no state");

	CriticalSection::ScopedLock guard(m_state->mutex);

	if (!m_state->ready)
		if (!m_state->cv.WaitFor(m_state->mutex, timeoutMs))
			return false;

	if (m_state->hasException)
		std::rethrow_exception(m_state->exception);

	return true;
}

bool Future<void>::IsReady() const
{
	if (!m_state)
		return false;

	CriticalSection::ScopedLock guard(m_state->mutex);
	return m_state->ready;
}

void Future<void>::Wait() const
{
	if (!m_state)
		throw ThreadException("Future has no state");

	CriticalSection::ScopedLock guard(m_state->mutex);

	while (!m_state->ready)
		m_state->cv.Wait(m_state->mutex);
}

bool Future<void>::WaitFor(DWORD timeoutMs) const
{
	if (!m_state)
		throw ThreadException("Future has no state");

	CriticalSection::ScopedLock guard(m_state->mutex);

	if (!m_state->ready)
		return m_state->cv.WaitFor(m_state->mutex, timeoutMs);

	return true;
}

template<typename Func>
auto Future<void>::Then(Func&& func) -> Future<typename std::invoke_result<Func>::type>
{
	using ReturnType = typename std::invoke_result<Func>::type;

	auto promise = std::make_shared<Promise<ReturnType>>();
	Future<ReturnType> future = promise->GetFuture();

	auto& state = m_state;
	auto task = [state, func = std::forward<Func>(func), promise]() mutable
	{
		try
		{
			{
				CriticalSection::ScopedLock guard(state->mutex);
				while (!state->ready)
					state->cv.Wait(state->mutex);
				if (state->hasException)
					std::rethrow_exception(state->exception);
			}

			if constexpr (std::is_void_v<ReturnType>)
			{
				func();
				promise->SetValue();
			}
			else
				promise->SetValue(func());
		}
		catch (...)
		{
			promise->SetException(std::current_exception());
		}
	};

	std::thread([task = std::move(task)]() mutable
	{
		task();
	}).detach();

	return future;
}

Promise<void>::Promise()
	: m_state(std::make_shared<typename Future<void>::SharedState>()) {}

Promise<void>::Promise(Promise&& other) noexcept
	: m_state(std::move(other.m_state)) {}

Promise<void>& Promise<void>::operator=(Promise&& other) noexcept
{
	if (this != &other)
		m_state = std::move(other.m_state);
	return *this;
}

void Promise<void>::SetValue()
{
	if (!m_state)
		throw ThreadException("Promise has no state");

	{
		CriticalSection::ScopedLock guard(m_state->mutex);
		if (m_state->ready)
			throw ThreadException("Promise already satisfied");
		m_state->ready = true;
	}

	m_state->cv.NotifyAll();
}

void Promise<void>::SetException(std::exception_ptr ex)
{
	if (!m_state)
		throw ThreadException("Promise has no state");

	{
		CriticalSection::ScopedLock guard(m_state->mutex);
		if (m_state->ready)
			throw ThreadException("Promise already satisfied");
		m_state->exception = ex;
		m_state->hasException = true;
		m_state->ready = true;
	}

	m_state->cv.NotifyAll();
}

Future<void> Promise<void>::GetFuture()
{
	if (!m_state)
		throw ThreadException("Promise has no state");
	return Future<void>(m_state);
}


template class Future<int>;
template class Future<long>;
template class Future<long long>;
template class Future<unsigned int>;
template class Future<unsigned long>;
template class Future<unsigned long long>;
template class Future<float>;
template class Future<double>;
template class Future<bool>;
template class Future<std::string>;
template class Future<std::wstring>;

template class Promise<int>;
template class Promise<long>;
template class Promise<long long>;
template class Promise<unsigned int>;
template class Promise<unsigned long>;
template class Promise<unsigned long long>;
template class Promise<float>;
template class Promise<double>;
template class Promise<bool>;
template class Promise<std::string>;
template class Promise<std::wstring>;

} // namespace ezthread
