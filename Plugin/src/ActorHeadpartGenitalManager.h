#pragma once
#include "SFEventHandler.h"
#include "SingletonBase.h"

#include "MutexUtils.h"

namespace events
{
	class ActorGenitalChangedEvent : public TimedEventBase
	{
	public:
		ActorGenitalChangedEvent(RE::Actor* a_actor, RE::BGSHeadPart* a_prev_genital_headpart, RE::BGSHeadPart* a_new_genital_headpart) :
			actor(a_actor),
			prevGenitalHeadpart(a_prev_genital_headpart),
			newGenitalHeadpart(a_new_genital_headpart)
		{}

		RE::Actor*       actor;
		RE::BGSHeadPart* prevGenitalHeadpart;
		RE::BGSHeadPart* newGenitalHeadpart;
	};

	class ActorRevealingStateChangedEvent : public TimedEventBase
	{
	public:
		ActorRevealingStateChangedEvent(RE::Actor* a_actor, bool a_old_state, bool a_new_state) :
			actor(a_actor),
			oldState(a_old_state),
			newState(a_new_state)
		{}
		RE::Actor* actor;
		bool       oldState; // True if genital is hidden
		bool       newState;
	};

	class GenitalDataLoadingEvent : public EventBase
	{
	public:
		GenitalDataLoadingEvent(RE::BGSHeadPart* genitalHeadpart, const nlohmann::json& a_configData, const nlohmann::json& a_genitalData) :
			genitalHeadpart(genitalHeadpart),
			configData(a_configData),
			genitalData(a_genitalData)
		{}
		RE::BGSHeadPart* genitalHeadpart;
		const nlohmann::json& configData;
		const nlohmann::json& genitalData;
	};

	class GenitalDataReloadEvent : public EventBase
	{};
}

namespace daf
{
	extern constexpr uint64_t    consealingSlots = 1 << 13 | 1 << 3; // Slot 13 and 3 (body) are consealing slots
	extern constexpr const char* genitalConsealKeywordEditorID = "DAFGenitalConseal";
	extern constexpr const char* genitalRevealKeywordEditorID = "DAFGenitalReveal";
	extern constexpr const char* hideGenitalMorphKey = "HideGenital";
	extern constexpr const char* genitalOverrideMorphKey = "DAF_GenitalTypeOverride";
	extern constexpr time_t      ActorPendingUpdateRevealingDelay_ms = 200;
	extern constexpr time_t      ActorPendingUpdateRevealingCycle_ms = 1000;

	class ActorHeadpartGenitalManager :
		public utils::SingletonBase<ActorHeadpartGenitalManager>,
		public events::EventDispatcher<events::ActorEquipManagerEquipEvent>::Listener,
		public events::EventDispatcher<events::ActorUpdateEvent>::Listener,
		public events::EventDispatcher<events::ActorFirstUpdateEvent>::Listener,
		public RE::BSTEventSink<RE::SaveLoadEvent>,
		public events::EventDispatcher<events::ActorGenitalChangedEvent>,
		public events::EventDispatcher<events::ActorRevealingStateChangedEvent>,
		public events::EventDispatcher<events::GenitalDataLoadingEvent>,
		public events::EventDispatcher<events::GenitalDataReloadEvent>
	{
		friend class utils::SingletonBase<ActorHeadpartGenitalManager>;
	public:
		class SkintoneGenitalHeadparts
		{
		public:
			SkintoneGenitalHeadparts(){};

			RE::BGSHeadPart*              skintoneDefaultGenital{ nullptr };
			std::vector<RE::BGSHeadPart*> skintoneGenitals;

			RE::BGSHeadPart* GetGenitalHeadpart(size_t a_skintone_index) const
			{
				const std::vector<RE::BGSHeadPart*>& genitalList = skintoneGenitals;

				if (genitalList.size() > a_skintone_index) {
					if (auto gen = genitalList[a_skintone_index]; gen) {
						return gen;
					}
				}
				return skintoneDefaultGenital;
			}

			// Returns true if the headpart was set, false if it was not
			bool SetGenitalHeadpart(size_t a_skintone_index, RE::BGSHeadPart* a_headpart)
			{
				if (!a_headpart) {
					return false;
				}

				std::vector<RE::BGSHeadPart*>& genitalList = skintoneGenitals;

				if (genitalList.size() <= a_skintone_index) {
					genitalList.resize(a_skintone_index + 1);
				}

				genitalList[a_skintone_index] = a_headpart;
				return true;
			}

			inline void SetDefaultGenitalHeadpart(RE::BGSHeadPart* a_headpart)
			{
				skintoneDefaultGenital = a_headpart;
			}

			bool emtpy() const {
				return skintoneDefaultGenital == nullptr && skintoneGenitals.empty();
			}
				
		};

		class RaceGenitalListRegistry
		{
			using _List_Type = tbb::concurrent_hash_map<std::string, SkintoneGenitalHeadparts>;
		public:
			RaceGenitalListRegistry() = default;

			std::string defaultGenitalName;
			std::shared_mutex defaultGenitalName_ReadWriteLock;
			tbb::concurrent_hash_map<std::string, SkintoneGenitalHeadparts> genitalList;

			std::vector<std::string> QueryAvailibleGenitalTypesUnsafe() const {
				std::vector<std::string> genitalTypes;
				_List_Type::const_accessor accessor;
				for (const auto& [genitalName, genitalData] : genitalList) {
					if (!genitalData.emtpy()) {
						genitalTypes.push_back(genitalName);
					}
				}
				return genitalTypes;
			}

			RE::BGSHeadPart* GetDefaultGenitalHeadpart(size_t a_skintone_index) {
				_List_Type::const_accessor accessor;
				std::shared_lock read_lock(defaultGenitalName_ReadWriteLock);
				if (genitalList.find(accessor, defaultGenitalName)) {
					const SkintoneGenitalHeadparts& skintoneGenitals = accessor->second;
					return skintoneGenitals.GetGenitalHeadpart(a_skintone_index);
				}
				return nullptr;
			}

			RE::BGSHeadPart* GetGenitalHeadpart(const std::string& a_genital_name, size_t a_skintone_index) const
			{
				_List_Type::const_accessor accessor;
				if (genitalList.find(accessor, a_genital_name)) {
					const SkintoneGenitalHeadparts& skintoneGenitals = accessor->second;
					return skintoneGenitals.GetGenitalHeadpart(a_skintone_index);
				}
				return nullptr;
			}

			bool SetGenitalHeadpart(const std::string& a_genital_name, size_t a_skintone_index, RE::BGSHeadPart* a_headpart)
			{
				_List_Type::accessor accessor;
				genitalList.insert(accessor, a_genital_name);
				SkintoneGenitalHeadparts& skintoneGenitals = accessor->second;
				return skintoneGenitals.SetGenitalHeadpart(a_skintone_index, a_headpart);
			}

			inline void SetDefaultGenitalHeadpart(const std::string& a_genital_name, RE::BGSHeadPart* a_headpart) {
				_List_Type::accessor accessor;
				genitalList.insert(accessor, a_genital_name);
				SkintoneGenitalHeadparts& skintoneGenitals = accessor->second;
				skintoneGenitals.SetDefaultGenitalHeadpart(a_headpart);
			}

			inline bool SetDefaultRaceSexGenitalHeadpart(const std::string& a_genital_name) {
				std::unique_lock write_lock(defaultGenitalName_ReadWriteLock);
				defaultGenitalName = a_genital_name;
				return true;
			}

			inline bool IsValid() {
				_List_Type::const_accessor accessor;
				std::shared_lock           read_lock(defaultGenitalName_ReadWriteLock);
				return genitalList.find(accessor, defaultGenitalName);
			}
		};

		using _Storage_T = tbb::concurrent_hash_map<RE::TESRace*, RaceGenitalListRegistry>;

		class ActorGenitalUpdateInfo
		{
		public:
			std::string newGenitalName;
			bool        setOverride{ false };
		};

		virtual ~ActorHeadpartGenitalManager() = default;

		void OnEvent(const events::ActorEquipManagerEquipEvent& a_event, events::EventDispatcher<events::ActorEquipManagerEquipEvent>* a_dispatcher) override{
			auto actor = a_event.actor;

			if (!ActorHasGenital(actor)) {
				return;
			}

			{
				tbb::concurrent_hash_map<RE::TESFormID, time_t>::accessor accessor;
				m_actor_pending_update.insert(accessor, actor->formID);
				accessor->second = a_event.when();
			}
		};

		void OnEvent(const events::ActorUpdateEvent& a_event, events::EventDispatcher<events::ActorUpdateEvent>* a_dispatcher) override{
			// Process pending actor update
			auto actor = a_event.actor;
			auto npc = actor->GetNPC();
			auto event_when = a_event.when();

			if (tbb::concurrent_hash_map<RE::TESFormID, ActorGenitalUpdateInfo>::accessor accessor;
				m_actor_pending_change_genital.find(accessor, actor->formID)) {
				auto& newGenitalName = accessor->second.newGenitalName;
				auto setOverride = accessor->second.setOverride;

				auto genital_headpart = GetGenitalHeadpart(npc, newGenitalName);
				if (!genital_headpart) {
					logger::warn("Genital headpart type {} not found for NPC {}", newGenitalName, utils::make_str(npc));
					m_actor_pending_change_genital.erase(accessor);
					return;
				}

				bool should_update = false;
				
				should_update |= ChangeActorGenital(actor, genital_headpart);
				
				SetActorGenitalOverride(npc, setOverride);

				should_update |= EvaluateRevealingStateAndApply(actor);

				if (should_update) {
					actor->UpdateAppearance(false, 0u, false);
					actor->UpdateChargenAppearance();
				}

				m_actor_pending_change_genital.erase(accessor);
				m_actor_pending_update.erase(actor->formID);
				return;
			}

			if (tbb::concurrent_hash_map<RE::TESFormID, time_t>::accessor accessor;
				m_actor_pending_update.find(accessor, actor->formID)) {
				auto when = accessor->second;
				if (event_when - when < ActorPendingUpdateRevealingDelay_ms) {
					return;
				}

				bool should_update = EvaluateRevealingStateAndApply(actor);

				if (should_update) {
					actor->UpdateChargenAppearance();
				}

				m_actor_pending_update.erase(accessor);
			} else if (ActorHasGenital(actor)) {
				m_actor_pending_update.insert({ actor->formID, event_when - ActorPendingUpdateRevealingCycle_ms + ActorPendingUpdateRevealingDelay_ms });
			}
		};

		void OnEvent(const events::ActorFirstUpdateEvent& a_event, events::EventDispatcher<events::ActorFirstUpdateEvent>* a_dispatcher) override{
			auto actor = a_event.actor;

			auto actor_npc = actor->GetNPC();
			if (!actor_npc) {
				return;
			}

			if (ActorHasGenitalOverride(actor_npc)) {
				m_actor_genital_cache.insert({ actor->formID, GetActorGenital_Impl(actor_npc) });
				return;
			}

			// Attempt to set default genital headpart
			auto genital_headpart = GetDefaultGenitalHeadpart(actor_npc);
			if (!genital_headpart) {
				m_actor_genital_cache.insert({ actor->formID, nullptr });
				return;
			}

			bool should_update_headpart = ChangeActorGenital(actor, genital_headpart);

			bool should_update_morphs = EvaluateRevealingStateAndApply(actor);
			if (utils::IsActorMenuActor(actor)) {
				m_actor_pending_update.insert({ m_playerRef->formID, a_event.when() });
			}

			if (should_update_headpart) {
				actor->UpdateAppearance(false, 0u, false);
				actor->UpdateChargenAppearance();
			} else if (should_update_morphs) {
				actor->UpdateChargenAppearance();
			}
		};

		RE::BSEventNotifyControl ProcessEvent(const RE::SaveLoadEvent& a_event, RE::BSTEventSource<RE::SaveLoadEvent>* a_storage) override
		{
			logger::info("Loading genital headparts...");

			auto configFolder = utils::GetAddonsFolder() + "\\DAFGen";

			this->LoadGenitalHeadpartsFromJSON(configFolder);

			GetGenitalConsealKeyword(true);
			GetGenitalRevealKeyword(true);

			m_playerRef = RE::TESForm::LookupByID<RE::Actor>(0x14);
			if (!m_playerRef) {
				logger::error("Player reference not found, genital system will not work properly.");
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		void Register()
		{
			events::ArmorOrApparelEquippedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorEquipManagerEquipEvent>::AddStaticListener(this);
			//events::GameDataLoadedEventDispatcher::GetSingleton()->AddStaticListener(this);
			events::ActorUpdatedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorUpdateEvent>::AddStaticListener(this);
			events::ActorUpdatedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorFirstUpdateEvent>::AddStaticListener(this);

			RE::SaveLoadEvent::GetEventSource()->RegisterSink(this);
		}

		std::vector<std::string> QueryAvailibleGenitalTypesForActor(RE::Actor* a_actor) {
			auto npc = a_actor->GetNPC();
			auto race = npc->GetRace();
			auto sex = npc->GetSex();

			_Storage_T::accessor accessor;

			switch (sex) {
			case RE::SEX::kMale:
				if (!m_maleRaceGenitalHeadparts.find(accessor, race)) {
					return {};
				}
				break;
			case RE::SEX::kFemale:
				if (!m_femaleRaceGenitalHeadparts.find(accessor, race)) {
					return {};
				}
				break;
			}

			return accessor->second.QueryAvailibleGenitalTypesUnsafe();
		}

		void ChangeActorGenital(RE::Actor* a_actor, const std::string& a_genitalName, bool a_setOverride = true)
		{
			tbb::concurrent_hash_map<RE::TESFormID, ActorGenitalUpdateInfo>::accessor accessor;
			m_actor_pending_change_genital.insert(accessor, a_actor->formID);
			accessor->second.newGenitalName = a_genitalName;
			accessor->second.setOverride = a_setOverride;
		}

		bool EvaluateRevealingStateAndApply(RE::Actor* a_actor) {
			bool should_update_morphs = false;
			auto should_reveal = EvaluateActorGenitalRevealingState(a_actor);
			if (should_reveal) {
				should_update_morphs |= RevealActorGenitalUnsafe(a_actor);
			} else {
				should_update_morphs |= HideActorGenitalUnsafe(a_actor);
			}
			return should_update_morphs;
		}

		bool EvaluateActorGenitalRevealingState(RE::Actor* a_actor) {
			bool is_spacesuit_visible = utils::ShouldActorShowSpacesuit(a_actor);
			auto genitalRevealKeyword = GetGenitalRevealKeyword();
			auto genitalConsealKeyword = GetGenitalConsealKeyword();

			if (is_spacesuit_visible) { // Spacesuit is visible, there must be GenitalRevealKeyword in the spacesuit body ARMO for the result to be true
				if (!genitalRevealKeyword) {
					return false;
				}

				bool has_genital_reveal_keyword = false;

				a_actor->ForEachEquippedItem(
					[&has_genital_reveal_keyword, genitalRevealKeyword](const RE::BGSInventoryItem& item) -> RE::BSContainer::ForEachResult {
						auto armor = item.object->As<RE::TESObjectARMO>();
						auto instanceData = reinterpret_cast<RE::TESObjectARMOInstanceData*>(item.instanceData.get());

						if (!armor) {
							return RE::BSContainer::ForEachResult::kContinue;
						}

						if (utils::GetARMOSpacesuitType(armor, instanceData) == utils::ARMOTypeSpacesuit::SpacesuitBody && 
							utils::ARMOHasKeyword(armor, instanceData, genitalRevealKeyword)) 
						{
							has_genital_reveal_keyword = 1.f;
							return RE::BSContainer::ForEachResult::kStop;
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});

				return has_genital_reveal_keyword;
			} else { 
				/* When apparels are visible, 
				* if consealingSlots is occupied, there must be genitalRevealKeyword in all occupying apperal ARMOs for the result to be true,
				* if consealingSlots is not occupied, there must be genitalConsealKeyword in any apperal ARMO for the result to be false
				*/
				bool consealingSlotsOccupied = false;
				bool occupantsHaveRevealKeyword = true;
				bool hasConsealKeyword = false;

				a_actor->ForEachEquippedItem(
					[&consealingSlotsOccupied, &occupantsHaveRevealKeyword, &hasConsealKeyword, genitalRevealKeyword, genitalConsealKeyword](const RE::BGSInventoryItem& item) -> RE::BSContainer::ForEachResult {
						auto armor = item.object->As<RE::TESObjectARMO>();
						if (!armor) {
							return RE::BSContainer::ForEachResult::kContinue;
						}
						auto instanceData = reinterpret_cast<RE::TESObjectARMOInstanceData*>(item.instanceData.get());


						if (!utils::IsARMOSpacesuit(armor, instanceData)){
							if (utils::GetARMOModelOccupiedSlots(armor) & consealingSlots) {
								consealingSlotsOccupied = true;
								if (!utils::ARMOHasKeyword(armor, instanceData, genitalRevealKeyword)) {
									occupantsHaveRevealKeyword = false;
									return RE::BSContainer::ForEachResult::kStop;
								}
							} else {
								if (utils::ARMOHasKeyword(armor, instanceData, genitalConsealKeyword)) {
									hasConsealKeyword = true;
									return RE::BSContainer::ForEachResult::kContinue;
								}
							}
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});

				if (consealingSlotsOccupied) {
					return occupantsHaveRevealKeyword ? true : false;
				} else {
					return hasConsealKeyword ? false : true;
				}
			}
			return true;
		}

		bool ActorHasGenitalOverride(RE::TESNPC* a_npc) {
			if (!a_npc->shapeBlendData) {
				return false;
			}

			auto& morphKeys = (*a_npc->shapeBlendData);

			if (!morphKeys.contains(genitalOverrideMorphKey)) {
				return false;
			}

			auto& value = morphKeys[genitalOverrideMorphKey];

			return value == 1;
		}

		void SetActorGenitalOverride(RE::TESNPC* a_npc, bool a_override) {
			if (a_override) {
				if (!a_npc->shapeBlendData) {
					a_npc->shapeBlendData = new RE::BSTHashMap<RE::BSFixedStringCS, float>();
				}

				auto& morphKeys = (*a_npc->shapeBlendData);

				morphKeys[genitalOverrideMorphKey] = 1;

			} else {
				if (!a_npc->shapeBlendData) {
					return;
				}

				auto& morphKeys = (*a_npc->shapeBlendData);

				if (!morphKeys.contains(genitalOverrideMorphKey)) {
					return;
				}

				morphKeys[genitalOverrideMorphKey] = 0;
			}
		}

		RE::BGSKeyword* GetGenitalConsealKeyword(bool a_forceReload = false)
		{
			std::shared_lock read_lock(this->m_genitalConsealKeyword_ReadWriteLock);
			if (a_forceReload || !m_genitalConsealKeyword) {
				read_lock.unlock();
				std::unique_lock write_lock(this->m_genitalConsealKeyword_ReadWriteLock);

				m_genitalConsealKeyword = RE::TESObjectREFR::LookupByEditorID<RE::BGSKeyword>(genitalConsealKeywordEditorID);

				if (!m_genitalConsealKeyword) {
					logger::error("Genital Conseal Keyword not found, EditorID: {}", genitalConsealKeywordEditorID);
					return nullptr;
				}

				return m_genitalConsealKeyword;
			}
			return m_genitalConsealKeyword;
		}

		RE::BGSKeyword* GetGenitalRevealKeyword(bool a_forceReload = false)
		{
			std::shared_lock read_lock(this->m_genitalRevealKeyword_ReadWriteLock);
			if (a_forceReload || !m_genitalRevealKeyword) {
				read_lock.unlock();
				std::unique_lock write_lock(this->m_genitalRevealKeyword_ReadWriteLock);

				m_genitalRevealKeyword = RE::TESObjectREFR::LookupByEditorID<RE::BGSKeyword>(genitalRevealKeywordEditorID);

				if (!m_genitalRevealKeyword) {
					logger::error("Genital Reveal Keyword not found, EditorID: {}", genitalRevealKeywordEditorID);
					return nullptr;
				}

				return m_genitalRevealKeyword;
			}
			return m_genitalRevealKeyword;
		}

		RE::BGSHeadPart* GetDefaultGenitalHeadpart(RE::TESNPC* a_npc) {
			auto race = a_npc->GetRace();
			auto sex = a_npc->GetSex();
			auto skintone_id = a_npc->skinToneIndex;

			return GetDefaultGenitalHeadpart_Impl(race, sex, skintone_id);
		}

		RE::BGSHeadPart* GetGenitalHeadpart(RE::TESNPC* a_npc, const std::string& a_genital_name)
		{
			auto race = a_npc->GetRace();
			auto sex = a_npc->GetSex();
			auto skintone_id = a_npc->skinToneIndex;

			return GetGenitalHeadpart_Impl(race, a_genital_name, sex, skintone_id);
		}

		inline bool ClearActorGenital(RE::Actor* a_actor)
		{
			return ChangeActorGenital(a_actor, nullptr);
		}

		// Return true if hidden state changed, thus requires actor update appearance, not thread-safe
		bool HideActorGenitalUnsafe(RE::Actor* a_actor)
		{
			if (!a_actor) {
				return false;
			}
			auto npc = a_actor->GetNPC();
			if (!npc->shapeBlendData) {
				npc->shapeBlendData = new RE::BSTHashMap<RE::BSFixedStringCS, float>();
			}

			auto& morphKeys = (*npc->shapeBlendData);

			auto& value = morphKeys[hideGenitalMorphKey];

			if (value == 1) {
				return false;
			} else {
				this->events::EventDispatcher<events::ActorRevealingStateChangedEvent>::Dispatch(a_actor, false, true);
				value = 1;
				return true;
			}
		}

		// Return true if hidden state changed, thus requires actor update appearance, not thread-safe
		bool RevealActorGenitalUnsafe(RE::Actor* a_actor)
		{
			if (!a_actor) {
				return false;
			}
			auto npc = a_actor->GetNPC();
			if (!npc->shapeBlendData) {
				return false;
			}

			auto& morphKeys = (*npc->shapeBlendData);

			auto& value = morphKeys[hideGenitalMorphKey];

			if (value == 0) {
				return false;
			} else {
				this->events::EventDispatcher<events::ActorRevealingStateChangedEvent>::Dispatch(a_actor, true, false);
				value = 0;
				return true;
			}
		}

		bool IsActorGenitalHiddenUnsafe(RE::Actor* a_actor)
		{
			auto npc = a_actor->GetNPC();
			if (!npc->shapeBlendData) {
				return false;
			}

			auto& morphKeys = (*npc->shapeBlendData);

			if (!morphKeys.contains(hideGenitalMorphKey)) {
				return false;
			}

			auto& value = morphKeys[hideGenitalMorphKey];

			return value == 1;
		}

		RE::BGSHeadPart* GetActorGenital(RE::Actor* a_actor)
		{
			{
				tbb::concurrent_hash_map<RE::TESFormID, RE::BGSHeadPart*>::const_accessor accessor;
				if (m_actor_genital_cache.find(accessor, a_actor->formID)) {
					return accessor->second;
				}
			}

			auto actor_genital = GetActorGenital_Impl(a_actor->GetNPC());
			m_actor_genital_cache.insert({ a_actor->formID, actor_genital });
			return actor_genital;
		}

		inline bool ActorHasGenital(RE::Actor* a_actor)
		{
			return GetActorGenital(a_actor) != nullptr;
		}

		bool ChangeActorGenital(RE::Actor* a_actor, RE::BGSHeadPart* a_genital_headpart)
		{
			bool headpart_changed = false;
			RE::BGSHeadPart* prev_genital_headpart = nullptr;
			auto npc = a_actor->GetNPC();
			{
				auto  acc = npc->headParts.lock();
				auto& headparts = *acc;
				bool  has_this_genital = false;

				for (auto it = headparts.begin(); it != headparts.end(); ++it) {
					auto headpart = *it;
					if (IsRegisteredGenitalHeadpart(headpart)) {
						if (headpart == a_genital_headpart) {
							has_this_genital = true;
						} else {
							prev_genital_headpart = headpart;
							headparts.erase(it);
							headpart_changed = true;
						}
					}
				}
				if (!has_this_genital && a_genital_headpart) {
					headparts.push_back(a_genital_headpart);
					headpart_changed = true;
				}
			}

			m_actor_genital_cache.insert({ a_actor->formID, a_genital_headpart});

			if (headpart_changed) {
				this->events::EventDispatcher<events::ActorGenitalChangedEvent>::Dispatch(a_actor, prev_genital_headpart, a_genital_headpart);
				logger::info("Genital headpart changed for NPC {}", utils::make_str(npc));
				return true;
			} else {
				return false;
			}
		}

		inline bool IsRegisteredGenitalHeadpart(RE::BGSHeadPart* a_headpart) const
		{
			return m_genitalHeadparts.contains(a_headpart);
		}

		bool RegisterGenitalHeadpart(const std::string& a_race_editorid, const std::string& a_genital_name, RE::SEX a_sex, size_t a_skintone_index, const std::string& a_headpart_editorid)
		{
			auto race = RE::TESObjectREFR::LookupByEditorID<RE::TESRace>(a_race_editorid);
			if (!race) {
				logger::warn("Cannot find race with EditorID {} while registering {}", a_race_editorid, a_headpart_editorid);
				return false;
			}

			auto headpart = RE::TESObjectREFR::LookupByEditorID<RE::BGSHeadPart>(a_headpart_editorid);
			if (!headpart) {
				logger::warn("Cannot find headpart with EditorID {} while registering for race {}", a_headpart_editorid, a_race_editorid);
				return false;
			}

			auto success = RegisterGenitalHeadpart_Impl(race, a_genital_name, a_sex, a_skintone_index, headpart);

			if (success) {
				logger::info("Registered genital headpart EditorID {} for race {}, genital type {}, sex {}, skintone {}",
					a_headpart_editorid,
					utils::make_str(race),
					a_genital_name,
					utils::GetSexString(a_sex),
					a_skintone_index);
			} else {
				logger::warn("Failed to register genital headpart EditorID {} for race {}, genital type {}, sex {}, skintone {}",
					a_headpart_editorid,
					utils::make_str(race),
					a_genital_name,
					utils::GetSexString(a_sex),
					a_skintone_index);
			}

			return success;
		}
		
		bool SetRaceSexDefaultGenitalName(const std::string& a_race_editorid, RE::SEX a_sex, const std::string& a_default_genital_name)
		{
			auto race = RE::TESObjectREFR::LookupByEditorID<RE::TESRace>(a_race_editorid);
			if (!race) {
				logger::warn("Cannot set default genital name {} for race with EditorID {}. Race EditorID not found.", a_default_genital_name, a_race_editorid);
				return false;
			}

			auto success = SetRaceSexDefaultGenitalName_Impl(race, a_sex, a_default_genital_name);

			if (success) {
				logger::info("Set default genital name {} for race {}, sex {}", 
					a_default_genital_name, 
					utils::make_str(race),
					utils::GetSexString(a_sex));
			} else {
				logger::warn("Failed to set default genital name {} for race {}, sex {}",
					a_default_genital_name,
					utils::make_str(race),
					utils::GetSexString(a_sex));
			}
		}

		bool SetRaceSexGenitalTypeDefaultHeadpart(const std::string& a_race_editorid, const std::string& a_genital_name, RE::SEX a_sex, const std::string& a_headpart_editorid, bool a_allowNewRace = false)
		{
			auto race = RE::TESObjectREFR::LookupByEditorID<RE::TESRace>(a_race_editorid);
			if (!race) {
				logger::warn("Cannot find race with EditorID {} while registering default genital {} for type {}", a_race_editorid, a_headpart_editorid, a_genital_name);
				return false;
			}

			auto headpart = RE::TESObjectREFR::LookupByEditorID<RE::BGSHeadPart>(a_headpart_editorid);
			if (!headpart) {
				logger::warn("Cannot find headpart with EditorID {} while registering default genital for race {}, type {}", a_headpart_editorid, a_race_editorid, a_genital_name);
				return false;
			}

			auto success = SetRaceSexGenitalTypeDefaultHeadpart_Impl(race, a_genital_name, a_sex, headpart, a_allowNewRace);

			if (success) {
				logger::info("Set default genital headpart EditorID {} for race {}, genital type {}, sex {}",
					a_headpart_editorid,
					utils::make_str(race),
					a_genital_name,
					utils::GetSexString(a_sex));
			} else {
				logger::warn("Failed to set default genital headpart EditorID {} for race {}, genital type {}, sex {}",
					a_headpart_editorid,
					utils::make_str(race),
					a_genital_name,
					utils::GetSexString(a_sex));
			}

			return success;
		}

		void Clear()
		{
			m_genitalHeadparts.clear();
			m_maleRaceGenitalHeadparts.clear();
			m_femaleRaceGenitalHeadparts.clear();
			m_actor_genital_cache.clear();
		}

		size_t LoadGenitalHeadpartsFromJSON(const std::string& a_jsonConfigFolder)
		{
			size_t num_loaded = 0;

			std::filesystem::path root_path(a_jsonConfigFolder);
			if (!std::filesystem::exists(root_path)) {
				logger::error("Root folder does not exist: {}", a_jsonConfigFolder);
				return num_loaded;
			}

			Clear();

			this->EventDispatcher<events::GenitalDataReloadEvent>::Dispatch();

			// For all json files in the folder
			for (const auto& entry : std::filesystem::directory_iterator(root_path)) {
				if (entry.is_regular_file() && entry.path().extension() == ".json") {
					const auto& file_path = entry.path();
					auto file_name = file_path.stem().string();
					auto file_extension = file_path.extension().string();

					logger::info("Loading genital headparts from file: {}", file_path.string());

					size_t num_loaded_from_file = ParseJsonFile(file_path.string());

					logger::info("Loaded {} genital headparts from file: {}", num_loaded_from_file, file_path.string());

					num_loaded += num_loaded_from_file;
				}
			}

			// Check all RaceGenitalListRegistry if they are valid
			logger::info("Checking genital registries for races...");
			for (auto it = m_maleRaceGenitalHeadparts.begin(); it != m_maleRaceGenitalHeadparts.end(); ++it) {
				if (!it->second.IsValid()) {
					logger::warn("Race {} male doesn't have default genital type? Please fix this issue.", utils::make_str(it->first));
				}
			}
			for (auto it = m_femaleRaceGenitalHeadparts.begin(); it != m_femaleRaceGenitalHeadparts.end(); ++it) {
				if (!it->second.IsValid()) {
					logger::warn("Race {} female doesn't have default genital type? Please fix this issue.", utils::make_str(it->first));
				}
			}
			return num_loaded;
		}

	private:
		tbb::concurrent_unordered_set<RE::BGSHeadPart*> m_genitalHeadparts;
		_Storage_T                                      m_maleRaceGenitalHeadparts;
		_Storage_T                                      m_femaleRaceGenitalHeadparts;

		tbb::concurrent_hash_map<RE::TESFormID, time_t>      m_actor_pending_update;
		tbb::concurrent_hash_map<RE::TESFormID, ActorGenitalUpdateInfo> m_actor_pending_change_genital;

		tbb::concurrent_hash_map<RE::TESFormID, RE::BGSHeadPart*> m_actor_genital_cache;

		RE::BGSKeyword*   m_genitalOverrideKeyword{ nullptr };
		std::shared_mutex m_genitalOverrideKeyword_ReadWriteLock;
		RE::BGSKeyword*   m_genitalConsealKeyword{ nullptr };
		std::shared_mutex m_genitalConsealKeyword_ReadWriteLock;
		RE::BGSKeyword*   m_genitalRevealKeyword{ nullptr };
		std::shared_mutex m_genitalRevealKeyword_ReadWriteLock;

		RE::Actor* m_playerRef{ nullptr };

		RE::BGSHeadPart* GetGenitalHeadpart_Impl(RE::TESRace* a_race, const std::string& a_genital_name, RE::SEX a_sex, size_t a_skintone_index) const
		{
			_Storage_T::const_accessor accessor;

			switch (a_sex) {
			case RE::SEX::kMale:
				if (!m_maleRaceGenitalHeadparts.find(accessor, a_race)) {
					return nullptr;
				}
				break;
			case RE::SEX::kFemale:
				if (!m_femaleRaceGenitalHeadparts.find(accessor, a_race)) {
					return nullptr;
				}
				break;
			}

			const RaceGenitalListRegistry& genitalList = accessor->second;
			return genitalList.GetGenitalHeadpart(a_genital_name, a_skintone_index);
		}

		RE::BGSHeadPart* GetDefaultGenitalHeadpart_Impl(RE::TESRace* a_race, RE::SEX a_sex, size_t a_skintone_index)
		{
			_Storage_T::accessor accessor;

			switch (a_sex) {
			case RE::SEX::kMale:
				if (!m_maleRaceGenitalHeadparts.find(accessor, a_race)) {
					return nullptr;
				}
				break;
			case RE::SEX::kFemale:
				if (!m_femaleRaceGenitalHeadparts.find(accessor, a_race)) {
					return nullptr;
				}
				break;
			}

			RaceGenitalListRegistry& genitalList = accessor->second;
			return genitalList.GetDefaultGenitalHeadpart(a_skintone_index);
		}

		bool RegisterGenitalHeadpart_Impl(RE::TESRace* a_race, const std::string& a_genital_name, RE::SEX a_sex, size_t a_skintone_index, RE::BGSHeadPart* a_headpart)
		{
			if (!a_race || !a_headpart) {
				return false;
			}

			_Storage_T::accessor accessor;
			
			switch (a_sex) {
			case RE::SEX::kMale:
				m_maleRaceGenitalHeadparts.insert(accessor, a_race);
				break;
			case RE::SEX::kFemale:
				m_femaleRaceGenitalHeadparts.insert(accessor, a_race);
				break;
			}

			RaceGenitalListRegistry& genitalList = accessor->second;

			bool success = false;

			success = genitalList.SetGenitalHeadpart(a_genital_name, a_skintone_index, a_headpart);
			
			if (success) {
				m_genitalHeadparts.insert(a_headpart);
			}
			return success;
		}

		bool SetRaceSexDefaultGenitalName_Impl(RE::TESRace* a_race, RE::SEX a_sex, const std::string& a_default_genital_name)
		{
			if (!a_race) {
				return false;
			}

			_Storage_T::accessor accessor;

			switch (a_sex) {
			case RE::SEX::kMale:
				if (!m_maleRaceGenitalHeadparts.find(accessor, a_race)) {
					return false;
				}
				break;
			case RE::SEX::kFemale:
				if (!m_femaleRaceGenitalHeadparts.find(accessor, a_race)) {
					return false;
				}
				break;
			}
			RaceGenitalListRegistry& genitalList = accessor->second;
			return genitalList.SetDefaultRaceSexGenitalHeadpart(a_default_genital_name);
		}

		bool SetRaceSexGenitalTypeDefaultHeadpart_Impl(RE::TESRace* a_race, const std::string& a_genital_name, RE::SEX a_sex, RE::BGSHeadPart* a_headpart, bool a_allowNewRace = false)
		{
			if (!a_race || !a_headpart) {
				return false;
			}

			_Storage_T::accessor accessor;
			switch (a_sex) {
			case RE::SEX::kMale:
				if (!a_allowNewRace) {
					if (!m_maleRaceGenitalHeadparts.find(accessor, a_race)) {
						return false;
					}
				} else {
					m_maleRaceGenitalHeadparts.insert(accessor, a_race);
				}
				break;
			case RE::SEX::kFemale:
				if (!a_allowNewRace) {
					if (!m_femaleRaceGenitalHeadparts.find(accessor, a_race)) {
						return false;
					}
				} else {
					m_femaleRaceGenitalHeadparts.insert(accessor, a_race);
				}
				break;
			}

			RaceGenitalListRegistry& genitalList = accessor->second;
			genitalList.SetDefaultGenitalHeadpart(a_genital_name, a_headpart);
			m_genitalHeadparts.insert(a_headpart);
			return true;
		}

		RE::BGSHeadPart* GetActorGenital_Impl(RE::TESNPC* a_npc)
		{
			auto  acc = a_npc->headParts.lock();
			auto& headparts = *acc;

			for (auto it = headparts.begin(); it != headparts.end(); ++it) {
				auto headpart = *it;
				if (IsRegisteredGenitalHeadpart(headpart)) {
					return headpart;
				}
			}
		}

		/* JSON file format:
		* { 
		*	raceEditorID: { 
		*		"male": { 
		*			"default": genital_type_str, // Check if exists
		*			genital_type_str0: {
		*				"default": headpartEditorID, // Check if exists
		*				"genitals": [
		*					{ 
		*						"EditorID" : headpartEditorID0,
		*						"skintoneIndex" : skintone_id0 
		*					},
		*					{ 
		*						"EditorID" : headpartEditorID1,
		*						"skintoneIndex" : skintone_id1 
		*					},
		*					...
		*				]
		*			},
		*			genital_type_str1: {
		*				...
		*			},
		*			...
		*		},
		*		"female": {
		*			...
		*		}
		*	}
		* }
		*/
		size_t ParseJsonFile(const std::string& a_filePath)
		{
			size_t num_loaded = 0;

			std::ifstream file(a_filePath);
			if (!file.is_open()) {
				logger::error("Failed to open file: {}", a_filePath);
				return num_loaded;
			}

			nlohmann::json j;
			try {
				file >> j;
			} catch (const nlohmann::json::parse_error& e) {
				logger::error("Failed to parse JSON file: {}", a_filePath);
				logger::error("Error: {}", e.what());
				return num_loaded;
			}

			for (auto& [raceEditorID, raceData] : j.items()) {
				if (raceEditorID.starts_with("$")) {
					continue;
				}

				if (!RE::TESForm::LookupByEditorID<RE::TESRace>(raceEditorID)) {
					logger::warn("Race EditorID {} not found. Skipping", raceEditorID);
				}

				for (auto& [sexEntry, sexData] : raceData.items()) {
					RE::SEX a_sex;
					if (sexEntry == "male") {
						a_sex = RE::SEX::kMale;
					} else if (sexEntry == "female") {
						a_sex = RE::SEX::kFemale;
					} else {
						logger::warn("Invalid sex entry: '{}', should be either 'male' or 'female'", sexEntry);
						continue;
					}

					for (auto& [genitalType, genitalData] : sexData.items()) {
						if (genitalType == "default") {
							auto defaultGenitalName = genitalData.get<std::string>();
							SetRaceSexDefaultGenitalName(raceEditorID, a_sex, defaultGenitalName);
							continue;
						}

						for (auto& [entry, data] : genitalData.items()) {
							if (entry == "default") {
								auto defaultHeadpartEditorID = data.get<std::string>();
								SetRaceSexGenitalTypeDefaultHeadpart(raceEditorID, genitalType, a_sex, defaultHeadpartEditorID, true);
							} else if (entry == "genitals") {
								for (auto& headpartData : data) {
									if (const auto& headpartEditorID_entry = headpartData.find("EditorID"); headpartEditorID_entry != headpartData.end()) {
										auto             headpartEditorID = headpartEditorID_entry.value().get<std::string>();
										RE::BGSHeadPart* headpart = RE::TESForm::LookupByEditorID<RE::BGSHeadPart>(headpartEditorID);

										if (!headpart) {
											logger::warn("Headpart EditorID {} not found. Skipping", headpartEditorID);
											continue;
										}

										// headpartData must have "skintoneIndex" field
										if (const auto& skintoneIndex = headpartData.find("skintoneIndex"); skintoneIndex != headpartData.end()) {
											auto skintone_id = skintoneIndex.value().get<size_t>();
											num_loaded += RegisterGenitalHeadpart(raceEditorID, genitalType, a_sex, skintone_id, headpartEditorID);
											this->events::EventDispatcher<events::GenitalDataLoadingEvent>::Dispatch(headpart, j, headpartData);
										} else {
											logger::warn("Skintone index entry not found for headpart {}, genital type {}, sex {}, race {}",
												headpartEditorID,
												genitalType,
												utils::GetSexString(a_sex),
												raceEditorID);
										}
									} else {
										logger::warn("Headpart EditorID entry not found for genital type {}, sex {}, race {}",
											genitalType,
											utils::GetSexString(a_sex),
											raceEditorID);
									}
								}
							}
						}
					}
				}
			}

			return num_loaded;
		}

		ActorHeadpartGenitalManager(){};
	};
}
