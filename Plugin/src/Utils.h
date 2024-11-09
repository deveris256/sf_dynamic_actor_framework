#pragma once
#include <yaml-cpp/yaml.h>
#include "Exceptions.h"

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
			return std::format("NPC:'{}'({:X})", npc->GetFormEditorID(), a_form->GetFormID());
		} else {
			return std::format("'{}'({:X})", a_form->GetDisplayFullName(), a_form->GetFormID());  // Could be potentially crashy
		}
	}

	bool caseInsensitiveCompare(const std::string& str, const char* cstr);
}