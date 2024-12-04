#pragma once
#include "ActorHeadpartGenitalManager.h"
#include "NodeChainLerpGenerator.h"
#include "SingletonBase.h"

namespace daf
{
	inline constexpr time_t           ArousalActorValueEvaluationCooldown{ 500 };
	inline constexpr std::string_view ArousalActorValueEditorID{ "DAFGen_ArousalLevel" };

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

		void OnEvent(const events::ActorUpdateEvent& a_event, events::EventDispatcher<events::ActorUpdateEvent>* a_dispatcher) override;
		
		void OnEvent(const events::ActorFirstUpdateEvent& a_event, events::EventDispatcher<events::ActorFirstUpdateEvent>* a_dispatcher) override;

		void OnEvent(const events::ActorLoadedEvent& a_event, events::EventDispatcher<events::ActorLoadedEvent>* a_dispatcher) override;

		void OnEvent(const events::ActorGenitalChangedEvent& a_event, events::EventDispatcher<events::ActorGenitalChangedEvent>* a_dispatcher) override;

		void OnEvent(const events::ActorRevealingStateChangedEvent& a_event, events::EventDispatcher<events::ActorRevealingStateChangedEvent>* a_dispatcher) override;

		void OnEvent(const events::GenitalDataLoadingEvent& a_event, events::EventDispatcher<events::GenitalDataLoadingEvent>* a_dispatcher) override;

		void OnEvent(const events::GenitalDataReloadEvent& a_event, events::EventDispatcher<events::GenitalDataReloadEvent>* a_dispatcher) override;

		inline void Clear()
		{
			m_actorsPendingAttachGenerator.clear();
			m_actorValueEvaluatedList.clear();
			m_actorGenitalAnimGenerators.clear();
			m_genitalAnimData.clear();
		}

		inline void Register()
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
		*				"stiffness": float or string, // If string (for instance, "200 + 3400 * clamp(-1.2, 3*t - 2, 1.0)^4". Using exprtk), it's an expression evaluated at runtime against 't' (identifying where the animation is from Minima 0 to Maxima 1)
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
		GenitalAnimData LoadAnimData(const nlohmann::json& genitalData, const nlohmann::json& configData);

		bool BuildNodeChainForActor(RE::Actor* a_actor, float init_t = 0.0f, bool a_noPhysics = false);

		bool UnloadNodeChainForActor(RE::Actor* a_actor);

		inline float GetArousalLevel(RE::Actor* a_actor) {
			if (m_arousalActorValue) {
				return a_actor->GetActorValue(*m_arousalActorValue);
			}
			return -1.0f;
		}
		
		ActorGenitalAnimManager(){};
	};
}