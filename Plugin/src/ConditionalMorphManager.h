#pragma once
#include "SFEventHandler.h"
#include "DynamicMorphSession.h"
#include "MorphEvaluationRuleSet.h"
#include "SingletonBase.h"

namespace daf
{
	extern constexpr time_t MenuActorUpdateInterval_ms = 300;
	extern constexpr time_t ActorUpdateInterval_ms = 500;
	extern constexpr float  DiffThreshold = 0.05f;

	namespace tokens
	{
		inline constexpr std::string conditional_morph_manager{ "ECOffset_" };
	}


	class ConditionalMorphManager :
		public utils::SingletonBase<ConditionalMorphManager>,
		public events::ArmorOrApparelEquippedEventDispatcher::Listener,
		public events::GameDataLoadedEventDispatcher::Listener,
		public events::EventDispatcher<events::ActorUpdateEvent>::Listener,
		public events::EventDispatcher<events::ActorFirstUpdateEvent>::Listener,
		public RE::BSTEventSink<RE::SaveLoadEvent>
	{
		friend class utils::SingletonBase<ConditionalMorphManager>;
	public:

		virtual ~ConditionalMorphManager() = default;

		void OnEvent(const events::ArmorOrApparelEquippedEvent& a_event, events::EventDispatcher<events::ArmorOrApparelEquippedEvent>* a_dispatcher) override
		{
			auto equip_type = a_event.equipType;
			auto actor = a_event.actor;
			auto armo = a_event.armorOrApparel;

			{
				tbb::concurrent_hash_map<RE::TESFormID, time_t>::accessor acc;
				if (!actor || !m_actor_watchlist.find(acc, actor->formID)) {
					return;
				}
			}

			switch (equip_type) {
			case events::ArmorOrApparelEquippedEvent::EquipType::kEquip:
				//logger::c_info("Armor Keyword Morph: {} equipped", utils::make_str(a_event.actor));
				m_actors_pending_reevaluation.insert(actor);
				break;
			case events::ArmorOrApparelEquippedEvent::EquipType::kUnequip:
				//logger::c_info("Armor Keyword Morph: {} unequipped", utils::make_str(a_event.actor));
				m_actors_pending_reevaluation.insert(actor);
				break;
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
			if (utils::IsActorMenuActor(a_event.actor) && a_event.when() - menu_actor_last_update_time > MenuActorUpdateInterval_ms) {
				menu_actor_last_update_time = a_event.when();
				if (this->ReevaluateActorMorph(actor)) {
					actor->UpdateChargenAppearance();
				}
				return;
			}

			{
				tbb::concurrent_hash_map<RE::TESFormID, time_t>::accessor acc;
				if (!actor || !m_actor_watchlist.find(acc, actor->formID)) {
					return;
				}

				// Reevaluate immediately if the actor is pending reevaluation
				if (m_actors_pending_reevaluation.contains(actor)) {
					std::lock_guard lock(m_actors_pending_reevaluation_erase_lock);
					acc->second = a_event.when();
					if (this->ReevaluateActorMorph(actor)) {
						actor->UpdateChargenAppearance();
					}
					m_actors_pending_reevaluation.unsafe_erase(actor);
					return;
				}

				// Update if the actor has not been updated for a certain interval
				if (a_event.when() - acc->second > ActorUpdateInterval_ms) {
					acc->second = a_event.when();
					if (this->ReevaluateActorMorph(actor)) {
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
					actor->UpdateChargenAppearance();
				}
			}
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::SaveLoadEvent& a_event, RE::BSTEventSource<RE::SaveLoadEvent>* a_storage) override
		{
			logger::info("Save loaded.");

			return RE::BSEventNotifyControl::kContinue;
		}

		void Watch(RE::Actor* a_actor)
		{
			if (a_actor) {
				tbb::concurrent_hash_map<RE::TESFormID, time_t>::accessor acc;
				m_actor_watchlist.insert(acc, { a_actor->formID, 0 });
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
			events::ArmorOrApparelEquippedEventDispatcher::GetSingleton()->AddStaticListener(this);
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

			daf::DynamicMorphSession session(daf::tokens::conditional_morph_manager, a_actor);

			{
				std::lock_guard<std::mutex> rule_set_lock(ruleSet->m_ruleset_mutex);
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
		ConditionalMorphManager(){};

		std::mutex                                m_actors_pending_reevaluation_erase_lock;
		tbb::concurrent_unordered_set<RE::Actor*> m_actors_pending_reevaluation;

		std::mutex                                      m_actor_watchlist_erase_lock;
		tbb::concurrent_hash_map<RE::TESFormID, time_t> m_actor_watchlist{ { 0x14, 0 } };  // Player_ref

		time_t menu_actor_last_update_time = 0;
	};
}