#pragma once
#include "SFEventHandler.h"
#include "SingletonBase.h"


namespace daf
{
	inline constexpr time_t ActorUpdateAppearanceDelay_ms = 300;

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
		};

		using _Pending_List_T = tbb::concurrent_hash_map<RE::Actor*, PendingUpdateInfo>;

		void OnEvent(const events::ActorUpdateEvent& a_event, events::EventDispatcher<events::ActorUpdateEvent>* a_dispatcher) override {
			auto actor = a_event.actor;

			_Pending_List_T::const_accessor acc;
			if (!actor || !m_actor_pending_update_appearance.find(acc, actor)) {
				return;
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

			m_actor_pending_update_appearance.erase(acc);
		}

		void OnEvent(const events::ActorEquipManagerEquipEvent& a_event, events::EventDispatcher<events::ActorEquipManagerEquipEvent>* a_dispatcher) override {
			auto actor = a_event.actor;
			if (!actor) {
				return;
			}

			_Pending_List_T::accessor acc;
			m_actor_pending_update_appearance.insert(acc, actor);

			// Invalidates update if the actor is equipping/unequipping items
			acc->second.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		void OnEvent(const events::SaveLoadEvent& a_event, events::EventDispatcher<events::SaveLoadEvent>* a_dispatcher) override
		{
			if (a_event.saveLoadType != events::SaveLoadEvent::SaveLoadType::kSaveLoad) {
				return;
			}
			m_actor_pending_update_appearance.clear();
		}

		bool UpdateActor(RE::Actor* a_actor, UpdateType a_type) {
			if (!a_actor) {
				return false;
			}

			_Pending_List_T::accessor acc;
			if (!m_actor_pending_update_appearance.insert(acc, a_actor)) {
				//return false;
			}

			acc->second.type = UpdateType(std::to_underlying(acc->second.type) | std::to_underlying(a_type));
			acc->second.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

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

	// Use this interface to ensure that the actor's appearance is updated at the right time
	inline bool UpdateActorAppearance(RE::Actor* a_actor, ActorAppearanceUpdator::UpdateType a_type) {
		return ActorAppearanceUpdator::GetSingleton().UpdateActor(a_actor, a_type);
	}
}