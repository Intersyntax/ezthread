#pragma once

#ifndef EZTHREAD_FUTURE_HPP
#define EZTHREAD_FUTURE_HPP 1

#include "mutex.hpp"
#include "condition.hpp"
#include <memory>
#include <type_traits>
#include <thread>

namespace ezthread
{

template<typename T>
class Promise;

template<typename T>
class Future
{
public:
	Future() = default;
	~Future() = default;

	Future(Future&& other) noexcept;
	Future& operator=(Future&& other) noexcept;

	Future(const Future&) = delete;
	Future& operator=(const Future&) = delete;

	T Get();
	bool TryGet(T& result, DWORD timeoutMs);
	bool IsReady() const;
	void Wait() const;
	bool WaitFor(DWORD timeoutMs) const;

	template<typename Func>
	auto Then(Func&& func) -> Future<typename std::invoke_result<Func, T>::type>;

private:
	friend class Promise<T>;

	struct SharedState
	{
		mutable CriticalSection mutex;
		mutable ConditionVariable cv;
		bool ready = false;
		bool hasException = false;
		T value{};
		std::exception_ptr exception;
	};

	std::shared_ptr<SharedState> m_state;

	explicit Future(std::shared_ptr<SharedState> state);
};

template<>
class Future<void>
{
public:
	Future() = default;
	~Future() = default;

	Future(Future&& other) noexcept;
	Future& operator=(Future&& other) noexcept;

	Future(const Future&) = delete;
	Future& operator=(const Future&) = delete;

	void Get();
	bool TryGet(DWORD timeoutMs);
	bool IsReady() const;
	void Wait() const;
	bool WaitFor(DWORD timeoutMs) const;

	template<typename Func>
	auto Then(Func&& func) -> Future<typename std::invoke_result<Func>::type>;

private:
	friend class Promise<void>;

	struct SharedState
	{
		mutable CriticalSection mutex;
		mutable ConditionVariable cv;
		bool ready = false;
		bool hasException = false;
		std::exception_ptr exception;
	};

	std::shared_ptr<SharedState> m_state;

	explicit Future(std::shared_ptr<SharedState> state);
};

template<typename T>
class Promise
{
public:
	Promise();
	~Promise() = default;

	Promise(Promise&& other) noexcept;
	Promise& operator=(Promise&& other) noexcept;

	Promise(const Promise&) = delete;
	Promise& operator=(const Promise&) = delete;

	void SetValue(const T& value);
	void SetValue(T&& value);
	void SetException(std::exception_ptr ex);
	Future<T> GetFuture();

private:
	std::shared_ptr<typename Future<T>::SharedState> m_state;
};

template<>
class Promise<void>
{
public:
	Promise();
	~Promise() = default;

	Promise(Promise&& other) noexcept;
	Promise& operator=(Promise&& other) noexcept;

	Promise(const Promise&) = delete;
	Promise& operator=(const Promise&) = delete;

	void SetValue();
	void SetException(std::exception_ptr ex);
	Future<void> GetFuture();

private:
	std::shared_ptr<typename Future<void>::SharedState> m_state;
};

} // namespace ezthread

#endif // !EZTHREAD_FUTURE_HPP