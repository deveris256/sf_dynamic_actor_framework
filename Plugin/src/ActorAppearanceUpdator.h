#pragma once
#include "SFEventHandler.h"
#include "SingletonBase.h"


namespace daf
{
	inline constexpr time_t ActorUpdateAppearanceDelay_ms = 200;
	inline constexpr bool DisableMenuActorMorphUpdate = true;

	class ActorAppearanceUpdator :
		public utils::SingletonBase<ActorAppearanceUpdator>,
		public events::EventDispatcher<events::ActorUpdateEvent>::Listener,
		public events::EventDispatcher<events::ActorEquipManagerEquipEvent>::Listener,
		public events::EventDispatcher<events::SaveLoadEvent>::Listener
	{
		friend class utils::SingletonBase<ActorAppearanceUpdator>;
	public:
		enum class UpdateType : std::uint8_t
		{
			kNone = 0,
			kBodyMorphOnly = 1 << 0,
			kHeadpartsOnly = 1 << 1,
			kBodyMorphAndHeadparts = kBodyMorphOnly | kHeadpartsOnly
		};

		struct PendingUpdateInfo
		{
			UpdateType type{ UpdateType::kNone };
			time_t     timestamp{ 0 };
			bool       refreshTimestampOnFetch{ false };
		};

		using _Pending_List_T = tbb::concurrent_hash_map<RE::Actor*, PendingUpdateInfo>;

		void OnEvent(const events::ActorUpdateEvent& a_event, events::EventDispatcher<events::ActorUpdateEvent>* a_dispatcher) override {
			auto actor = a_event.actor;

			_Pending_List_T::accessor acc;
			if (!actor || !m_actor_pending_update_appearance.find(acc, actor)) {
				return;
			}

			if (acc->second.refreshTimestampOnFetch) {
				acc->second.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
				//logger::info("ActorAppearanceUpdator::OnEvent: Refreshed Timestamp: Actor[{}], UpdateType[{}]", utils::make_str(actor), std::to_underlying(acc->second.type));
				acc->second.refreshTimestampOnFetch = false;
			}

			if (a_event.when() - acc->second.timestamp < ActorUpdateAppearanceDelay_ms) {
				return;
			}

			auto type = std::to_underlying(acc->second.type);

			if (type & std::to_underlying(UpdateType::kHeadpartsOnly)) {
				// Update headparts
				actor->UpdateAppearance(false, 0u, false);
			}
			if (type & std::to_underlying(UpdateType::kBodyMorphOnly)) {
				// Update body morph
				actor->UpdateChargenAppearance();
			}

			//logger::info("ActorAppearanceUpdator::OnEvent: Updated: Actor[{}], UpdateType[{}]", utils::make_str(actor), std::to_underlying(acc->second.type));

			m_actor_pending_update_appearance.erase(acc);
		}

		void OnEvent(const events::ActorEquipManagerEquipEvent& a_event, events::EventDispatcher<events::ActorEquipManagerEquipEvent>* a_dispatcher) override {
			auto actor = a_event.actor;
			if (!actor) {
				return;
			}

			
			if (DisableMenuActorMorphUpdate && utils::IsActorMenuActor(actor)) {
				return;
			}

			_Pending_List_T::accessor acc;
			m_actor_pending_update_appearance.insert(acc, actor);

			// Invalidates update if the actor is equipping/unequipping items
			//logger::info("ActorAppearanceUpdator::OnEvent: Actor[{}] set RefreshTimestamp: {}", utils::make_str(actor), std::to_underlying(acc->second.type), acc->second.refreshTimestampOnFetch);
			acc->second.refreshTimestampOnFetch = true;
		}

		void OnEvent(const events::SaveLoadEvent& a_event, events::EventDispatcher<events::SaveLoadEvent>* a_dispatcher) override
		{
			if (a_event.saveLoadType != events::SaveLoadEvent::SaveLoadType::kSaveLoad) {
				return;
			}
			m_actor_pending_update_appearance.clear();
		}

		bool UpdateActor(RE::Actor* a_actor, UpdateType a_type) {
			_Pending_List_T::accessor acc;
			if (!m_actor_pending_update_appearance.insert(acc, a_actor)) {
				//return false;
			}

			acc->second.type = UpdateType(std::to_underlying(acc->second.type) | std::to_underlying(a_type));
			acc->second.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

			//logger::info("ActorAppearanceUpdator::UpdateActor: Actor[{}], UpdateType[{}], RefreshTimestamp: {}", utils::make_str(a_actor), std::to_underlying(acc->second.type), acc->second.refreshTimestampOnFetch);

			return true;
		}

		bool UpdateActorImmediate(RE::Actor* a_actor, UpdateType a_type) {
			auto type = std::to_underlying(a_type);

			_Pending_List_T::accessor acc;
			if (m_actor_pending_update_appearance.find(acc, a_actor)) {
				type |= std::to_underlying(acc->second.type);
				m_actor_pending_update_appearance.erase(acc);
			}

			if (type & std::to_underlying(UpdateType::kHeadpartsOnly)) {
				// Update headparts
				a_actor->UpdateAppearance(false, 0u, false);
			}
			if (type & std::to_underlying(UpdateType::kBodyMorphOnly)) {
				// Update body morph
				a_actor->UpdateChargenAppearance();
			}

			return true;
		}

		void Register() {
			events::ActorUpdatedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorUpdateEvent>::AddStaticListener(this);
			events::ArmorOrApparelEquippedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorEquipManagerEquipEvent>::AddStaticListener(this);
			events::SaveLoadEventDispatcher::GetSingleton()->AddStaticListener(this);
		}

	private:
		_Pending_List_T m_actor_pending_update_appearance;

		ActorAppearanceUpdator() {};
	};

	// Use this interface to ensure that the actor's appearance is updated at the right time. Thread-safe.
	inline bool UpdateActorAppearance(RE::Actor* a_actor, ActorAppearanceUpdator::UpdateType a_type)
	{
		if (DisableMenuActorMorphUpdate && utils::IsActorMenuActor(a_actor)) {
			return false;
		}
		return ActorAppearanceUpdator::GetSingleton().UpdateActor(a_actor, a_type);
	}

	inline bool UpdateActorAppearanceImmediate(RE::Actor* a_actor, ActorAppearanceUpdator::UpdateType a_type)
	{
		if (DisableMenuActorMorphUpdate && utils::IsActorMenuActor(a_actor)) {
			return false;
		}
		return ActorAppearanceUpdator::GetSingleton().UpdateActorImmediate(a_actor, a_type);
	}
}