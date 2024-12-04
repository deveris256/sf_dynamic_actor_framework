#pragma once
#include "SFEventHandler.h"
#include "DynamicMorphSession.h"
#include "MorphEvaluationRuleSet.h"
#include "SingletonBase.h"

#include "MutexUtils.h"

namespace daf
{
	extern constexpr time_t MenuActorUpdateInterval_ms = 200;
	extern constexpr time_t ActorUpdateInterval_ms = 300;
	extern constexpr time_t ActorPendingUpdateDelay_ms = 0;
	extern constexpr float  DiffThreshold = 0.05f;

	namespace tokens
	{
		inline constexpr std::string conditional_chargen_morph_manager{ "ECOffset_" };
	}


	class ConditionalChargenMorphManager :
		public utils::SingletonBase<ConditionalChargenMorphManager>,
		public events::GameDataLoadedEventDispatcher::Listener,
		public events::EventDispatcher<events::ArmorOrApparelEquippedEvent>::Listener,
		public events::EventDispatcher<events::ActorEquipManagerEquipEvent>::Listener,
		public events::EventDispatcher<events::ActorUpdateEvent>::Listener,
		public events::EventDispatcher<events::ActorFirstUpdateEvent>::Listener,
		public RE::BSTEventSink<RE::SaveLoadEvent>
	{
		friend class utils::SingletonBase<ConditionalChargenMorphManager>;

	public:
		virtual ~ConditionalChargenMorphManager() = default;

		void OnEvent(const events::ArmorOrApparelEquippedEvent& a_event, events::EventDispatcher<events::ArmorOrApparelEquippedEvent>* a_dispatcher) override
		{
			auto equip_type = a_event.equipType;
			auto actor = a_event.actor;
			auto armo = a_event.armorOrApparel;

			{
				tbb::concurrent_hash_map<RE::TESFormID, time_t>::accessor acc;
				if (!m_actor_watchlist.find(acc, actor->formID)) {
					return;
				}
				acc->second = a_event.when();
			}

			switch (equip_type) {
			case events::ArmorOrApparelEquippedEvent::EquipType::kEquip:
				logger::info("Armor Keyword Morph: {} equipped", utils::make_str(a_event.actor));
				m_actors_pending_reevaluation.insert(actor);
				break;
			case events::ArmorOrApparelEquippedEvent::EquipType::kUnequip:
				logger::info("Armor Keyword Morph: {} unequipped", utils::make_str(a_event.actor));
				m_actors_pending_reevaluation.insert(actor);
				break;
			}
		}

		void OnEvent(const events::ActorEquipManagerEquipEvent& a_event, events::EventDispatcher<events::ActorEquipManagerEquipEvent>* a_dispatcher) override
		{ // Handle Menu Actors since they don't send regular equip/unequip events
			auto equip_type = a_event.equipType;
			auto actor = a_event.actor;
			auto armo = a_event.armorOrApparel;

			if (!utils::IsActorMenuActor(actor)) {
				return;
			}

			{
				std::lock_guard lock(m_menu_actor_last_update_time_lock);
				this->m_menu_actor_last_update_time = a_event.when();
			}
		}

		void OnEvent(const events::GameLoadedEvent& a_event, events::EventDispatcher<events::GameLoadedEvent>* a_dispatcher) override
		{
			if (a_event.messageType != SFSE::MessagingInterface::MessageType::kPostPostDataLoad) {
				return;
			}
			// Load settings & Scan for morph keywords for caching (optional)
			return;
		}

		void OnEvent(const events::ActorUpdateEvent& a_event, events::EventDispatcher<events::ActorUpdateEvent>* a_dispatcher) override
		{
			//logger::c_info("Actor {} updated with deltaTime: {} ms, timeStamp {}", utils::make_str(a_event.actor), a_event.deltaTime * 1000, a_event.when());
			auto actor = a_event.actor;
			{
				std::lock_guard lock(m_menu_actor_last_update_time_lock);
				if (utils::IsActorMenuActor(a_event.actor) && a_event.when() - m_menu_actor_last_update_time > MenuActorUpdateInterval_ms) {
					m_menu_actor_last_update_time = a_event.when();
					if (this->ReevaluateActorMorph(actor)) {
						logger::info("MenuActor {} updating morphs", utils::make_str(actor));
						actor->UpdateChargenAppearance(); // Actors seems to partially copy morphs from MenuActors, this is bad
					}
					return;
				}
			}

			{
				tbb::concurrent_hash_map<RE::TESFormID, time_t>::accessor acc;
				if (!actor || !m_actor_watchlist.find(acc, actor->formID)) {
					return;
				}

				// Reevaluate immediately if the actor is pending reevaluation
				if (m_actors_pending_reevaluation.contains(actor)) {
					if (a_event.when() - acc->second < ActorPendingUpdateDelay_ms) {
						return;
					}
					std::lock_guard lock(m_actors_pending_reevaluation_erase_lock);
					acc->second = a_event.when();
					if (this->ReevaluateActorMorph(actor)) {
					}
					logger::info("Actor {} updating morphs", utils::make_str(actor));
					actor->UpdateChargenAppearance();
					m_actors_pending_reevaluation.unsafe_erase(actor);
					return;
				}

				// Update if the actor has not been updated for a certain interval
				if (a_event.when() - acc->second > ActorUpdateInterval_ms) {
					acc->second = a_event.when();
					if (this->ReevaluateActorMorph(actor)) {
						logger::info("Actor {} updating morphs regular", utils::make_str(actor));
						actor->UpdateChargenAppearance();
					}
					return;
				}
			}
		}

		void OnEvent(const events::ActorFirstUpdateEvent& a_event, events::EventDispatcher<events::ActorFirstUpdateEvent>* a_dispatcher) override
		{
			logger::c_info("Actor {} first updated with deltaTime: {} ms, timeStamp {}", utils::make_str(a_event.actor), a_event.deltaTime * 1000, a_event.when());

			auto actor = a_event.actor;
			{
				tbb::concurrent_hash_map<RE::TESFormID, time_t>::accessor acc;
				if (!actor || !m_actor_watchlist.find(acc, actor->formID)) {
					return;
				}

				acc->second = a_event.when();
				if (this->ReevaluateActorMorph(actor)) {
					logger::info("Actor {} updating morphs first", utils::make_str(actor));
					actor->UpdateChargenAppearance();
				}
			}
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::SaveLoadEvent& a_event, RE::BSTEventSource<RE::SaveLoadEvent>* a_storage) override
		{
			logger::info("Save loaded.");

			daf::MorphRuleSetManager::GetSingleton().LoadRulesets(utils::GetPluginFolder() + "\\Rulesets");

			return RE::BSEventNotifyControl::kContinue;
		}

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
			events::GameDataLoadedEventDispatcher::GetSingleton()->AddStaticListener(this);
			events::ActorUpdatedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorUpdateEvent>::AddStaticListener(this);
			events::ActorUpdatedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorFirstUpdateEvent>::AddStaticListener(this);

			RE::SaveLoadEvent::GetEventSource()->RegisterSink(this);
		}

		bool ReevaluateActorMorph(RE::Actor* a_actor)
		{
			auto& rs_manager = daf::MorphRuleSetManager::GetSingleton();

			auto ruleSet = rs_manager.GetForActor(a_actor);
			if (!ruleSet) {
				return false;
			}

			daf::MorphEvaluationRuleSet::ResultTable results;

			daf::DynamicMorphSession session(daf::tokens::conditional_chargen_morph_manager, a_actor);

			{
				std::lock_guard ruleset_lock(ruleSet->m_ruleset_spinlock);
				ruleSet->Snapshot(a_actor);
				ruleSet->Evaluate(results);
			}

			session.RestoreMorph();
			for (auto& [morph_name, result] : results) {
				if (result.is_setter) {
					session.MorphTargetCommit(std::string(morph_name), result.value);
				} else {
					session.MorphOffsetCommit(std::string(morph_name), result.value);
				}
			}

			return session.PushCommits() > DiffThreshold;
		}

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