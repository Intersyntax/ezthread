#include <ezthread.hpp>
#include <iostream>
#include <cassert>
#include <numeric>
#include <algorithm>

using namespace ezthread;

void TestBasicThread()
{
	std::cout << "Testing basic thread operations...\n";

	std::atomic<int> counter{ 0 };

	Thread thread([&counter]() {
		for (int i=0; i<1000; ++i)
			counter.fetch_add(1, std::memory_order_relaxed);
	});

	assert(thread.Joinable());
	thread.Join();
	assert(!thread.Joinable());
	assert(counter.load() == 1000);

	std::cout << "  Basic thread operation passed" << std::endl;
}

void TestThreadPriority()
{
	std::cout << "Testing thread priority...\n";

	std::atomic<bool> running{ true };

	Thread thread([&running]() {
		while (running.load(std::memory_order_relaxed))
			Thread::Sleep(10);
	});

	thread.SetPriority(Thread::PriorityLevel::AboveNormal);
	auto priority = thread.GetPriority();
	assert(priority == Thread::PriorityLevel::AboveNormal);

	running.store(false, std::memory_order_relaxed);
	thread.Join();

	std::cout << "  Thread priority test passed" << std::endl;
}

void TestCriticalSection()
{
	std::cout << "Testing critical section...\n";

	CriticalSection cs;
	std::atomic<int> counter{ 0 };

	std::vector<Thread> threads;
	for (int i = 0; i < 4; ++i)
	{
		threads.emplace_back([&cs, &counter]() {
			for (int j=0; j<10000; ++j)
			{
				CriticalSection::ScopedLock guard(cs);
				counter.fetch_add(1, std::memory_order_relaxed);
			}
		});
	}

	for (auto& thread : threads)
		thread.Join();

	assert(counter.load() == 40000);

	std::cout << "  Critical section test passed" << std::endl;
}

void TestMutex()
{
	std::cout << "Testing mutex...\n";

	Mutex mutex;
	std::atomic<int> counter{ 0 };

	std::vector<Thread> threads;
	for (int i = 0; i < 4; ++i)
	{
		threads.emplace_back([&mutex, &counter]() {
			for (int j=0; j<10000; ++j)
			{
				std::lock_guard<Mutex> guard(mutex);
				counter.fetch_add(1, std::memory_order_relaxed);
			}
		});
	}

	for (auto& thread : threads)
		thread.Join();

	assert(counter.load() == 40000);

	std::cout << "  Mutex test passed" << std::endl;
}

void TestConditionVariable()
{
	std::cout << "Testing condition variable...\n";

	CriticalSection cs;
	ConditionVariable cv;
	bool ready = false;
	std::vector<int> data;

	Thread producer([&]() {
		Thread::Sleep(10);
		{
			CriticalSection::ScopedLock guard(cs);
			for (int i=0; i<10; ++i)
				data.push_back(i);
			ready = true;
		}
		cv.NotifyOne();
	});

	Thread consumer([&]() {
		CriticalSection::ScopedLock guard(cs);
		while (!ready)
			cv.Wait(cs);

		assert(data.size() == 10);
		for (int i=0; i<10; ++i)
			assert(data[i]==i);
	});

	producer.Join();
	consumer.Join();

	std::cout << "  Condition variable test passed" << std::endl;
}

void TestSemaphore()
{
	std::cout << "Testing semaphore...\n";

	Semaphore semaphore(0, 10);
	std::atomic<int> counter{ 0 };

	std::vector<Thread> consumers;
	for (int i=0; i<3;++i)
	{
		consumers.emplace_back([&semaphore, &counter]() {
			semaphore.Acquire();
			counter.fetch_add(1, std::memory_order_relaxed);
		});
	}

	Thread::Sleep(10);

	for (int i=0; i<3; ++i)
		semaphore.Release();

	for (auto& consumer : consumers)
		consumer.Join();

	assert(counter.load() == 3);

	std::cout << "  Semaphore test passed" << std::endl;
}

void TestEvent()
{
	std::cout << "Testing event...\n";

	Event autoEvent(Event::Type::AutoReset);
	Event manualEvent(Event::Type::ManualReset);
	std::atomic<int> counter{ 0 };

	Thread thread1([&]() {
		autoEvent.Wait();
		counter.fetch_add(1, std::memory_order_relaxed);
	});

	Thread thread2([&]() {
		manualEvent.Wait();
		counter.fetch_add(1, std::memory_order_relaxed);
	});

	Thread thread3([&]() {
		manualEvent.Wait();
		counter.fetch_add(1, std::memory_order_relaxed);
	});

	Thread::Sleep(10);

	autoEvent.Set();
	manualEvent.Set();

	thread1.Join();
	thread2.Join();
	thread3.Join();

	assert(counter.load() == 3);

	std::cout << "  Event test passed" << std::endl;
}

void TestRWLock()
{
	std::cout << "Testing read-write lock...\n";

	RWLock rwLock;
	std::atomic<int> readers{ 0 };
	std::atomic<int> writers{ 0 };
	std::atomic<int> data{ 0 };

	std::vector<Thread> threads;

	// reader threads
	for (int i=0; i<3; ++i)
	{
		threads.emplace_back([&]() {
			for (int j=0; j<100; ++j)
			{
				RWLock::ReadLock guard(rwLock);
				readers.fetch_add(1, std::memory_order_relaxed);
				Thread::Sleep(1);
				readers.fetch_sub(1, std::memory_order_relaxed);
			}
		});
	}

	// writer threads
	for (int i=0; i<2; ++i)
	{
		threads.emplace_back([&]() {
			for (int j=0; j<100; ++j)
			{
				RWLock::WriteLock guard(rwLock);
				writers.fetch_add(1, std::memory_order_relaxed);
				data.fetch_add(1, std::memory_order_relaxed);
				writers.fetch_sub(1, std::memory_order_relaxed);
			}
		});
	}

	for (auto& thread : threads)
		thread.Join();

	assert(data.load() == 200);

	std::cout << "  Read-write lock test passed" << std::endl;
}

void TestThreadPool()
{
	std::cout << "Testing thread pool...\n";

	ThreadPool::Config config;
	config.minThreads = 2;
	config.maxThreads = 4;

	ThreadPool pool(config);

	auto future1 = pool.Submit([]() {
		return 123;
	});

	auto future2 = pool.Submit([](int x) {
		return x*2;
	}, 52);

	assert(future1.Get() == 123);
	assert(future2.Get() == 104);


	std::vector<int> data(1000);
	pool.ParallelFor(0, data.size(), [&](size_t i) {
		data[i] = static_cast<int>(i * i);
	});

	for (size_t i=0; i<data.size(); ++i)
		assert(data[i] == static_cast<int>(i*i));


	std::atomic<int> counter{ 0 };
	for (int i=0; i<100; ++i)
	{
		pool.SubmitDetached([&counter]() {
			counter.fetch_add(1, std::memory_order_relaxed);
		});
	}

	while (pool.GetPendingTaskCount() > 0)
		Thread::Sleep(1);

	assert(counter.load() == 100);

	std::cout << "  Thread pool test passed" << std::endl;
}

void TestFuture()
{
	std::cout << "Testing futures and promises...\n";

	Promise<int> promise;
	Future<int> future = promise.GetFuture();

	Thread thread([&promise]() {
		Thread::Sleep(10);
		promise.SetValue(555);
	});

	assert(future.Get() == 555);
	thread.Join();

	Promise<void> voidPromise;
	Future<void> voidFuture = voidPromise.GetFuture();

	Thread voidThread([&voidPromise]() {
		Thread::Sleep(10);
		voidPromise.SetValue();
	});

	voidFuture.Wait();
	voidThread.Join();

	//

	Promise<int> exceptionPromise;
	Future<int> exceptionFuture = exceptionPromise.GetFuture();

	Thread exceptionThread([&exceptionPromise]() {
		try
		{
			throw std::runtime_error("Test");

		}
		catch (...)
		{
			exceptionPromise.SetException(std::current_exception());
		}
	});

	bool caught = false;
	try
	{
		exceptionFuture.Get();
	}
	catch (const std::runtime_error& e)
	{
		caught = true;
		assert(std::string(e.what()) == "Test");
	}

	assert(caught);
	exceptionThread.Join();

	std::cout << "  Future test passed" << std::endl;
}

void TestLockFreeQueue()
{
	std::cout << "Testing lock-free queue...\n";

	SPSCQueue<int, 1024> queue;

	Thread producer([&queue]() {
		for (int i=0; i<1000; ++i)
			while (!queue.Push(i))
				Thread::Yield();
	});

	Thread consumer([&queue]() {
		for (int i=0; i<1000; ++i)
		{
			int value;
			while (!queue.Pop(value))
				Thread::Yield();
			assert(value == i);
		}
	});

	producer.Join();
	consumer.Join();

	std::cout << "  Lock-free queue test passed" << std::endl;
}

void TestBarrier()
{
	std::cout << "Testing barrier...\n";

	const int threadCount = 4;
	Barrier barrier(threadCount);
	std::atomic<int> counter{ 0 };

	std::vector<Thread> threads;
	for (int i=0; i<threadCount;++i)
	{
		threads.emplace_back([&barrier, &counter]() {
			counter.fetch_add(1, std::memory_order_relaxed);
			barrier.Wait();
			assert(counter.load() == threadCount);
		});
	}

	for (auto& thread : threads)
		thread.Join();

	std::cout << "  Barrier test passed" << std::endl;
}

void TestThreadLocal()
{
	std::cout << "Testing thread-local storage...\n";

	ThreadLocal<int> tlsValue;
	std::atomic<int> sum{ 0 };

	std::vector<Thread> threads;
	for (int i=0; i<4; ++i)
	{
		threads.emplace_back([&tlsValue, &sum, i]() {
			tlsValue.Get() = i;
			Thread::Sleep(10);
			sum.fetch_add(tlsValue.Get(), std::memory_order_relaxed);
		});
	}

	for (auto& thread : threads)
		thread.Join();

	assert(sum.load() == 6); // 0+1+2+3

	std::cout << "  Thread-local storage test passed" << std::endl;
}

void TestPerformanceCounter()
{
	std::cout << "Testing performance counter...\n";

	PerformanceCounter counter;

	counter.Start();
	Thread::Sleep(100);
	counter.Stop();

	double elapsedMs = counter.GetElapsedMilliseconds();
	assert(elapsedMs >= 90.0 && elapsedMs <= 200.);

	std::cout << "  Performance counter test passed (elapsed: " << elapsedMs << " ms)" << std::endl;
}

///

void TestThreadDetach()
{
	std::cout<<"Testing thread deatach...\n";

	std::atomic<bool> threadFinished{ false };

	{
		Thread thread([&threadFinished]() {
			Thread::Sleep(50);
			threadFinished.store(true, std::memory_order_relaxed);
		});

		thread.Detach();
		assert(!thread.Joinable());
	}

	while (!threadFinished.load(std::memory_order_relaxed))
		Thread::Sleep(1);

	std::cout << "  Thread deatach test passed" << std::endl;
}

void TestThreadTryJoin()
{
	std::cout << "Testing thread try join...\n";

	Thread thread([]() {
		Thread::Sleep(1000);
	});

	std::cout << "  Thread created, joinable: " << thread.Joinable() << std::endl;

	bool result = thread.TryJoin(10);
	std::cout << "  TryJoin(10) returned: " << result << std::endl;
	std::cout << "  Still joinable: " << thread.Joinable() << std::endl;

	assert(!result);
	assert(thread.Joinable());

	assert(thread.TryJoin(2000));
	assert(!thread.Joinable());

	std::cout<<"  Thread try join test passed"<<std::endl;
}

void TestThreadAffinity()
{
	std::cout << "Testing thread affinity...\n";

	uint32_t numProcessors = Thread::GetHardwareConcurrency();
	if (numProcessors < 2)
	{
		std::cout<<"Skipping affinity test (need at least 2 processors)"<<std::endl;
		return;
	}

	std::atomic<bool> threadStarted{ false };
	std::atomic<bool> threadFinished{ false };

	Thread thread([&]() {
		threadStarted.store(true, std::memory_order_relaxed);
		Thread::Sleep(50);
		threadFinished.store(true, std::memory_order_relaxed);
	});

	while (!threadStarted.load(std::memory_order_relaxed))
		Thread::Sleep(1);

	thread.SetAffinity(1ULL << 1);

	thread.Join();
	assert(threadFinished.load(std::memory_order_relaxed));

	std::cout << "  Thread affinity test passed" << std::endl;
}

void TestThreadNaming()
{
	// cant check for thread name, just check whether ts crashes or not
	std::cout << "Testing thread naming...\n";

	Thread thread([]() {
		Thread::Sleep(10);
	});

	thread.SetName("TestThread");

	thread.Join();

	std::cout << "  Thread naming test passed" << std::endl;
}

void TestAtomicOperations()
{
	std::cout<<"Testing atomic operations...\n";

	Atomic<int> atomicInt(0);

	assert(atomicInt.load() == 0);
	assert(atomicInt.FetchAdd(5) == 0);
	assert(atomicInt.load() == 5);
	assert(atomicInt.FetchSub(2) == 5);
	assert(atomicInt.load() == 3);

	atomicInt.store(10);
	assert(atomicInt.load() == 10);

	int expected = 10;
	assert(atomicInt.compare_exchange_strong(expected, 20));
	assert(atomicInt.load() == 20);

	expected = 15;
	assert(!atomicInt.compare_exchange_strong(expected,25));
	assert(atomicInt.load() == 20);
	assert(expected == 20);

	int v1=1,v2=2;
	Atomic<int*> atomicPtr(&v1);

	int* expectedPtr = &v1;
	assert(atomicPtr.CompareExchange(expectedPtr, &v1));
	assert(atomicPtr.load() == &v1);

	std::cout << "  Atomic operations test passed" << std::endl;
}

void TestSpinLock()
{
	std::cout << "Testing spin lock...\n";

	SpinLock spinLock;
	std::atomic<int> counter{ 0 };

	std::vector<Thread> threads;
	for (int i=0; i<4; ++i)
	{
		threads.emplace_back([&spinLock, &counter]() {
			for (int j=0; j<1000; ++j)
			{
				spinLock.Lock();
				counter.fetch_add(1, std::memory_order_relaxed);
				spinLock.Unlock();
			}
		});
	}

	for (auto& thread : threads)
		thread.Join();

	assert(counter.load() == 4000);

	std::cout << "  Spin lock test passed" << std::endl;
}

void TestMemoryBarrier()
{
	std::cout << "Testing memory barriers...\n";

	std::atomic<int> data{ 0 };
	std::atomic<bool> ready{ false };

	Thread writer([&]() {
		data.store(123,std::memory_order_relaxed);
		MemoryBarrier::Release();
		ready.store(true, std::memory_order_relaxed);
	});

	Thread reader([&]() {
		while (!ready.load(std::memory_order_relaxed))
			Thread::Yield();
		MemoryBarrier::Acquire();
		assert(data.load(std::memory_order_relaxed) == 123);
	});

	writer.Join();
	reader.Join();

	MemoryBarrier::Full();
	MemoryBarrier::Compiler();

	std::cout << "  Memory barrier test passed" << std::endl;
}

void TestLockFreeStack()
{
	std::cout << "Testing lock-free stack...\n";

	LockFreeStack<int> stack;

	for(int i=0; i<100; ++i)
		stack.Push(i);

	for (int i=99; i>=0; --i)
	{
		std::cout << i << " ";
		int v=0;
		assert(stack.Pop(v));
		std::cout << v << "\n";
		assert(v == i);
	}

	assert(stack.Empty());

	std::atomic<int> sum{ 0 };
	std::vector<Thread> producers;
	std::vector<Thread> consumers;

	for (int i=0; i<2; ++i)
	{
		producers.emplace_back([&stack, i]() {
			for (int j=0; j<100; ++j)
				stack.Push(i*100+j);
		});
	}

	for (int i=0; i<2; ++i)
	{
		consumers.emplace_back([&stack, &sum]() {
			int v;
			int count = 0;
			while (count < 100)
			{
				if (stack.Pop(v))
				{
					sum.fetch_add(v, std::memory_order_relaxed);
					count++;
				}
				else
					Thread::Yield();
			}
		});
	}

	for (auto& producer : producers)
		producer.Join();
	for (auto& consumer : consumers)
		consumer.Join();

	assert(sum.load() == 19900);

	std::cout << "  Lock-free stack test passed" << std::endl;
}

void TestMPMCQueue()
{
	std::cout << "Testing MPMC queue...\n";

	MPMCQueue<int, 1024> queue;

	for (int i=0;i<100;++i)
		assert(queue.Push(i));

	assert(queue.Size() == 100);

	for( int i=0;i<100;++i)
	{
		int v;
		assert(queue.Pop(v));
		assert(v == i);
	}

	assert(queue.Empty());

	std::atomic<int> sum{ 0 };
	std::atomic<int> totalPushed{ 0 };
	std::atomic<int> totalPopped{ 0 };
	std::vector<Thread> producers;
	std::vector<Thread> consumers;

	for (int i = 0; i < 2; ++i)
	{
		producers.emplace_back([&queue, &totalPushed, i]() {
			for (int j = 0; j < 500; ++j)
			{
				while (!queue.Push(i*500+j))
					Thread::Yield();
				totalPushed.fetch_add(1,std::memory_order_relaxed);
			}
		});
	}

	for (int i = 0; i < 2; ++i)
	{
		consumers.emplace_back([&queue, &sum, &totalPopped]() {
			int v;
			while (totalPopped.load(std::memory_order_relaxed)<1000)
			{
				if (queue.Pop(v))
				{
					sum.fetch_add(v, std::memory_order_relaxed);
					totalPopped.fetch_add(1,std::memory_order_relaxed);
				}
				else
					Thread::Yield();
			}
		});
	}

	for (auto& producer : producers)
		producer.Join();
	for (auto& consumer : consumers)
		consumer.Join();

	assert(totalPushed.load()==1000);
	assert(totalPopped.load()==1000);

	assert(sum.load() == 499500);

	std::cout << "  MPMC queue test passed" << std::endl;
}

void TestThreadLocalPointer()
{
	std::cout << "Testing thread-local pointer...\n";

	ThreadLocal<int*> tlsPtr;

	Thread thread([&tlsPtr]() {
		int v = 123;
		*tlsPtr = &v;
		Thread::Sleep(10);
		assert(**tlsPtr==123);
	});

	thread.Join();

	assert(!tlsPtr.HasValue());

	std::cout << "  Thread-local pointer test passed" << std::endl;
}

void TestLatch()
{
	std::cout << "Testing latch...\n";

	Latch latch(3);
	std::atomic<int> counter{ 0 };

	std::vector<Thread> threads;
	for (int i=0; i<3; ++i)
	{
		threads.emplace_back([&latch, &counter]() {
			Thread::Sleep(10);
			counter.fetch_add(1, std::memory_order_relaxed);
			latch.CountDown();
		});
	}

	latch.Wait();
	assert(counter.load() == 3);
	assert(latch.IsReady());

	for (auto& thread : threads)
		thread.Join();

	Latch latch2(1);
	assert(!latch2.WaitFor(10));
	latch2.CountDown();
	assert(latch2.WaitFor(1000));

	std::cout << "  Latch test passed" << std::endl;
}

void TestOnceFlag()
{
	std::cout << "Testing once flag...\n";

	OnceFlag flag;
	std::atomic<int> counter{ 0 };

	auto callOnceFunc = [&flag, &counter]() {
		flag.CallOnce([&counter]() {
			counter.fetch_add(1,std::memory_order_relaxed);
		});
	};

	std::vector<Thread> threads;
	for (int i=0; i<10; ++i)
		threads.emplace_back(callOnceFunc);

	for (auto& thread : threads)
		thread.Join();

	assert(counter.load()==1);

	OnceFlag flag2;
	std::atomic<int> counter2{ 0 };
	Thread thread([&flag2, &counter2]() {
		CallOnce(flag2, [&counter2]() {
			counter2.fetch_add(1,std::memory_order_relaxed);
		});
	});

	thread.Join();

	CallOnce(flag2, [&counter2]() {
		counter2.fetch_add(1,std::memory_order_relaxed);
	});

	assert(counter2.load() == 1);

	std::cout << "  Once flag test passed" << std::endl;
}

void TestParallelAlgorithms()
{
	std::cout << "Testing parallel algorithms...\n";

	ThreadPool::Config config;
	config.minThreads = 2;
	config.maxThreads = 4;
	ThreadPool pool(config);

	std::vector<int> data(10000);
	parallel::ForEach(data.begin(), data.end(), [](int& v) {
		v = 123;
	}, &pool);

	for (int v:data)
		assert(v==123);

	std::vector<int> nums(10000);
	std::iota(nums.begin(), nums.end(), 0);

	int sum = parallel::Reduce(nums.begin(), nums.end(), 0, [](int a, int b) {
		return a+b;
	}, &pool);

	assert(sum == (9999*10000)/2);

	std::vector<int> unsorted(10000);
	std::generate(unsorted.begin(),unsorted.end(), []() {
		static int counter = 0;
		return (counter++*7) % 10000;
	});

	parallel::Sort(unsorted, std::less<int>(), &pool);

	assert(std::is_sorted(unsorted.begin(), unsorted.end()));

	std::cout << "  Parallel algorithms test passed" << std::endl;
}

void TestDeadlockDetector()
{
	std::cout << "Testing deadlock detector...\n";

	DeadlockDetector::Enable();

	bool cbCalled = false;
	DeadlockDetector::SetCallback([&cbCalled](const DeadlockInfo& info) {
		cbCalled = true;
	});

	void* fake1 = reinterpret_cast<void*>(0x1233454);
	void* fake2 = reinterpret_cast<void*>(0x1451354);

	DeadlockDetector::OnLockAcquired(fake1,"Lock1");
	DeadlockDetector::OnLockReleased(fake1);

	DeadlockDetector::OnLockAcquired(fake2,"Lock2");
	DeadlockDetector::OnLockReleased(fake2);

	assert(!cbCalled);

	DeadlockDetector::Enable(false);

	std::cout << "  Deadlock detector test passed" << std::endl;
}

void TestScopedProfiler()
{
	std::cout << "Testing scoped profiler...\n";

	{
		ScopedProfiler profiler("TestOp");
		Thread::Sleep(10);
	}

	PerformanceCounter pc;
	pc.Start();
	Thread::Sleep(10);
	pc.Stop();

	assert(pc.GetElapsedMilliseconds() >= 5.);
	assert(pc.GetElapsedSeconds() < 1.);
	assert(pc.GetElapsedMicroseconds() > 0.);
	assert(PerformanceCounter::GetFrequency() > 0.);

	std::cout << "  Scoped profiler test passed" << std::endl;
}

void TestMutexOpenExisting()
{
	std::cout << "Testing named mutex...\n";

	const char* name = "EZTHREADTESTMTX";

	Mutex mutex1(false, name);
	Mutex mutex2 = Mutex::OpenExisting(name);

	mutex1.Lock();
	mutex2.Unlock();

	mutex2.Lock();
	mutex2.Unlock();

	std::cout << "  Named mutex test passed" << std::endl;
}

void TestSemaphoreOpenExisting()
{
	std::cout << "Testing named semaphore...\n";

	const char* name = "EZTHREADTESTSEMAPHORE";

	Semaphore semaphore1(0, 10, name);
	Semaphore semaphore2 = Semaphore::OpenExisting(name);

	semaphore2.Release(5);
	semaphore1.Acquire();
	semaphore1.Acquire();
	semaphore1.Acquire();
	semaphore1.Acquire();
	semaphore1.Acquire();

	std::cout << "  Named semaphore test passed" << std::endl;
}

void TestEventOpenExisting()
{
	std::cout << "Testing named event...\n";

	const char* name = "EZTHREADTESTEVENT";

	Event event1(Event::Type::ManualReset, false, name);
	Event event2 = Event::OpenExisting(name);

	event2.Set();
	assert(event1.Wait(0));
	assert(event2.IsSignaled());

	std::cout << "  Named event test passed" << std::endl;
}

void TestThreadPoolStress()
{
	std::cout << "Testing thread pool stress...\n";

	ThreadPool::Config config;
	config.minThreads = 2;
	config.maxThreads = 8;
	ThreadPool pool(config);

	const int numTasks = 1000;
	std::atomic<int> completedTasks{ 0 };
	std::vector<Future<int>> futures;
	futures.reserve(numTasks);

	for (int i=0; i<numTasks; ++i)
	{
		futures.push_back(pool.Submit([i, &completedTasks]() {
			Thread::Sleep(1);
			completedTasks.fetch_add(1,std::memory_order_relaxed);
			return i*2;
		}));
	}

	for (int i=0; i<numTasks; ++i)
		assert(futures[i].Get() == i*2);

	assert(completedTasks.load() == numTasks);

	pool.Stop(false);

	std::cout << "  Thread pool stress test passed" << std::endl;
}

void TestAsyncRun()
{
	std::cout << "Testing async run...\n";

	auto f1 = async::Run([]() {
		return 100;
	});

	assert(f1.Get()==100);

	auto f2 = async::Run([](int x) {
		return x * 2;
	}, 50);

	assert(f2.Get()==100);

	std::atomic<bool> executed{ false };
	auto f3 = async::Run([&executed]() {
		executed.store(true, std::memory_order_relaxed);
	});

	f3.Wait();
	assert(executed.load(std::memory_order_relaxed));

	std::cout << "  Async run test passed" << std::endl;
}

void TestAsyncRunDetached()
{
	std::cout << "Testing async run detached...\n";

	std::atomic<bool> executed{ false };

	async::RunDetached([&executed]() {
		Thread::Sleep(10);
		executed.store(true, std::memory_order_relaxed);
	});

	while (!executed.load(std::memory_order_relaxed))
		Thread::Sleep(1);

	assert(executed.load(std::memory_order_relaxed));

	std::cout << "  Async run detached test passed" << std::endl;
}

void TestWaitAll()
{
	std::cout << "Testing wait all...\n";

	std::vector<Future<void>> futures;
	futures.reserve(5);
	for (int i=0; i<5; ++i)
	{
		futures.push_back(async::Run([i]() {
			Thread::Sleep(10+i*5);
		}));
	}

	async::WaitAll(futures,1000);

	for (const auto& future : futures)
		assert(future.IsReady());

	std::vector<Future<void>> futures2;
	futures2.reserve(3);
	for (int i = 0; i < 3; ++i)
	{
		futures2.push_back(async::Run([i]() {
			Thread::Sleep(20);
		}));
	}

	std::vector<bool> completed;
	bool allCompleted = async::WaitAll(futures2, 1000, completed);

	assert(allCompleted);
	assert(completed.size() == 3);
	for (bool status : completed)
		assert(status);

	std::cout << "  Wait all test passed" << std::endl;
}

void TestWaitAny()
{
	std::cout << "Testing wait any...\n";

	std::vector<Future<void>> futures;
	futures.reserve(5);
	for (int i = 0; i < 5; ++i)
	{
		futures.push_back(async::Run([i]() {
			Thread::Sleep(10 + i * 10);
		}));
	}

	async::WaitAny(futures, 1000);

	bool anyReady = false;
	for (const auto& future : futures)
	{
		if (future.IsReady())
		{
			anyReady = true;
			break;
		}
	}
	assert(anyReady);

	std::vector<Future<void>> futures2;
	futures2.reserve(3);
	for (int i = 0; i < 3; ++i)
	{
		futures2.push_back(async::Run([i]() {
			Thread::Sleep(20+i*20);
		}));
	}

	std::vector<bool> completed;
	int completedIndex = async::WaitAny(futures2, 1000, completed);

	assert(completedIndex >= 0 && completedIndex < 3);
	assert(completed[completedIndex]);

	std::cout << "  Wait any test passed" << std::endl;
}

void TestTimer()
{
	std::cout << "Testing timer...\n";

	std::atomic<int> counter{ 0 };

	async::Timer timer;

	timer.SetOneShot(50, [&counter]() {
		counter.fetch_add(1, std::memory_order_relaxed);
	});

	Thread::Sleep(100);

	assert(counter.load(std::memory_order_relaxed) == 1);
	assert(!timer.IsRunning());

	counter.store(0, std::memory_order_relaxed);
	timer.Start(10, 10, [&counter]() {
		counter.fetch_add(1, std::memory_order_relaxed);
	});

	Thread::Sleep(100);
	timer.Stop();

	assert(counter.load(std::memory_order_relaxed) >= 5);
	assert(!timer.IsRunning());

	std::cout << "  Timer test passed" << std::endl;
}

void TestWork()
{
	std::cout << "Testing work...\n";

	async::Work work;
	std::atomic<bool> executed{ false };

	work.Submit([&executed]() {
		Thread::Sleep(10);
		executed.store(true,std::memory_order_relaxed);
	});

	work.Wait(1000);

	assert(work.IsComplete());
	assert(executed.load(std::memory_order_relaxed));

	async::Work work2;
	work2.Submit([]() {
		Thread::Sleep(100);
	});

	work2.Cancel();
	assert(work2.IsComplete());

	std::cout << "  Work test passed" << std::endl;
}

void TestAsyncErrors()
{
	std::cout << "Testing async error handling...\n";

	auto future = async::Run([]() -> int {
		throw std::runtime_error("Async error");
	});

	bool caught = false;
	try
	{
		future.Get();
	}
	catch (const std::runtime_error& e)
	{
		caught = true;
		assert(std::string(e.what())=="Async error");
	}

	assert(caught);

	auto slowFuture = async::Run([]() {
		Thread::Sleep(500);
	});

	std::vector<Future<void>> futures;
	futures.push_back(std::move(slowFuture));

	bool timeoutCaught = false;
	try
	{
		async::WaitAll(futures, 10);
	}
	catch (const ThreadException& e)
	{
		timeoutCaught = true;
		assert(e.ErrorCode() == WAIT_TIMEOUT);
	}

	assert(timeoutCaught);

	std::cout << "  Async error handling test passed"<<std::endl;
}

void TestAsyncPerformance()
{
	std::cout << "Testing async performance...\n";

	const int numTasks = 1000;
	std::vector<Future<int>> futures;
	futures.reserve(numTasks);

	PerformanceCounter counter;
	counter.Start();

	for (int i=0;i<numTasks;++i)
	{
		futures.push_back(async::Run([i]() {
			return i*2;
		}));
	}

	int sum = 0;
	for (auto& future:futures)
		sum+=future.Get();

	counter.Stop();

	assert(sum==(numTasks-1)*numTasks);

	std::cout << "  Async performance test passed (elapsed: "
			  << counter.GetElapsedMilliseconds() << " ms)" << std::endl;
}

int main()
{
	try
	{
		std::cout<<"ezthread test suite\n";
		std::cout<<"Hardware concurrency: " << Thread::GetHardwareConcurrency()<<"\n\n";

		// basic suite

		TestBasicThread();
		TestThreadPriority();
		TestCriticalSection();
		TestMutex();
		TestConditionVariable();
		TestSemaphore();
		TestEvent();
		TestRWLock();
		TestThreadPool();
		TestFuture();
		TestLockFreeQueue();
		TestBarrier();
		TestThreadLocal();
		TestPerformanceCounter();

		// extended suite

		TestThreadDetach();
		TestThreadTryJoin();
		TestThreadAffinity();
		TestThreadNaming();
		TestAtomicOperations();
		TestSpinLock();
		TestMemoryBarrier();
		TestLockFreeStack();
		TestMPMCQueue();
		TestThreadLocalPointer();
		TestLatch();
		TestOnceFlag();
		TestParallelAlgorithms();
		TestDeadlockDetector();
		TestScopedProfiler();
		TestMutexOpenExisting();
		TestSemaphoreOpenExisting();
		TestEventOpenExisting();
		TestThreadPoolStress();
		std::cout<<"I will fucking rape you\n";
		TestAsyncRun();
		TestAsyncRunDetached();
		TestWaitAll();
		TestWaitAny();
		TestTimer();
		TestWork();
		TestAsyncErrors();
		TestAsyncPerformance();

		std::cout << "\nAll tests passed\n";
		return 0;
	}
	catch (const ThreadException& e)
	{
		std::cerr << "ThreadException: " << e.what() << " (Error code: " << e.ErrorCode() << ")\n";
		return 1;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << std::endl;
		return 1;
	}
}

