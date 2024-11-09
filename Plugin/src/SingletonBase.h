#pragma once
#include <mutex>
#include <memory>

namespace utils
{
	template <typename T>
	class SingletonBase
	{
	public:
		static T& GetSingleton()
		{
			std::call_once(initInstanceFlag, &SingletonBase::initSingleton);
			return *instance;
		}

		// Delete copy constructor and assignment operator
		SingletonBase(const SingletonBase&) = delete;
		SingletonBase& operator=(const SingletonBase&) = delete;

	protected:
		SingletonBase() = default;
		virtual ~SingletonBase() = default;

		static void initSingleton()
		{
			instance.reset(new T());
		}

		static std::unique_ptr<T> instance;
		static std::once_flag     initInstanceFlag;
	};

	template <typename T>
	std::unique_ptr<T> SingletonBase<T>::instance = nullptr;

	template <typename T>
	std::once_flag SingletonBase<T>::initInstanceFlag;
}