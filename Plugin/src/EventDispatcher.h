#pragma once

namespace events
{
	class EventBase
	{
	public:
		virtual ~EventBase() = default;
	};

	template <class _Event_T>
		requires std::derived_from<_Event_T, EventBase>
	class EventDispatcher
	{
	public:
		class Listener
		{
		public:
			using event_type = _Event_T;
			using dispatcher_type = EventDispatcher<_Event_T>;

			virtual ~Listener() = default;
			virtual void OnEvent(const _Event_T& a_event, EventDispatcher<_Event_T>* a_dispatcher) = 0;
		};

		virtual ~EventDispatcher() = default;

		std::vector<std::weak_ptr<Listener>> listeners;
		std::vector<Listener*>               singleton_listeners;
		std::mutex                           mtx;

		void AddListener(std::shared_ptr<Listener> a_listener)
		{
			std::lock_guard lock(mtx);
			listeners.emplace_back(a_listener);
		}

		void AddStaticListener(Listener* a_listener)
		{
			std::lock_guard lock(mtx);
			if (std::find(singleton_listeners.begin(), singleton_listeners.end(), a_listener) == singleton_listeners.end()) {
				singleton_listeners.emplace_back(a_listener);
			}
		}

		void RemoveListener(Listener* a_listener)
		{
			std::lock_guard lock(mtx);
			listeners.erase(
				std::remove_if(
					listeners.begin(),
					listeners.end(),
					[a_listener](const std::weak_ptr<Listener>& weak_listener) {
						if (auto listener = weak_listener.lock()) {
							return listener.get() == a_listener;
						}
						return false;
					}
				),
				listeners.end()
			);
			singleton_listeners.erase(
				std::remove(singleton_listeners.begin(), singleton_listeners.end(), a_listener),
				singleton_listeners.end()
			);
		}

		void Dispatch(_Event_T a_event)
		{
			std::vector<std::shared_ptr<Listener>> active_listeners;

			{
				std::lock_guard lock(mtx);
				for (auto& weak_listener : listeners) {
					if (auto listener = weak_listener.lock()) {
						active_listeners.push_back(listener);
					}
				}
			}

			for (auto& listener : active_listeners) {
				listener->OnEvent(a_event, this);
			}

			for (auto& singleton_listener : singleton_listeners) {
				singleton_listener->OnEvent(a_event, this);
			}
		}
	};
}