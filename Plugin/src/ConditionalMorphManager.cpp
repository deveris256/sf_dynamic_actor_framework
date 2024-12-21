#include "ConditionalMorphManager.h"

void daf::ConditionalChargenMorphManager::OnEvent(const events::ArmorOrApparelEquippedEvent& a_event, events::EventDispatcher<events::ArmorOrApparelEquippedEvent>* a_dispatcher)
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

void daf::ConditionalChargenMorphManager::OnEvent(const events::ActorEquipManagerEquipEvent& a_event, events::EventDispatcher<events::ActorEquipManagerEquipEvent>* a_dispatcher)
{  // Handle Menu Actors since they don't send regular equip/unequip events
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

void daf::ConditionalChargenMorphManager::OnEvent(const events::ActorUpdateEvent& a_event, events::EventDispatcher<events::ActorUpdateEvent>* a_dispatcher)
{
	//logger::c_info("Actor {} updated with deltaTime: {} ms, timeStamp {}", utils::make_str(a_event.actor), a_event.deltaTime * 1000, a_event.when());
	auto actor = a_event.actor;
	//{
	//	std::lock_guard lock(m_menu_actor_last_update_time_lock);
	//	if (utils::IsActorMenuActor(a_event.actor) && a_event.when() - m_menu_actor_last_update_time > MenuActorUpdateInterval_ms) {
	//		m_menu_actor_last_update_time = a_event.when();
	//		if (this->ReevaluateActorMorph(actor)) {
	//			logger::info("MenuActor {} updating morphs", utils::make_str(actor));
	//			UpdateActorAppearance(actor, ActorAppearanceUpdator::UpdateType::kBodyMorphOnly);  // Actors seems to partially copy morphs from MenuActors, this is bad
	//			m_actors_pending_reevaluation.insert(RE::PlayerCharacter::GetSingleton());
	//		}
	//		return;
	//	}
	//}

	tbb::concurrent_hash_map<RE::TESFormID, time_t>::accessor acc;
	if (!actor || !m_actor_watchlist.find(acc, actor->formID)) {
		return;
	}

	// Reevaluate immediately if the actor is pending reevaluation
	if (m_actors_pending_reevaluation.contains(actor)) {
		std::lock_guard lock(m_actors_pending_reevaluation_erase_lock);
		acc->second = a_event.when();
		if (this->ReevaluateActorMorph(actor)) {
		}
		logger::info("Actor {} updating morphs", utils::make_str(actor));
		UpdateActorAppearance(actor, ActorAppearanceUpdator::UpdateType::kBodyMorphOnly);
		m_actors_pending_reevaluation.unsafe_erase(actor);
		return;
	}

	// Update if the actor has not been updated for a certain interval
	if (a_event.when() - acc->second > ActorUpdateInterval_ms) {
		acc->second = a_event.when();
		if (this->ReevaluateActorMorph(actor)) {
			logger::info("Actor {} updating morphs regular", utils::make_str(actor));
			UpdateActorAppearance(actor, ActorAppearanceUpdator::UpdateType::kBodyMorphOnly);
		}
		return;
	}
}

void daf::ConditionalChargenMorphManager::OnEvent(const events::ActorFirstUpdateEvent& a_event, events::EventDispatcher<events::ActorFirstUpdateEvent>* a_dispatcher)
{
	//logger::c_info("Actor {} first updated with deltaTime: {} ms, timeStamp {}", utils::make_str(a_event.actor), a_event.deltaTime * 1000, a_event.when());

	auto actor = a_event.actor;
	{
		tbb::concurrent_hash_map<RE::TESFormID, time_t>::accessor acc;
		if (!actor || !m_actor_watchlist.find(acc, actor->formID)) {
			return;
		}

		acc->second = a_event.when();
		if (this->ReevaluateActorMorph(actor)) {
			logger::info("Actor {} updating morphs first", utils::make_str(actor));
			UpdateActorAppearanceImmediate(actor, ActorAppearanceUpdator::UpdateType::kBodyMorphOnly);
		}
	}
}

void daf::ConditionalChargenMorphManager::OnEvent(const events::SaveLoadEvent& a_event, events::EventDispatcher<events::SaveLoadEvent>* a_dispatcher)
{
	if (a_event.saveLoadType != events::SaveLoadEvent::SaveLoadType::kSaveLoad) {
		return;
	}

	logger::info("Save loaded.");

	daf::MorphRuleSetManager::GetSingleton().LoadRulesets(utils::GetPluginFolder() + "\\Rulesets");

	return;
}

bool daf::ConditionalChargenMorphManager::ReevaluateActorMorph(RE::Actor* a_actor)
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

	return session.PushCommits(DiffThreshold) > DiffThreshold;
}
