#pragma once
#include "Exceptions.h"
#include "ModelDB.h"

namespace utils
{
	namespace traits
	{
		template <class _FORM_T>
		concept _is_form = std::derived_from<_FORM_T, RE::TESForm>;
	}

	std::string GetPluginFolder();

	std::string GetAddonsFolder();

	std::string GetAddonFilePath(std::string addonFileName);

	YAML::Node GetAddonFile(std::string addonFileName);

	inline constexpr std::string_view GetPluginName()
	{
		return Plugin::NAME;
	};

	std::string GetPluginIniFile();

	std::string_view GetPluginLogFile();

	std::string GetCurrentTimeString(std::string fmt = "%d.%m.%Y %H:%M:%S");

	RE::Actor* GetSelActorOrPlayer();

	inline const std::string& GetSexString(RE::SEX a_sex) {
		static const std::string male("Male");
		static const std::string female("Female");
		static const std::string unknown("Unknown");

		switch (a_sex) {
		case RE::SEX::kFemale:
			return female;
		case RE::SEX::kMale:
			return male;
		default:
			return unknown;
		}
	}

	inline bool IsActorMenuActor(RE::Actor* a_actor)
	{
		return a_actor->boolFlags.underlying() == 4 && a_actor->boolFlags2.underlying() == 8458272;  // From experience
	}

	template <class _FORM_T>
		requires traits::_is_form<_FORM_T>
	inline std::string make_str(_FORM_T* a_form)
	{
		return std::format("'{}'({:X})", a_form->GetFormEditorID(), a_form->GetFormID());
	}

	template <>
	inline std::string make_str<RE::Actor>(RE::Actor* a_form)
	{
		auto npc = a_form->GetNPC();
		if (npc) {
			return std::format("NPC:'{}'({:X}, npc: {:X})", npc->GetFormEditorID(), a_form->GetFormID(), npc->GetFormID());
		} else {
			return std::format("'{}'({:X})", a_form->GetDisplayFullName(), a_form->GetFormID());  // Could be potentially crashy
		}
	}

	bool caseInsensitiveCompare(const std::string& str, const char* cstr);

	float GetActorValue(RE::Actor* a_actor, const std::string& a_avName, bool a_log = false);

	bool ShouldActorShowSpacesuit(RE::Actor* a_actor);

	enum class ARMOTypeSpacesuit : std::uint32_t
	{
		SpacesuitBackpack,
		SpacesuitBody,
		SpacesuitHelmet,
		NotSpacesuit
	};

	inline std::vector<bool> KeywordFormHasKeywords(RE::BGSKeywordForm* a_kwForm, const std::vector<RE::BGSKeyword*>& a_kw_list)
	{
		std::vector<bool> found(a_kw_list.size(), false);
		if (!a_kwForm || a_kw_list.empty()) {
			return found;
		}

		a_kwForm->ForEachKeyword([&found, &a_kw_list](const RE::BGSKeyword* a_keyword) {
			for (std::size_t i = 0; i < a_kw_list.size(); i++) {
				if (a_kw_list[i] == a_keyword) {
					found[i] = true;
					break;
				}
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		return found;
	};

	inline bool KeywordFormHasAnyKeywords(RE::BGSKeywordForm* a_kwForm, const std::vector<RE::BGSKeyword*>& a_kw_list)
	{
		bool result{ false };
		if (!a_kwForm || a_kw_list.empty()) {
			return result;
		}

		a_kwForm->ForEachKeyword([&result, &a_kw_list](const RE::BGSKeyword* a_keyword) {
			if (result = std::ranges::find(a_kw_list, a_keyword) != a_kw_list.end(); result) {
				return RE::BSContainer::ForEachResult::kStop;
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		return result;
	};

	inline bool KeywordFormHasKeyword(RE::BGSKeywordForm* a_kwForm, RE::BGSKeyword* a_kw)
	{
		bool result{ false };
		if (!a_kw || !a_kwForm) {
			return result;
		}

		a_kwForm->ForEachKeyword([&result, &a_kw](const RE::BGSKeyword* a_keyword) {
			if (result = a_keyword == a_kw; result) {
				return RE::BSContainer::ForEachResult::kStop;
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		return result;
	};

	inline std::vector<bool> ARMOHasKeywords(RE::TESObjectARMO* a_armo, const RE::TESObjectARMOInstanceData* a_instanceData, std::vector<RE::BGSKeyword*>& a_kw_list) {
		auto result1 = KeywordFormHasKeywords(a_armo, a_kw_list);
		auto result2 = KeywordFormHasKeywords(a_instanceData->keywords, a_kw_list);
		for (std::size_t i = 0; i < result1.size(); i++) {
			result1[i] = result1[i] || result2[i];
		}

		return result1;
	};

	inline bool ARMOHasAnyKeywords(RE::TESObjectARMO* a_armo, const RE::TESObjectARMOInstanceData* a_instanceData, std::vector<RE::BGSKeyword*>& a_kw_list)
	{
		bool result1 = KeywordFormHasAnyKeywords(a_armo, a_kw_list);
		bool result2 = KeywordFormHasAnyKeywords(a_instanceData->keywords, a_kw_list);
		return result1 || result2;
	};

	inline bool ARMOHasKeyword(RE::TESObjectARMO* a_armo, const RE::TESObjectARMOInstanceData* a_instanceData, RE::BGSKeyword* a_kw_list){
		return KeywordFormHasKeyword(a_armo, a_kw_list) || KeywordFormHasKeyword(a_instanceData->keywords, a_kw_list);
	};

	inline ARMOTypeSpacesuit GetARMOSpacesuitType(RE::TESObjectARMO* a_armo, const RE::TESObjectARMOInstanceData* a_instanceData)
	{
		static auto                         ArmorTypeSpacesuitBackpack_kw = RE::TESForm::LookupByID<RE::BGSKeyword>(0x0023C7BF);
		static auto                         ArmorTypeSpacesuitBody_kw = RE::TESForm::LookupByID<RE::BGSKeyword>(0x0023C7C0);
		static auto                         ArmorTypeSpacesuitHelmet_kw = RE::TESForm::LookupByID<RE::BGSKeyword>(0x0023C7C1);
		static std::vector<RE::BGSKeyword*> spacesuit_kw_list = { ArmorTypeSpacesuitBackpack_kw, ArmorTypeSpacesuitBody_kw, ArmorTypeSpacesuitHelmet_kw };

		auto found = ARMOHasKeywords(a_armo, a_instanceData, spacesuit_kw_list);
		if (found[0]) {
			return ARMOTypeSpacesuit::SpacesuitBackpack;
		} else if (found[1]) {
			return ARMOTypeSpacesuit::SpacesuitBody;
		} else if (found[2]) {
			return ARMOTypeSpacesuit::SpacesuitHelmet;
		} else {
			return ARMOTypeSpacesuit::NotSpacesuit;
		}
	}

	inline bool IsARMOSpacesuit(RE::TESObjectARMO* a_armo, const RE::TESObjectARMOInstanceData* a_instanceData)
	{
		static auto                         ArmorTypeSpacesuitBackpack_kw = RE::TESForm::LookupByID<RE::BGSKeyword>(0x0023C7BF);
		static auto                         ArmorTypeSpacesuitBody_kw = RE::TESForm::LookupByID<RE::BGSKeyword>(0x0023C7C0);
		static auto                         ArmorTypeSpacesuitHelmet_kw = RE::TESForm::LookupByID<RE::BGSKeyword>(0x0023C7C1);
		static std::vector<RE::BGSKeyword*> spacesuit_kw_list = { ArmorTypeSpacesuitBackpack_kw, ArmorTypeSpacesuitBody_kw, ArmorTypeSpacesuitHelmet_kw };
		return ARMOHasAnyKeywords(a_armo, a_instanceData, spacesuit_kw_list);
	}

	std::uint32_t GetARMOModelOccupiedSlots(RE::TESObjectARMO* a_armo);

	RE::BGSFadeNode* GetModel(const char* a_modelName);

	RE::BGSFadeNode* GetActorBaseSkeleton(RE::Actor* a_actor);

	template<class _Func_T>
	class scope_guard
	{
	public:
		scope_guard(_Func_T a_func) :
			_func(a_func)
		{}

		scope_guard(const scope_guard&) = delete;

		scope_guard(scope_guard&& other) {
			_func = std::move(other._func);
			other._shouldRelease = false;
		}

		~scope_guard()
		{
			if (_shouldRelease) {
				_func();
			}
		}

		scope_guard& operator=(const scope_guard&) = delete;

		scope_guard& operator=(scope_guard&& other) {
			if (this != &other) {
				_func = std::move(other._func);
				other._shouldRelease = false;
			}
			return *this;
		}

		void dismiss() noexcept
		{
			_func = nullptr;
		}

	private:
		_Func_T _func;
		bool _shouldRelease{ true };
	};

	template <class _InitFunc_T, class _ReleaseFunt_T>
	class scope_guard_init_release
	{
	public:
		scope_guard_init_release(_InitFunc_T a_initFunc, _ReleaseFunt_T a_releaseFunc) :
			_initFunc(a_initFunc),
			_releaseFunc(a_releaseFunc)
		{
			if (!_initFunc || !_releaseFunc) {
				throw std::invalid_argument("Invalid init or release function");
			}
			_initFunc();
		}

		scope_guard_init_release(const scope_guard_init_release&) = delete;

		scope_guard_init_release(scope_guard_init_release&& other) noexcept
		{
			_initFunc = std::move(other._initFunc);
			_releaseFunc = std::move(other._releaseFunc);
			other._shouldRelease = false;
		}

		~scope_guard_init_release()
		{
			if (_shouldRelease) {
				_releaseFunc();
			}
		}

		scope_guard_init_release& operator=(const scope_guard_init_release&) = delete;

		scope_guard_init_release& operator=(scope_guard_init_release&& other)
		{
			if (this != &other) {
				_initFunc = std::move(other._initFunc);
				_releaseFunc = std::move(other._releaseFunc);
				_shouldRelease = false;
			}
			return *this;
		}

		void dismiss() noexcept
		{
			_initFunc = nullptr;
			_releaseFunc = nullptr;
		}

	private:
		_InitFunc_T _initFunc;
		_ReleaseFunt_T _releaseFunc;
		bool _shouldRelease{ true };
	};

	template <class _Instance_T, class _Ret_T, class... _Args_T>
	class MemberFuncReturnDecorator
	{
	public:
		using _MemberFunc_T = _Ret_T (_Instance_T::*)(_Args_T...);
		using _DecoratorFunc_T = std::function<_Ret_T(_Ret_T)>;

		MemberFuncReturnDecorator(_Instance_T* a_instance, _MemberFunc_T a_memberFunc, _DecoratorFunc_T a_decorator) :
			_instance(a_instance),
			_memberFunc(a_memberFunc),
			_decorator(a_decorator)
		{
			if (!_instance || !_memberFunc || !_decorator) {
				throw std::invalid_argument("Invalid instance, member function, or decorator");
			}
		}

		_Ret_T operator()(_Args_T... args)
		{
			if constexpr (std::is_void_v<_Ret_T>) {
				(_instance->*_memberFunc)(std::forward<_Args_T>(args)...);
				_decorator();
			} else {
				return _decorator((_instance->*_memberFunc)(std::forward<_Args_T>(args)...));
			}
		}

	private:
		_Instance_T*     _instance;
		_MemberFunc_T    _memberFunc;
		_DecoratorFunc_T _decorator;
	};
}