#pragma once
#include "EventDispatcher.h"

namespace events
{
	template<typename ..._Args>
	class HookFuncCalledEvent : public EventBase
	{
	public:
		HookFuncCalledEvent(_Args... args) : 
			args(args...)
		{}

		template <size_t arg_index, typename _Arg_T = std::tuple_element_t<arg_index, std::tuple<_Args...>>>
		_Arg_T GetArg() const
		{
			return std::get<arg_index>(args);
		}

		std::tuple<_Args...> args;
	};

	template<typename _Rtn_T, typename ..._Args>
	class HookFuncCalledEventDispatcher : 
		public events::EventDispatcher<events::HookFuncCalledEvent<_Args...>>
	{
	public:
		using _Func_T = _Rtn_T(*)(_Args...);

		static HookFuncCalledEventDispatcher* GetSingleton()
		{
			static HookFuncCalledEventDispatcher singleton;
			return &singleton;
		}

		static _Rtn_T DetourFunc(_Args... args)
		{
			auto dispatcher = HookFuncCalledEventDispatcher::GetSingleton();
			dispatcher->Dispatch({ args... });
			return ((_Func_T)dispatcher->m_originalFunc)(args...);
		}

		void Install(uintptr_t a_targetAddr)
		{
			if (IsHooked()) {
				return;
			}
			m_targetAddr = (void*)a_targetAddr;
			m_originalFunc = (void*)a_targetAddr;
			m_detourFunc = DetourFunc;

			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());

			DetourAttach(&(PVOID&)m_originalFunc, m_detourFunc);

			DetourTransactionCommit();
		}

		void Uninstall()
		{
			if (!IsHooked()) {
				return;
			}

			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());

			DetourDetach(&(PVOID&)m_originalFunc, m_detourFunc);

			DetourTransactionCommit();
		}

		virtual ~HookFuncCalledEventDispatcher()
		{
			Uninstall();
		}

		bool IsHooked() const
		{
			return m_originalFunc != m_targetAddr;
		}
		
		void*   m_originalFunc{ nullptr };
		void*   m_targetAddr{ nullptr };
		_Func_T m_detourFunc{ nullptr };
	};
}

namespace hooks
{
	namespace addrs
	{
		inline constexpr REL::ID MAYBE_ActorInitializer_Func{ 150902 };
		inline constexpr REL::ID MAYBE_FormPostInitialized_Func{ 34242 };
		inline constexpr REL::ID MAYBE_ActorEquipAllItems_Func{ 150387 };
		inline constexpr REL::ID ActorUpdate_Func{ 151391 };
		inline constexpr REL::ID TESCondition_EvaluateChain_Func{ 116104 };
	}

	namespace funcs
	{
		inline bool EvaluateConditionChain(RE::BGSConditionForm* a_conditionForm, RE::ConditionCheckParams& a_param)
		{
			using func_t = bool (*)(RE::TESConditionItem*, RE::ConditionCheckParams&);
			static REL::Relocation<func_t> func(addrs::TESCondition_EvaluateChain_Func.address());
			return func(a_conditionForm->conditions.head, a_param);
		}
	}

	using ActorUpdateFuncHook = events::HookFuncCalledEventDispatcher<void, RE::Actor*, float>;
	extern ActorUpdateFuncHook const* g_actorUpdateFuncHook;

	// bool EquipObject(Actor* a_actor, const BGSObjectInstance& a_object, const BGSEquipSlot* a_slot, bool a_queueEquip, bool a_forceEquip, bool a_playSounds, bool a_applyNow, bool a_locked)
	using ActorEquipManagerEquipFuncHook = events::HookFuncCalledEventDispatcher<bool, RE::ActorEquipManager*, RE::Actor*, const RE::BGSObjectInstance&, const RE::BGSEquipSlot*, bool, bool, bool, bool, bool>;
	extern ActorEquipManagerEquipFuncHook const* g_actorEquipManagerEquipFuncHook;

	// bool UnequipObject(Actor* a_actor, const BGSObjectInstance& a_object, const BGSEquipSlot* a_slot, bool a_queueUnequip, bool a_forceUnequip, bool a_playSounds, bool a_applyNow, const BGSEquipSlot* a_slotBeingReplaced)
	using ActorEquipManagerUnequipFuncHook = events::HookFuncCalledEventDispatcher<bool, RE::ActorEquipManager*, RE::Actor*, const RE::BGSObjectInstance&, const RE::BGSEquipSlot*, bool, bool, bool, bool, const RE::BGSEquipSlot*>;
	extern ActorEquipManagerUnequipFuncHook const* g_actorEquipManagerUnequipFuncHook;

	inline void InstallHooks()
	{
		ActorUpdateFuncHook::GetSingleton()->Install((uintptr_t)addrs::ActorUpdate_Func.address());
		ActorEquipManagerEquipFuncHook::GetSingleton()->Install((uintptr_t)RE::ID::ActorEquipManager::EquipObject.address());
		ActorEquipManagerUnequipFuncHook::GetSingleton()->Install((uintptr_t)RE::ID::ActorEquipManager::UnequipObject.address());
	}
}