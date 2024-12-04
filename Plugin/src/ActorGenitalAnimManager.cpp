#include "ActorGenitalAnimManager.h"

void daf::ActorGenitalAnimManager::OnEvent(const events::ActorUpdateEvent& a_event, events::EventDispatcher<events::ActorUpdateEvent>* a_dispatcher)
{
	auto actor = a_event.actor;
	auto npc = actor->GetNPC();
	auto when = a_event.when();

	_Generator_Map_T::accessor accessor;
	if (!m_actorGenitalAnimGenerators.find(accessor, actor->formID)) {
		return;
	}
	auto& generator = accessor->second;
	generator->updateWithSystemTime(when, GenitalAnimGeneratorMaxDeltaTime);

	_ActorValue_Evaluated_List_T::accessor actorValueEvaluatedAccessor;
	if (!m_actorValueEvaluatedList.find(actorValueEvaluatedAccessor, actor->formID)) {
		return;
	}

	if (when - actorValueEvaluatedAccessor->second < ArousalActorValueEvaluationCooldown) {
		return;
	}
	auto arousalLevel = GetArousalLevel(actor);
	generator->setAtT_Propotional(arousalLevel / 100.f);
	actorValueEvaluatedAccessor->second = when;
}

void daf::ActorGenitalAnimManager::OnEvent(const events::ActorFirstUpdateEvent& a_event, events::EventDispatcher<events::ActorFirstUpdateEvent>* a_dispatcher)
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

void daf::ActorGenitalAnimManager::OnEvent(const events::ActorLoadedEvent& a_event, events::EventDispatcher<events::ActorLoadedEvent>* a_dispatcher)
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

void daf::ActorGenitalAnimManager::OnEvent(const events::ActorGenitalChangedEvent& a_event, events::EventDispatcher<events::ActorGenitalChangedEvent>* a_dispatcher)
{
	auto actor = a_event.actor;
	auto npc = actor->GetNPC();

	if (BuildNodeChainForActor(actor, GetArousalLevel(actor) / 100.f)) {
		m_actorsPendingAttachGenerator.erase(actor->formID);  // No need to build again
	}
}

void daf::ActorGenitalAnimManager::OnEvent(const events::ActorRevealingStateChangedEvent& a_event, events::EventDispatcher<events::ActorRevealingStateChangedEvent>* a_dispatcher)
{
	auto actor = a_event.actor;
	auto npc = actor->GetNPC();
	bool isPreviouslyHiding = a_event.oldState;
	bool isNowHiding = a_event.newState;

	_Generator_Map_T::accessor accessor;
	if (m_actorGenitalAnimGenerators.find(accessor, actor->formID)) {
		auto& generator = accessor->second;
		if (isNowHiding) {
			generator->freezeAtT_Delayed(0.0f, 0);  // Freeze at Minima immediately
		} else {
			generator->unfreeze_Propotional(1.0f);
		}
	}
}

void daf::ActorGenitalAnimManager::OnEvent(const events::GenitalDataLoadingEvent& a_event, events::EventDispatcher<events::GenitalDataLoadingEvent>* a_dispatcher)
{
	auto  headPart = a_event.genitalHeadpart;
	auto& genitalData = a_event.genitalData;
	auto& configData = a_event.configData;

	try {
		auto animData = LoadAnimData(genitalData, configData);
		m_genitalAnimData.insert({ headPart, animData });
	} catch (const std::exception& e) {
		logger::error("ActorGenitalAnimManager: Failed to load genital anim data for headpart {}. Error: {}", headPart->formEditorID.c_str(), e.what());
		return;
	}
}

void daf::ActorGenitalAnimManager::OnEvent(const events::GenitalDataReloadEvent& a_event, events::EventDispatcher<events::GenitalDataReloadEvent>* a_dispatcher)
{
	m_arousalActorValue = RE::TESObjectREFR::LookupByEditorID<RE::ActorValueInfo>(ArousalActorValueEditorID);
	if (!m_arousalActorValue) {
		logger::warn("ActorGenitalAnimManager: Arousal actor value not found. Arousal actor value editor ID: {}", ArousalActorValueEditorID);
	}
	Clear();
}

daf::ActorGenitalAnimManager::GenitalAnimData daf::ActorGenitalAnimManager::LoadAnimData(const nlohmann::json& genitalData, const nlohmann::json& configData)
{
	GenitalAnimData animData;

	if (!genitalData.contains("animDataPreset")) {
		throw std::runtime_error("animDataPreset not found in genital data");
	}

	auto  animDataPreset = genitalData["animDataPreset"].get<std::string>();
	auto& animDataPresetsData = configData["$animDataPresets"][animDataPreset];

	animData.rootNodeName = animDataPresetsData["rootNodeName"].get<std::string>();
	animData.fullErectionTimeMs = time_t(animDataPresetsData["fullErectionTimeSec"].get<float>() * 1000);

	auto&  chainNodeData = animDataPresetsData["chainNodeData"];
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
		if (animData.physicsData.mass < 0.01f) {
			animData.physicsData.mass = 0.01f;
			logger::warn("ActorGenitalAnimManager: Mass value too low. Set to 0.01");
		}
		if (physicsData.contains("stiffness")) {
			auto& entry = physicsData["stiffness"];
			// If the entry is float
			if (entry.is_number()) {
				animData.physicsData.stiffness = entry.get<float>();
				if (animData.physicsData.stiffness < 0.1f) {
					animData.physicsData.stiffness = 0.1f;
					logger::warn("ActorGenitalAnimManager: Stiffness value too low. Set to 0.1");
				}
			} else if (entry.is_string()) {
				animData.physicsData.stiffnessExpression = entry.get<std::string>();
			}
		}
		if (physicsData.contains("angularDamping")) {
			auto& entry = physicsData["angularDamping"];
			// If the entry is float
			if (entry.is_number()) {
				animData.physicsData.angularDamping = entry.get<float>();
				if (animData.physicsData.angularDamping < 0.1f) {
					animData.physicsData.angularDamping = 0.1f;
					logger::warn("ActorGenitalAnimManager: AngularDamping value too low. Set to 0.1");
				}
			} else if (entry.is_string()) {
				animData.physicsData.angularDampingExpression = entry.get<std::string>();
			}
		}
		if (physicsData.contains("linearDrag")) {
			auto& entry = physicsData["linearDrag"];
			// If the entry is float
			if (entry.is_number()) {
				animData.physicsData.linearDrag = entry.get<float>();
				if (animData.physicsData.linearDrag < 0.f) {
					animData.physicsData.linearDrag = 0.f;
					logger::warn("ActorGenitalAnimManager: LinearDrag value too low. Set to 0");
				}
			} else if (entry.is_string()) {
				animData.physicsData.linearDragExpression = entry.get<std::string>();
			}
		}
	}

	return animData;
}

bool daf::ActorGenitalAnimManager::BuildNodeChainForActor(RE::Actor* a_actor, float init_t, bool a_noPhysics)
{
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
			generator->setAtT_Delayed(init_t, 0);
		}

		_Generator_Map_T::accessor m_actorGenitalAnimGenerators_acc;

		m_actorGenitalAnimGenerators.insert(m_actorGenitalAnimGenerators_acc, a_actor->formID);

		m_actorGenitalAnimGenerators_acc->second = std::move(generator);

		m_actorValueEvaluatedList.insert({ a_actor->formID, 0 });

		return true;
	} else {
		logger::warn("ActorGenitalAnimManager::BuildNodeChainForActor: No genital anim data found for Actor {}", utils::make_str(a_actor));
		return false;
	}
}

bool daf::ActorGenitalAnimManager::UnloadNodeChainForActor(RE::Actor* a_actor)
{
	_Generator_Map_T::accessor accessor;
	if (m_actorGenitalAnimGenerators.find(accessor, a_actor->formID)) {
		m_actorGenitalAnimGenerators.erase(accessor);
		logger::info("ActorGenitalAnimManager::UnloadNodeChainForActor: Unloaded genital node chain for Actor {}", utils::make_str(a_actor));
		return true;
	}
	return false;
}
