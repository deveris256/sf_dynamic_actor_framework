#pragma once
#include <memory>
#include <mutex>

namespace mutex
{
	// To be able to use std::lock_guard
	class MutexBase
	{
	public:
		virtual ~MutexBase() = default;
		virtual void lock() = 0;
		virtual void unlock() = 0;
	};

	class NonReentrantSpinLock : public MutexBase
	{
	public:
		NonReentrantSpinLock() = default;
		NonReentrantSpinLock(const NonReentrantSpinLock&) = delete;
		NonReentrantSpinLock& operator=(const NonReentrantSpinLock&) = delete;

		void lock()
		{
			while (flag.test_and_set(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}
		}

		void unlock()
		{
			flag.clear(std::memory_order_release);
		}

	private:
		std::atomic_flag flag = ATOMIC_FLAG_INIT;
	};
}