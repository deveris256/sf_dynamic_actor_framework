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
	}

	using ActorUpdateFuncHook = events::HookFuncCalledEventDispatcher<void, RE::Actor*, float>;
	extern ActorUpdateFuncHook const* g_actorUpdateFuncHook;

	inline void InstallHooks()
	{
		ActorUpdateFuncHook::GetSingleton()->Install((uintptr_t)addrs::ActorUpdate_Func.address());
	}

}