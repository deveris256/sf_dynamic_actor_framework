#pragma once
#include "ActorHeadpartGenitalManager.h"
#include "NodeChainLerpGenerator.h"
#include "SingletonBase.h"

namespace daf
{
	extern constexpr time_t           ArousalActorValueEvaluationCooldown{ 500 };
	extern constexpr std::string_view ArousalActorValueEditorID{ "DAFGen_ArousalLevel" };

	class ActorGenitalAnimManager : 
		public utils::SingletonBase<ActorGenitalAnimManager>,
		public events::EventDispatcher<events::ActorFirstUpdateEvent>::Listener,
		public events::EventDispatcher<events::ActorUpdateEvent>::Listener,
		public events::EventDispatcher<events::ActorLoadedEvent>::Listener,
		public events::EventDispatcher<events::ActorGenitalChangedEvent>::Listener,
		public events::EventDispatcher<events::ActorRevealingStateChangedEvent>::Listener,
		public events::EventDispatcher<events::GenitalDataLoadingEvent>::Listener,
		public events::EventDispatcher<events::GenitalDataReloadEvent>::Listener
	{
		friend class utils::SingletonBase<ActorGenitalAnimManager>;
	public:
		using ChainNodeData = NodeChainLerpGenerator::ChainNodeData;
		using PhysicsData = NodeChainLerpGenerator::PhysicsData;

		class GenitalAnimData
		{
		public:
			std::string                rootNodeName;
			std::vector<ChainNodeData> chainNodeData;
			PhysicsData                physicsData;
			time_t                     fullErectionTimeMs{ 5000 };
		};

		using _Pending_Actor_List_T = tbb::concurrent_hash_map<RE::TESFormID, time_t>;
		using _ActorValue_Evaluated_List_T = tbb::concurrent_hash_map<RE::TESFormID, time_t>;
		using _Generator_Map_T = tbb::concurrent_hash_map<RE::TESFormID, std::unique_ptr<NodeChainLerpGenerator>>;
		using _Genital_Anim_Data_Map_T = tbb::concurrent_hash_map<RE::BGSHeadPart*, GenitalAnimData>;

		void OnEvent(const events::ActorUpdateEvent& a_event, events::EventDispatcher<events::ActorUpdateEvent>* a_dispatcher) override
		{
			auto actor = a_event.actor;
			auto npc = actor->GetNPC();
			auto when = a_event.when();

			_Generator_Map_T::accessor accessor;
			if (!m_actorGenitalAnimGenerators.find(accessor, actor->formID)) {
				return;
			}
			auto& generator = accessor->second;
			generator->update(when);

			_ActorValue_Evaluated_List_T::accessor actorValueEvaluatedAccessor;
			if (!m_actorValueEvaluatedList.find(actorValueEvaluatedAccessor, actor->formID)) {
				return;
			}

			if (when - actorValueEvaluatedAccessor->second < ArousalActorValueEvaluationCooldown) {
				return;
			}
			auto arousalLevel = GetArousalLevel(actor);
			generator->setInterpolatedTargetsPropotional(arousalLevel / 100.f);
			actorValueEvaluatedAccessor->second = when;
		}
		
		void OnEvent(const events::ActorFirstUpdateEvent& a_event, events::EventDispatcher<events::ActorFirstUpdateEvent>* a_dispatcher) override
		{
			auto actor = a_event.actor;
			auto npc = actor->GetNPC();

			{
				_Generator_Map_T::const_accessor gen_accessor;
				if (m_actorGenitalAnimGenerators.find(gen_accessor, actor->formID)) {
					return;
				}
			}

			if (actor->IsPlayerRef()) {
				actor->SetActorValue(*m_arousalActorValue, 100.0);
			}

			_Pending_Actor_List_T::accessor accessor;
			if (m_actorsPendingAttachGenerator.find(accessor, actor->formID)) {
				BuildNodeChainForActor(actor, GetArousalLevel(actor) / 100.f);
				m_actorsPendingAttachGenerator.erase(accessor);
			} else if (actor->IsPlayerRef()) {
				BuildNodeChainForActor(actor, GetArousalLevel(actor) / 100.f);
			}
		}

		void OnEvent(const events::ActorLoadedEvent& a_event, events::EventDispatcher<events::ActorLoadedEvent>* a_dispatcher) override
		{
			auto actor = a_event.actor;
			auto npc = actor->GetNPC();
			bool isUnloaded = !a_event.loaded;

			if (isUnloaded) {
				UnloadNodeChainForActor(actor);
			} else {
				m_actorsPendingAttachGenerator.insert({ actor->formID, a_event.when() });
			}
		}

		void OnEvent(const events::ActorGenitalChangedEvent& a_event, events::EventDispatcher<events::ActorGenitalChangedEvent>* a_dispatcher) override
		{
			auto actor = a_event.actor;
			auto npc = actor->GetNPC();

			if (BuildNodeChainForActor(actor, GetArousalLevel(actor) / 100.f)) {
				m_actorsPendingAttachGenerator.erase(actor->formID);  // No need to build again
			}
		}

		void OnEvent(const events::ActorRevealingStateChangedEvent& a_event, events::EventDispatcher<events::ActorRevealingStateChangedEvent>* a_dispatcher) override
		{
			auto actor = a_event.actor;
			auto npc = actor->GetNPC();
			bool isPreviouslyHiding = a_event.oldState;
			bool isNowHiding = a_event.newState;

			_Generator_Map_T::accessor accessor;
			if (m_actorGenitalAnimGenerators.find(accessor, actor->formID)) {
				auto& generator = accessor->second;
				if (isNowHiding) {
					generator->freezeAt(0.0f, 0);  // Freeze at Minima immediately
				} else {
					generator->unfreezeAutoTransitionTime(1.0f);
				}
			}
		}

		void OnEvent(const events::GenitalDataLoadingEvent& a_event, events::EventDispatcher<events::GenitalDataLoadingEvent>* a_dispatcher) override
		{
			auto headPart = a_event.genitalHeadpart;
			auto& genitalData = a_event.genitalData;
			auto& configData = a_event.configData;

			try {
				auto animData = LoadAnimData(genitalData, configData);
				m_genitalAnimData.insert({ headPart, animData });
			}catch (const std::exception& e) {
				logger::error("ActorGenitalAnimManager: Failed to load genital anim data for headpart {}. Error: {}", headPart->formEditorID.c_str(), e.what());
				return;
			}
		}

		void OnEvent(const events::GenitalDataReloadEvent& a_event, events::EventDispatcher<events::GenitalDataReloadEvent>* a_dispatcher) override
		{
			m_arousalActorValue = RE::TESObjectREFR::LookupByEditorID<RE::ActorValueInfo>(ArousalActorValueEditorID);
			if (!m_arousalActorValue) {
				logger::warn("ActorGenitalAnimManager: Arousal actor value not found. Arousal actor value editor ID: {}", ArousalActorValueEditorID);
			}
			Clear();
		}

		void Clear()
		{
			m_actorsPendingAttachGenerator.clear();
			m_actorValueEvaluatedList.clear();
			m_actorGenitalAnimGenerators.clear();
			m_genitalAnimData.clear();
		}

		void Register()
		{
			events::ActorUpdatedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorUpdateEvent>::AddStaticListener(this);
			events::ActorUpdatedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorFirstUpdateEvent>::AddStaticListener(this);
			events::ActorLoadedEventDispatcher::GetSingleton()->EventDispatcher<events::ActorLoadedEvent>::AddStaticListener(this);
			ActorHeadpartGenitalManager::GetSingleton().EventDispatcher<events::ActorGenitalChangedEvent>::AddStaticListener(this);
			ActorHeadpartGenitalManager::GetSingleton().EventDispatcher<events::ActorRevealingStateChangedEvent>::AddStaticListener(this);
			ActorHeadpartGenitalManager::GetSingleton().EventDispatcher<events::GenitalDataLoadingEvent>::AddStaticListener(this);
			ActorHeadpartGenitalManager::GetSingleton().EventDispatcher<events::GenitalDataReloadEvent>::AddStaticListener(this);
		}

	private:
		_Pending_Actor_List_T        m_actorsPendingAttachGenerator;
		_ActorValue_Evaluated_List_T m_actorValueEvaluatedList;
		_Generator_Map_T             m_actorGenitalAnimGenerators;
		_Genital_Anim_Data_Map_T     m_genitalAnimData;
		
		RE::ActorValueInfo* m_arousalActorValue = nullptr;

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
		*						"skintoneIndex" : skintone_id0,
		*						"animDataPreset": preset_name0 // Added, same headpart shared the same preset
		*					},
		*					{ 
		*						"EditorID" : headpartEditorID1,
		*						"skintoneIndex" : skintone_id1,
		*						"animDataPreset": preset_name1
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
		*	},
		*   "$animDataPresets": { // Added, '$' is used to indicate a special key
		*		preset_name0: {
		*			"rootNodeName": chain_root_bone_name,
		*			"fullErectionTimeSec": float,
		*			"chainNodeData": [
		*				{
		*					"id": 0,
		*					"Bone": genital_bone_name,
        *                   "Maxima": { // t = 1, or Minima (t = 0)
        *                       "Position": {
        *                           "x": -1.3338799476623535,
        *                           "y": -0.19518999755382538,
        *                           "z": -0.2290560007095337
        *                       },
        *                       "Rotation": { // XYZ Euler
        *                           "x": 0,
        *                           "y": 0,
        *                           "z": 0
        *                       },
        *                       "Scale": float, multiplied to the original scale
        *                   }
		*				},
		*				...
		*			],
		*			"physicsData": {
		*				"enabled": true,
		*				"mass": 2.0,
		*				"stiffness": float or string, // If string (for instance, "200 + 3400 * clamp(-1.2, 3*t - 2, 1.0)^4"), it's an expression evaluated at runtime against 't' (identifying where the animation is from Minima 0 to Maxima 1)
		*				"angularDamping": float or string,
		*				"linearDrag": float or string
		*			}
		*		},
		*		preset_name1: {
		*			...
		*		},
		*	}
		* }
		*/
		GenitalAnimData LoadAnimData(const nlohmann::json& genitalData, const nlohmann::json& configData)
		{
			GenitalAnimData animData;

			if (!genitalData.contains("animDataPreset")) {
				throw std::runtime_error("animDataPreset not found in genital data");
			}

			auto  animDataPreset = genitalData["animDataPreset"].get<std::string>();
			auto& animDataPresetsData = configData["$animDataPresets"][animDataPreset];

			animData.rootNodeName = animDataPresetsData["rootNodeName"].get<std::string>();
			animData.fullErectionTimeMs = time_t(animDataPresetsData["fullErectionTimeSec"].get<float>() * 1000);

			auto& chainNodeData = animDataPresetsData["chainNodeData"];
			size_t numChainNodes = chainNodeData.size();
			animData.chainNodeData.resize(numChainNodes);
			for (auto& nodeData : chainNodeData) {
				auto id = nodeData["id"].get<int>();

				ChainNodeData chainNode;
				chainNode.nodeName = nodeData["Bone"].get<std::string>();

				if (nodeData.contains("Maxima")) {
					auto& maxima = nodeData["Maxima"];
					chainNode.maxima.position = {
						-maxima["Position"]["x"].get<float>(),
						maxima["Position"]["y"].get<float>(),
						maxima["Position"]["z"].get<float>()
					};
					chainNode.maxima.xyz_euler_rotation = {
						maxima["Rotation"]["x"].get<float>(),
						maxima["Rotation"]["y"].get<float>(),
						-maxima["Rotation"]["z"].get<float>()
					};
					chainNode.maxima.scale = maxima["Scale"].get<float>();
				}

				if (nodeData.contains("Minima")) {
					auto& minima = nodeData["Minima"];
					chainNode.minima.position = {
						-minima["Position"]["x"].get<float>(),
						minima["Position"]["y"].get<float>(),
						minima["Position"]["z"].get<float>()
					};
					chainNode.minima.xyz_euler_rotation = {
						minima["Rotation"]["x"].get<float>(),
						minima["Rotation"]["y"].get<float>(),
						-minima["Rotation"]["z"].get<float>()
					};
					chainNode.minima.scale = minima["Scale"].get<float>();
				}

				animData.chainNodeData[id] = chainNode;
			}

			if (animDataPresetsData.contains("physicsData")) {
				auto& physicsData = animDataPresetsData["physicsData"];
				animData.physicsData.enabled = physicsData["enabled"].get<bool>();
				animData.physicsData.mass = physicsData["mass"].get<float>();
				if (physicsData.contains("stiffness")) {
					auto& entry = physicsData["stiffness"];
					// If the entry is float
					if (entry.is_number()) {
						animData.physicsData.stiffness = entry.get<float>();
					} else if (entry.is_string()) {
						animData.physicsData.stiffnessExpression = entry.get<std::string>();
					}
				}
				if (physicsData.contains("angularDamping")) {
					auto& entry = physicsData["angularDamping"];
					// If the entry is float
					if (entry.is_number()) {
						animData.physicsData.angularDamping = entry.get<float>();
					} else if (entry.is_string()) {
						animData.physicsData.angularDampingExpression = entry.get<std::string>();
					}
				}
				if (physicsData.contains("linearDrag")) {
					auto& entry = physicsData["linearDrag"];
					// If the entry is float
					if (entry.is_number()) {
						animData.physicsData.linearDrag = entry.get<float>();
					} else if (entry.is_string()) {
						animData.physicsData.linearDragExpression = entry.get<std::string>();
					}
				}
			}

			return animData;
		};

		bool BuildNodeChainForActor(RE::Actor* a_actor, float init_t = 0.0f, bool a_noPhysics = false) {
			auto loadedData = a_actor->loadedData.lock_read();
			auto data3D = reinterpret_cast<RE::BGSFadeNode*>(loadedData->data3D.get());
			if (!data3D) {
				logger::warn("ActorGenitalAnimManager::BuildNodeChainForActor: Actor has no loaded data. Actor {}", utils::make_str(a_actor));
				return false;
			}

			auto genitalHeadpart = ActorHeadpartGenitalManager::GetSingleton().GetActorGenital(a_actor);
			if (!genitalHeadpart) {
				logger::warn("ActorGenitalAnimManager::BuildNodeChainForActor: No genital headpart found for Actor {}", utils::make_str(a_actor));
				return false;
			}

			_Genital_Anim_Data_Map_T::const_accessor accessor;
			if (m_genitalAnimData.find(accessor, genitalHeadpart)) {
				auto& genitalAnimData = accessor->second;

				std::unique_ptr<NodeChainLerpGenerator> generator = std::make_unique<NodeChainLerpGenerator>();
				if (!generator->build(data3D, genitalAnimData.rootNodeName, genitalAnimData.chainNodeData, genitalAnimData.physicsData, a_noPhysics)) {
					logger::warn("ActorGenitalAnimManager::BuildNodeChainForActor: Failed to build node chain for Actor {}", utils::make_str(a_actor));
					return false;
				} else {
					logger::info("ActorGenitalAnimManager::BuildNodeChainForActor: Built node chain for Actor {}, init_t: {}", utils::make_str(a_actor), init_t);
				}

				generator->fullErectionTimeMs = genitalAnimData.fullErectionTimeMs;

				if (init_t > 0.f) {
					generator->setInterpolatedTargetsDelayed(init_t, 0);
				}


				_Generator_Map_T::accessor m_actorGenitalAnimGenerators_acc;

				m_actorGenitalAnimGenerators.insert(m_actorGenitalAnimGenerators_acc, a_actor->formID);

				m_actorGenitalAnimGenerators_acc->second = std::move(generator);

				m_actorValueEvaluatedList.insert({ a_actor->formID, 0 });
				
				return true;
			}else {
				logger::warn("ActorGenitalAnimManager::BuildNodeChainForActor: No genital anim data found for Actor {}", utils::make_str(a_actor));
				return false;
			}
		}

		bool UnloadNodeChainForActor(RE::Actor* a_actor) {
			_Generator_Map_T::accessor accessor;
			if (m_actorGenitalAnimGenerators.find(accessor, a_actor->formID)) {
				m_actorGenitalAnimGenerators.erase(accessor);
				logger::info("ActorGenitalAnimManager::UnloadNodeChainForActor: Unloaded genital node chain for Actor {}", utils::make_str(a_actor));
				return true;
			}
			return false;
		}

		float GetArousalLevel(RE::Actor* a_actor) {
			if (m_arousalActorValue) {
				return a_actor->GetActorValue(*m_arousalActorValue);
			}
			return -1.0f;
		}
		
		ActorGenitalAnimManager(){};
	};
}