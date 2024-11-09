#pragma once
#include "LogWrapper.h"
#include "RE/E/Events.h"

#include "EventDispatcher.h"
#include "HookManager.h"

namespace events
{
	class ArmorOrApparelEquippedEvent: public EventBase
	{
	public:
		enum class EquipType
		{
			kEquip,
			kEquip2,
			kUnequip
		};

		ArmorOrApparelEquippedEvent(RE::Actor* a_actor, RE::TESObjectARMO* a_armorOrApparel, EquipType a_equipType) :
			actor(a_actor),
			armorOrApparel(a_armorOrApparel),
			equipType(a_equipType)
		{}

		RE::Actor*         actor;
		RE::TESObjectARMO* armorOrApparel;
		EquipType          equipType;
	};

	class ActorLoadedEvent: public EventBase
	{
	public:
		ActorLoadedEvent(RE::Actor* a_actor, bool a_loaded) :
			actor(a_actor),
			loaded(a_loaded)
		{}

		RE::Actor* actor;
		bool       loaded;
	};

	class ActorReferenceSet3dEvent: public EventBase
	{
	public:
		ActorReferenceSet3dEvent(RE::Actor* a_actor) :
			actor(a_actor)
		{}

		RE::Actor* actor;
	};

	class ActorInitializedEvent: public EventBase
	{
	public:
		ActorInitializedEvent(RE::Actor* a_actor, bool a_unk0, bool a_unk1) :
			actor(a_actor),
			unk0(a_unk0),
			unk1(a_unk1)
		{}

		RE::Actor* actor;
		bool       unk0;
		bool       unk1;
	};

	class ActorUpdateEvent : public EventBase
	{
	public:
		ActorUpdateEvent(RE::Actor* a_actor, float a_deltaTime) :
			actor(a_actor),
			deltaTime(a_deltaTime),
			lastUpdateTime(std::chrono::steady_clock::now())
		{}

		RE::Actor*								actor;
		float									deltaTime;
		std::chrono::steady_clock::time_point	lastUpdateTime;

		time_t when() const
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(lastUpdateTime.time_since_epoch()).count();
		}
	};

	class ActorFirstUpdateEvent : public ActorUpdateEvent 
	{
	public:
		ActorFirstUpdateEvent(RE::Actor* a_actor, float a_deltaTime) :
			ActorUpdateEvent(a_actor, a_deltaTime)
		{}
	};

	class GameLoadedEvent : public EventBase
	{
	public:
		GameLoadedEvent(SFSE::MessagingInterface::MessageType a_messageType) :
			messageType(a_messageType)
		{}

		SFSE::MessagingInterface::MessageType messageType;
	};

	class ArmorOrApparelEquippedEventDispatcher :
		public RE::BSTEventSink<RE::TESEquipEvent>,
		public RE::BSTEventSink<RE::ActorItemEquipped::Event>,
		public EventDispatcher<ArmorOrApparelEquippedEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;

		static ArmorOrApparelEquippedEventDispatcher* GetSingleton()
		{
			static ArmorOrApparelEquippedEventDispatcher singleton;
			return &singleton;
		}

		EventResult ProcessEvent(const RE::TESEquipEvent& a_event, RE::BSTEventSource<RE::TESEquipEvent>* a_eventSource) override
		{
			//logger::c_info("TESEquipEvent: Item actor_who {}({:X}), baseObject {:X}, origRef {:X} {}", a_event.actor->GetDisplayFullName(), a_event.actor->GetFormID(), a_event.baseObject, a_event.origRef, a_event.equipped ? "Equipped" : "Unequipped");

			auto actor = a_event.actor.get();
			auto object_form = RE::TESObjectREFR::LookupByID(a_event.baseObject);

			if (auto armo_form = object_form->As<RE::TESObjectARMO>(); armo_form != nullptr && actor != nullptr) {
				//logger::c_info("Item actor {}, armo {}, {}", utils::make_str(actor), utils::make_str(armo_form), a_event.equipped ? "Equipped" : "Unequipped");

				auto equip_type = a_event.equipped ? ArmorOrApparelEquippedEvent::EquipType::kEquip : ArmorOrApparelEquippedEvent::EquipType::kUnequip;

				this->Dispatch({ actor, armo_form, equip_type });
			}

			return EventResult::kContinue;
		}

		EventResult ProcessEvent(const RE::ActorItemEquipped::Event& a_event, RE::BSTEventSource<RE::ActorItemEquipped::Event>* a_eventSource) override
		{
			//logger::c_info("ActorItemEquipped::Event: Item actor_who {}({:X}), item {}", a_event.actor->GetDisplayFullName(), a_event.actor->GetFormID(), a_event.item->GetFormID());

			auto actor = a_event.actor.get();
			auto object_form = a_event.item;
			if (auto armo_form = object_form->As<RE::TESObjectARMO>(); armo_form != nullptr && actor != nullptr) {
				this->Dispatch({ actor, armo_form, ArmorOrApparelEquippedEvent::EquipType::kEquip2 });
			}
			return EventResult::kContinue;
		}

		void Register()
		{
			RE::TESEquipEvent::GetEventSource()->RegisterSink(this);
			RE::ActorItemEquipped::Event::GetEventSource()->RegisterSink(this);
		}
	};

	class ActorLoadedEventDispatcher :
		public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
		public EventDispatcher<ActorLoadedEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
		using Event = RE::TESObjectLoadedEvent;

		static ActorLoadedEventDispatcher* GetSingleton()
		{
			static ActorLoadedEventDispatcher singleton;
			return &singleton;
		}

		EventResult ProcessEvent(const Event& a_event, RE::BSTEventSource<Event>* a_eventSource) override
		{
			//logger::c_info("Object {} formID {:X}", a_event.loaded ? "Loaded" : "Unloaded", a_event.formID);

			auto object_form = RE::TESObjectREFR::LookupByID(a_event.formID);

			if (auto actor_form = object_form->As<RE::Actor>(); actor_form != nullptr) {
				//logger::c_info("Actor {} {}", utils::make_str(actor_form), a_event.loaded ? "Loaded" : "Unloaded");

				this->Dispatch({ actor_form, a_event.loaded });
			}

			return EventResult::kContinue;
		}

		void Register()
		{
			Event::GetEventSource()->RegisterSink(this);
		}
	};
	
	class ActorReferenceSet3dEventDispatcher :
		public RE::BSTEventSink<RE::RuntimeComponentDBFactory::ReferenceSet3d>,
		public EventDispatcher<ActorReferenceSet3dEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
		using Event = RE::RuntimeComponentDBFactory::ReferenceSet3d;

		static ActorReferenceSet3dEventDispatcher* GetSingleton()
		{
			static ActorReferenceSet3dEventDispatcher singleton;
			return &singleton;
		}

		EventResult ProcessEvent(const Event& a_event, RE::BSTEventSource<Event>* a_eventSource) override
		{
			//logger::c_info("ActorReferenceSet3dEvent");

			auto object_refr = a_event.ref.get();

			if (auto actor = object_refr->As<RE::Actor>(); actor) {
				//logger::c_info("ActorReferenceSet3dEvent actor {}", utils::make_str(actor));

				this->Dispatch({ actor });
			}

			return EventResult::kContinue;
		}

		void Register()
		{
			Event::GetEventSource()->RegisterSink(this);
		}
	
	};

	class GameDataLoadedEventDispatcher :
		public EventDispatcher<GameLoadedEvent>
	{
	public:
		static GameDataLoadedEventDispatcher* GetSingleton()
		{
			static GameDataLoadedEventDispatcher singleton;
			return &singleton;
		}
	};

	class ActorInitializedEventDispatcher :
		public EventDispatcher<ActorInitializedEvent>
	{
	public:
		static ActorInitializedEventDispatcher* GetSingleton()
		{
			static ActorInitializedEventDispatcher singleton;
			return &singleton;
		}
	
	};

	class ActorUpdatedEventDispatcher :
		public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
		public hooks::ActorUpdateFuncHook::Listener,
		public EventDispatcher<ActorUpdateEvent>,
		public EventDispatcher<ActorFirstUpdateEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
		using Event = RE::TESObjectLoadedEvent;

		static ActorUpdatedEventDispatcher* GetSingleton()
		{
			static ActorUpdatedEventDispatcher singleton;
			return &singleton;
		}

		EventResult ProcessEvent(const Event& a_event, RE::BSTEventSource<Event>* a_eventSource) override
		{
			auto object_form = RE::TESObjectREFR::LookupByID(a_event.formID);
			if (auto actor_form = object_form->As<RE::Actor>(); actor_form != nullptr) {
				if (a_event.loaded) {
					this->WatchInstance(actor_form);
					
					if (this->NumWatching() > 512) {
						logger::c_warn("ActorLoadedEventDispatcher::ProcessEvent(): watch list is too large! (instance count > 512)");
					}
				}
				else {
					this->UnwatchInstance(actor_form);
				}
			}

			return EventResult::kContinue;
		}

		void OnEvent(const event_type& a_vfunc_event, dispatcher_type* a_vfunc_dispatcher) override 
		{
			auto actor = a_vfunc_event.GetArg<0>();
			if (!_get(actor)) {
				_insert_or_allocate(actor, true);
				this->EventDispatcher<ActorFirstUpdateEvent>::Dispatch({ actor, a_vfunc_event.GetArg<1>() });
			}
			this->EventDispatcher<ActorUpdateEvent>::Dispatch({ actor, a_vfunc_event.GetArg<1>() });
		}

		void Register()
		{
			Event::GetEventSource()->RegisterSink(this);
			hooks::ActorUpdateFuncHook::GetSingleton()->AddStaticListener(this);
		}

		size_t NumWatching()
		{
			return m_actor_updated.size();
		}

	protected:
		void WatchInstance(RE::Actor* a_actor)
		{
			m_actor_updated.insert({ a_actor, false });
		}

		void UnwatchInstance(RE::Actor* a_actor)
		{
			m_actor_updated.erase(a_actor);
		}

		bool _find(RE::Actor* a_actor) // Don't do anything if the element doesn't exist
		{
			tbb::concurrent_hash_map<RE::Actor*, bool>::const_accessor acc;
			return m_actor_updated.find(acc, a_actor);
		}

		bool _get(RE::Actor* a_actor) 
		{
			tbb::concurrent_hash_map<RE::Actor*, bool>::const_accessor acc;
			if (m_actor_updated.find(acc, a_actor)) {
				return acc->second;
			}
			return false;
		}

		void _try_update(RE::Actor* a_actor, bool a_updated) // Don't do anything if the element doesn't exist
		{
			tbb::concurrent_hash_map<RE::Actor*, bool>::accessor acc;
			if (m_actor_updated.find(acc, a_actor)) {
				acc->second = a_updated;
			}
		}

		void _insert_or_allocate(RE::Actor* a_actor, bool a_updated) // Insert if not exist, otherwise update
		{
			tbb::concurrent_hash_map<RE::Actor*, bool>::accessor acc;
			m_actor_updated.insert(acc, a_actor);
			acc->second = a_updated;
		}

		tbb::concurrent_hash_map<RE::Actor*, bool> m_actor_updated;
	};

	inline void RegisterHandlers()
	{
		ArmorOrApparelEquippedEventDispatcher::GetSingleton()->Register();
		ActorLoadedEventDispatcher::GetSingleton()->Register();
		ActorReferenceSet3dEventDispatcher::GetSingleton()->Register();
		//GameDataLoadedEventDispatcher::GetSingleton()->Register();
		//ActorInitializedEventDispatcher::GetSingleton()->Register();
		ActorUpdatedEventDispatcher::GetSingleton()->Register();
	}
}