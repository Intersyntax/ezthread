#pragma once

#ifndef EZTHREAD_ATOMIC_HPP
#define EZTHREAD_ATOMIC_HPP 1

#include "core.hpp"
#include <atomic>
#include <type_traits>


namespace ezthread
{

template<typename T>
class Atomic : public std::atomic<T>
{
public:
	Atomic() noexcept = default;
	constexpr Atomic(T desired) noexcept : std::atomic<T>(desired) {}

	Atomic(const Atomic&) = delete;
	Atomic& operator=(const Atomic&) = delete;

	T FetchAdd(T value, std::memory_order order = std::memory_order_seq_cst)
	{
		return std::atomic<T>::fetch_add(value, order);
	}

	T FetchSub(T value, std::memory_order order = std::memory_order_seq_cst)
	{
		return std::atomic<T>::fetch_sub(value, order);
	}

	template<typename U = T>
	typename std::enable_if<std::is_pointer<U>::value, bool>::type
	CompareExchange(U expected, U desired,
					std::memory_order order = std::memory_order_seq_cst)
	{
		U expectedv = expected;
		return std::atomic<T>::compare_exchange_strong(expectedv, desired, order);;
	}

	template<typename U = T>
	typename std::enable_if<!std::is_pointer<U>::value, bool>::type
	CompareExchange(U expected, U desired,
					std::memory_order order = std::memory_order_seq_cst)
	{
		U expectedv = expected;
		return std::atomic<T>::compare_exchange_strong(expectedv, desired, order);;
	}
};

#undef YieldProcessor

class SpinLock
{
public:
	SpinLock() noexcept = default;
	~SpinLock() = default;

	SpinLock(const SpinLock&) = delete;
	SpinLock& operator=(const SpinLock&) = delete;

	void Lock()
	{
		while (m_flag.test_and_set(std::memory_order_acquire))
			YieldProcessor();
	}

	bool TryLock()
	{
		return !m_flag.test_and_set(std::memory_order_acquire);
	}

	void Unlock()
	{
		m_flag.clear(std::memory_order_release);
	}

private:
	std::atomic_flag m_flag = ATOMIC_FLAG_INIT;

	static void YieldProcessor()
	{
#ifdef _M_X64
		_mm_pause();
#elif defined(_M_IX86)
		__asm pause
#else
		SwitchToThread();
#endif
	}
};

#undef MemoryBarrier

class MemoryBarrier
{
public:
	static void Full()
	{
		std::atomic_thread_fence(std::memory_order_seq_cst);
	}

	static void Acquire()
	{
		std::atomic_thread_fence(std::memory_order_acquire);
	}

	static void Release()
	{
		std::atomic_thread_fence(std::memory_order_release);
	}

	static void Compiler()
	{
		std::atomic_signal_fence(std::memory_order_seq_cst);
	}
};

template<typename T, size_t Capacity>
class SPSCQueue
{
public:
	SPSCQueue() : m_readPos(0), m_writePos(0)
	{
		static_assert((Capacity & (Capacity-1)) == 0, "Capacity must be power of 2");
		static_assert(Capacity > 0, "Capacity must be greater than 0");
	}

	~SPSCQueue() = default;

	SPSCQueue(const SPSCQueue&) = delete;
	SPSCQueue& operator=(const SPSCQueue&) = delete;

	bool Push(const T& item)
	{
		size_t currentWrite = m_writePos.load(std::memory_order_relaxed);
		size_t nextWrite = (currentWrite+1) & (Capacity-1);

		if (nextWrite == m_readPos.load(std::memory_order_acquire))
			return false;

		m_buffer[currentWrite] = item;
		m_writePos.store(nextWrite, std::memory_order_release);
		return true;
	}

	bool Push(T&& item)
	{
		size_t currentWrite = m_writePos.load(std::memory_order_relaxed);
		size_t nextWrite = (currentWrite+1) & (Capacity-1);

		if (nextWrite == m_readPos.load(std::memory_order_acquire))
			return false;

		m_buffer[currentWrite] = std::move(item);
		m_writePos.store(nextWrite, std::memory_order_release);
		return true;
	}

	bool Pop(T& item)
	{
		size_t currentRead = m_readPos.load(std::memory_order_relaxed);

		if (currentRead == m_writePos.load(std::memory_order_acquire))
			return false;

		item = std::move(m_buffer[currentRead]);
		m_readPos.store((currentRead+1) & (Capacity-1), std::memory_order_release);
		return true;
	}

	bool Empty() const
	{
		return m_readPos.load(std::memory_order_acquire) ==
			   m_writePos.load(std::memory_order_acquire);
	}

	size_t Size() const
	{
		size_t writePos = m_writePos.load(std::memory_order_acquire);
		size_t readPos = m_readPos.load(std::memory_order_acquire);

		if (writePos >= readPos)
			return writePos - readPos;
		return Capacity - readPos + writePos;
	}

private:
	alignas(64) T m_buffer[Capacity];
	alignas(64) std::atomic<size_t> m_readPos;
	alignas(64) std::atomic<size_t> m_writePos;
};

template<typename T>
class LockFreeStack
{
public:
	LockFreeStack() : m_head(nullptr) {}

	~LockFreeStack()
	{
		Node* current = m_head.load(std::memory_order_acquire);
		while (current)
		{
			Node* next = current->next;
			delete current;
			current = next;
		}
	}

	LockFreeStack(const LockFreeStack&) = delete;
	LockFreeStack& operator=(const LockFreeStack&) = delete;

	void Push(const T& value)
	{
		Node* newNode = new Node(value);
		Node* oldHead = m_head.load(std::memory_order_relaxed);

		do
		{
			newNode->next = oldHead;
		}
		while (!m_head.compare_exchange_weak(oldHead, newNode,
											 std::memory_order_release,
											 std::memory_order_relaxed));
	}

	void Push(T&& value)
	{
		Node* newNode = new Node(std::move(value));
		Node* oldHead = m_head.load(std::memory_order_relaxed);

		do
		{
			newNode->next = oldHead;
		}
		while (!m_head.compare_exchange_weak(oldHead, newNode,
											 std::memory_order_release,
											 std::memory_order_relaxed));
	}

	bool Pop(T& value)
	{
		Node* current = m_head.load(std::memory_order_acquire);

		while (current)
		{
			if (m_head.compare_exchange_weak(current, current->next,
											 std::memory_order_release,
											 std::memory_order_relaxed))
			{
				value = std::move(current->data);

				return true;
			}
		}
		return false;
	}

	bool Empty() const
	{
		return m_head.load(std::memory_order_acquire) == nullptr;
	}

private:
	struct Node
	{
		T data;
		Node* next;

		explicit Node(const T& val) : data(val), next(nullptr) {}
		explicit Node(T&& val) : data(std::move(val)), next(nullptr) {}
	};

	std::atomic<Node*> m_head;
};

template<typename T, size_t Capacity>
class MPMCQueue
{
public:
	MPMCQueue() : m_head(0), m_tail(0)
	{
		static_assert((Capacity & (Capacity-1)) == 0, "Capacity must be power of 2");
		static_assert(Capacity > 0, "Capacity must be greater than 0");

		for (size_t i=0; i<Capacity; ++i)
			m_buffer[i].sequence.store(i, std::memory_order_relaxed);
	}

	~MPMCQueue() = default;

	MPMCQueue(const MPMCQueue&) = delete;
	MPMCQueue& operator=(const MPMCQueue&) = delete;

	bool Push(const T& item)
	{
		size_t pos = m_tail.load(std::memory_order_relaxed);

		while (true)
		{
			Cell* cell = &m_buffer[pos & (Capacity-1)];
			size_t seq = cell->sequence.load(std::memory_order_acquire);

			intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

			if (diff == 0)
			{
				if (m_tail.compare_exchange_weak(pos, pos+1,
												 std::memory_order_relaxed))
				{
					cell->data = item;
					cell->sequence.store(pos+1, std::memory_order_release);
					return true;
				}
			}
			else if (diff<0)
				return false;
			else
				pos = m_tail.load(std::memory_order_relaxed);
		}
	}

	bool Push(T&& item)
	{
		size_t pos = m_tail.load(std::memory_order_relaxed);

		while (true)
		{
			Cell* cell = &m_buffer[pos & (Capacity-1)];
			size_t seq = cell->sequence.load(std::memory_order_acquire);

			intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

			if (diff == 0)
			{
				if (m_tail.compare_exchange_weak(pos, pos+1,
												 std::memory_order_relaxed))
				{
					cell->data = std::move(item);
					cell->sequence.store(pos+1, std::memory_order_release);
					return true;
				}
			}
			else if (diff<0)
				return false;
			else
				pos = m_tail.load(std::memory_order_relaxed);
		}
	}

	bool Pop(T& item)
	{
		size_t pos = m_head.load(std::memory_order_relaxed);

		while (true)
		{
			Cell* cell = &m_buffer[pos & (Capacity-1)];
			size_t seq = cell->sequence.load(std::memory_order_acquire);

			intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos+1);

			if (diff == 0)
			{
				if (m_head.compare_exchange_weak(pos, pos+1,
												 std::memory_order_relaxed))
				{
					item = std::move(cell->data);
					cell->sequence.store(pos+Capacity, std::memory_order_release);
					return true;
				}
			}
			else if (diff<0)
				return false;
			else
				pos = m_head.load(std::memory_order_relaxed);
		}
	}

	bool Empty() const
	{
		return m_head.load(std::memory_order_acquire) ==
			   m_tail.load(std::memory_order_acquire);
	}

	size_t Size() const
	{
		size_t head = m_head.load(std::memory_order_acquire);
		size_t tail = m_tail.load(std::memory_order_acquire);
		return tail - head;
	}

private:
	struct Cell
	{
		T data;
		std::atomic<size_t> sequence;

		Cell() : data(), sequence(0) {}
	};

	alignas(64) Cell m_buffer[Capacity];
	alignas(64) std::atomic<size_t> m_head;
	alignas(64) std::atomic<size_t> m_tail;
};


} // namespace ezthread


#endif // !EZTHREAD_ATOMIC_HPP

