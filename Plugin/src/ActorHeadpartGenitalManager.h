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
	inline constexpr uint64_t    consealingSlots = 1 << 13 | 1 << 3; // Slot 13 and 3 (body) are consealing slots
	inline constexpr const char* genitalConsealKeywordEditorID = "DAFGenitalConseal";
	inline constexpr const char* genitalRevealKeywordEditorID = "DAFGenitalReveal";
	inline constexpr const char* hideGenitalMorphKey = "HideGenital";
	inline constexpr const char* genitalOverrideMorphKey = "DAF_GenitalTypeOverride";
	inline constexpr time_t      ActorPendingUpdateRevealingDelay_ms = 200;
	inline constexpr time_t      ActorPendingUpdateRevealingCycle_ms = 1000;

	class ActorHeadpartGenitalManager :
		public utils::SingletonBase<ActorHeadpartGenitalManager>,
		public events::EventDispatcher<events::ActorEquipManagerEquipEvent>::Listener,
		public events::EventDispatcher<events::ActorUpdateEvent>::Listener,
		public events::EventDispatcher<events::ActorFirstUpdateEvent>::Listener,
		public events::EventDispatcher<events::GameDataLoadedEvent>::Listener,
		public events::SaveLoadEventDispatcher::Listener,
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

		void OnEvent(const events::ActorEquipManagerEquipEvent& a_event, events::EventDispatcher<events::ActorEquipManagerEquipEvent>* a_dispatcher) override;

		void OnEvent(const events::ActorUpdateEvent& a_event, events::EventDispatcher<events::ActorUpdateEvent>* a_dispatcher) override;

		void OnEvent(const events::ActorFirstUpdateEvent& a_event, events::EventDispatcher<events::ActorFirstUpdateEvent>* a_dispatcher) override;

		void OnEvent(const events::GameDataLoadedEvent& a_event, events::EventDispatcher<events::GameDataLoadedEvent>* a_dispatcher) override {
			logger::info("GameDataLoadedEvent, message_type {}", std::to_underlying(a_event.messageType));
		}

		void OnEvent(const events::SaveLoadEvent& a_event, events::EventDispatcher<events::SaveLoadEvent>* a_dispatcher) override;

		void Register()
		{
			events::ArmorOrApparelEquippedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorEquipManagerEquipEvent>::AddStaticListener(this);
			events::GameDataLoadedEventDispatcher::GetSingleton()->AddStaticListener(this);
			events::ActorUpdatedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorUpdateEvent>::AddStaticListener(this);
			events::ActorUpdatedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorFirstUpdateEvent>::AddStaticListener(this);
			events::SaveLoadEventDispatcher::GetSingleton()->AddStaticListener(this);
		}

		std::vector<std::string> QueryAvailibleGenitalTypesForActor(RE::Actor* a_actor);

		void ChangeActorGenital(RE::Actor* a_actor, const std::string& a_genitalName, bool a_setOverride = true);

		bool EvaluateRevealingStateAndApply(RE::Actor* a_actor);

		/*
		Spacesuit is visible, there must be GenitalRevealKeyword in the spacesuit body ARMO for the result to be true
		When apparels are visible, 
		* if consealingSlots is occupied, there must be genitalRevealKeyword in all occupying apperal ARMOs for the result to be true,
		* if consealingSlots is not occupied, there must be genitalConsealKeyword in any apperal ARMO for the result to be false
		*/
		bool EvaluateActorGenitalRevealingState(RE::Actor* a_actor);

		bool ActorHasGenitalOverride(RE::TESNPC* a_npc);

		void SetActorGenitalOverride(RE::TESNPC* a_npc, bool a_override);

		RE::BGSKeyword* GetGenitalConsealKeyword(bool a_forceReload = false);

		RE::BGSKeyword* GetGenitalRevealKeyword(bool a_forceReload = false);

		RE::BGSHeadPart* GetDefaultGenitalHeadpart(RE::TESNPC* a_npc);

		RE::BGSHeadPart* GetGenitalHeadpart(RE::TESNPC* a_npc, const std::string& a_genital_name);

		bool ClearActorGenital(RE::Actor* a_actor);

		// Return true if hidden state changed, thus requires actor update appearance, not thread-safe
		bool HideActorGenitalUnsafe(RE::Actor* a_actor);

		// Return true if hidden state changed, thus requires actor update appearance, not thread-safe
		bool RevealActorGenitalUnsafe(RE::Actor* a_actor);

		bool IsActorGenitalHiddenUnsafe(RE::Actor* a_actor);

		RE::BGSHeadPart* GetActorGenital(RE::Actor* a_actor);

		bool ActorHasGenital(RE::Actor* a_actor);

		bool ChangeActorGenital(RE::Actor* a_actor, RE::BGSHeadPart* a_genital_headpart);

		inline bool IsRegisteredGenitalHeadpart(RE::BGSHeadPart* a_headpart) const
		{
			return m_genitalHeadparts.contains(a_headpart);
		}

		bool RegisterGenitalHeadpart(const std::string& a_race_editorid, const std::string& a_genital_name, RE::SEX a_sex, size_t a_skintone_index, const std::string& a_headpart_editorid);
		
		bool SetRaceSexDefaultGenitalName(const std::string& a_race_editorid, RE::SEX a_sex, const std::string& a_default_genital_name);

		bool SetRaceSexGenitalTypeDefaultHeadpart(const std::string& a_race_editorid, const std::string& a_genital_name, RE::SEX a_sex, const std::string& a_headpart_editorid, bool a_allowNewRace = false);

		inline void Clear()
		{
			m_genitalHeadparts.clear();
			m_maleRaceGenitalHeadparts.clear();
			m_femaleRaceGenitalHeadparts.clear();
			m_actor_genital_cache.clear();
		}

		size_t LoadGenitalHeadpartsFromJSON(const std::string& a_jsonConfigFolder);

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

		void CacheGenitalHeadpart(RE::Actor* actor, RE::BGSHeadPart* a_headpart) {
			tbb::concurrent_hash_map<RE::TESFormID, RE::BGSHeadPart*>::accessor accessor;
			m_actor_genital_cache.insert(accessor, actor->formID);
			accessor->second = a_headpart;
		}

		RE::BGSHeadPart* GetGenitalHeadpart_Impl(RE::TESRace* a_race, const std::string& a_genital_name, RE::SEX a_sex, size_t a_skintone_index) const;

		RE::BGSHeadPart* GetDefaultGenitalHeadpart_Impl(RE::TESRace* a_race, RE::SEX a_sex, size_t a_skintone_index);

		bool RegisterGenitalHeadpart_Impl(RE::TESRace* a_race, const std::string& a_genital_name, RE::SEX a_sex, size_t a_skintone_index, RE::BGSHeadPart* a_headpart);

		bool SetRaceSexDefaultGenitalName_Impl(RE::TESRace* a_race, RE::SEX a_sex, const std::string& a_default_genital_name);

		bool SetRaceSexGenitalTypeDefaultHeadpart_Impl(RE::TESRace* a_race, const std::string& a_genital_name, RE::SEX a_sex, RE::BGSHeadPart* a_headpart, bool a_allowNewRace = false);

		RE::BGSHeadPart* GetActorGenital_Impl(RE::TESNPC* a_npc);

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
		size_t ParseJsonFile(const std::string& a_filePath);

		ActorHeadpartGenitalManager(){};
	};
}