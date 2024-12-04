#pragma once
#include "SFEventHandler.h"
#include "DynamicMorphSession.h"
#include "MorphEvaluationRuleSet.h"
#include "SingletonBase.h"

#include "MutexUtils.h"

namespace daf
{
	inline constexpr time_t MenuActorUpdateInterval_ms = 200;
	inline constexpr time_t ActorUpdateInterval_ms = 300;
	inline constexpr time_t ActorPendingUpdateDelay_ms = 0;
	inline constexpr float  DiffThreshold = 0.05f;

	namespace tokens
	{
		inline constexpr std::string conditional_chargen_morph_manager{ "ECOffset_" };
	}


	class ConditionalChargenMorphManager :
		public utils::SingletonBase<ConditionalChargenMorphManager>,
		public events::EventDispatcher<events::ArmorOrApparelEquippedEvent>::Listener,
		public events::EventDispatcher<events::ActorEquipManagerEquipEvent>::Listener,
		public events::EventDispatcher<events::ActorUpdateEvent>::Listener,
		public events::EventDispatcher<events::ActorFirstUpdateEvent>::Listener,
		public RE::BSTEventSink<RE::SaveLoadEvent>
	{
		friend class utils::SingletonBase<ConditionalChargenMorphManager>;

	public:
		virtual ~ConditionalChargenMorphManager() = default;

		void OnEvent(const events::ArmorOrApparelEquippedEvent& a_event, events::EventDispatcher<events::ArmorOrApparelEquippedEvent>* a_dispatcher) override;

		void OnEvent(const events::ActorEquipManagerEquipEvent& a_event, events::EventDispatcher<events::ActorEquipManagerEquipEvent>* a_dispatcher) override;

		void OnEvent(const events::ActorUpdateEvent& a_event, events::EventDispatcher<events::ActorUpdateEvent>* a_dispatcher) override;

		void OnEvent(const events::ActorFirstUpdateEvent& a_event, events::EventDispatcher<events::ActorFirstUpdateEvent>* a_dispatcher) override;

		RE::BSEventNotifyControl ProcessEvent(const RE::SaveLoadEvent& a_event, RE::BSTEventSource<RE::SaveLoadEvent>* a_storage) override;

		void Watch(RE::Actor* a_actor, bool a_pendingUpdate = true)
		{
			if (a_actor) {
				tbb::concurrent_hash_map<RE::TESFormID, time_t>::accessor acc;
				if (!m_actor_watchlist.find(acc, a_actor->formID)) {
					m_actor_watchlist.insert(acc, { a_actor->formID, 0 });
				}
			}
			if (a_pendingUpdate) {
				m_actors_pending_reevaluation.insert(a_actor);
			}
		}

		void Unwatch(RE::Actor* a_actor)
		{
			if (a_actor) {
				std::lock_guard lock(m_actor_watchlist_erase_lock);
				m_actor_watchlist.erase(a_actor->formID);
			}
		}

		void Register()
		{
			events::ArmorOrApparelEquippedEventDispatcher::GetSingleton()->EventDispatcher<events::ArmorOrApparelEquippedEvent>::AddStaticListener(this);
			events::ArmorOrApparelEquippedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorEquipManagerEquipEvent>::AddStaticListener(this);
			events::ActorUpdatedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorUpdateEvent>::AddStaticListener(this);
			events::ActorUpdatedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorFirstUpdateEvent>::AddStaticListener(this);

			RE::SaveLoadEvent::GetEventSource()->RegisterSink(this);
		}

		bool ReevaluateActorMorph(RE::Actor* a_actor);

	private:
		ConditionalChargenMorphManager(){};

		mutex::NonReentrantSpinLock               m_actors_pending_reevaluation_erase_lock;
		tbb::concurrent_unordered_set<RE::Actor*> m_actors_pending_reevaluation;

		std::mutex                                      m_actor_watchlist_erase_lock;
		tbb::concurrent_hash_map<RE::TESFormID, time_t> m_actor_watchlist{ { 0x14, 0 } };  // Player_ref

		mutex::NonReentrantSpinLock m_menu_actor_last_update_time_lock;
		time_t                      m_menu_actor_last_update_time{ 0 };
	};
}