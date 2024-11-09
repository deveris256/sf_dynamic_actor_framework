#include "Utils.h"

std::string utils::GetPluginFolder()
{
	char    path[MAX_PATH];
	HMODULE hModule = nullptr;  // nullptr == current DLL

	GetModuleFileNameA(hModule, path, MAX_PATH);

	std::string fullPath = path;
	size_t      lastSlash = fullPath.find_last_of("\\/");
	std::string folder = fullPath.substr(0, lastSlash) + "\\Data\\SFSE\\Plugins\\DAF";

	return folder;
}

std::string utils::GetAddonsFolder() {
	std::string addonsFolder = utils::GetPluginFolder() + "\\Addons";
	
	return addonsFolder;
}

std::string utils::GetAddonFilePath(std::string addonFileName) {
	if (addonFileName == "") {
		throw new InvalidAddonFileException;
	}

	std::string addonFileYml = utils::GetAddonsFolder() + addonFileName + ".yml";
	std::string addonFileYaml = utils::GetAddonsFolder() + addonFileName + ".yaml";

	if (std::filesystem::exists(addonFileYml)) {
		return addonFileYml;
	} else if (std::filesystem::exists(addonFileYaml)) {
		return addonFileYaml;
	}

	throw new InvalidAddonFileException;
}

YAML::Node utils::GetAddonFile(std::string addonFileName) {
	
	try {
		std::string addonFilePath = utils::GetAddonFilePath(addonFileName);
		
		YAML::Node  addonFile = YAML::LoadFile(addonFilePath);

		return addonFile;
	}
	catch (InvalidAddonFileException) {
		throw new InvalidAddonFileException;
	}
}

std::string utils::GetPluginIniFile()
{
	std::string iniFile = utils::GetPluginFolder() + std::format("\\{}.ini", GetPluginName());
	return iniFile;
}

std::string_view utils::GetPluginLogFile()
{
	return std::format("{}/{}.log", GetPluginFolder(), GetPluginName()).c_str();
}

std::string utils::GetCurrentTimeString(std::string fmt)
{
	auto now = std::chrono::system_clock::now();

	std::time_t now_time = std::chrono::system_clock::to_time_t(now);

	std::tm local_time = *std::localtime(&now_time);

	std::ostringstream oss;
	oss << std::put_time(&local_time, fmt.c_str());

	return oss.str();
}

//
// Console reference getters
//

RE::NiPointer<RE::TESObjectREFR> GetRefrFromHandle(uint32_t handle)
{
	RE::NiPointer<RE::TESObjectREFR>                                    result;
	REL::Relocation<void(RE::NiPointer<RE::TESObjectREFR>&, uint32_t*)> func(REL::ID(72399));
	func(result, &handle);
	return result;
}

RE::NiPointer<RE::TESObjectREFR> GetConsoleRefr()
{
	REL::Relocation<uint64_t**>                      consoleReferencesManager(REL::ID(879512));
	REL::Relocation<uint32_t*(uint64_t*, uint32_t*)> GetConsoleHandle(REL::ID(166314));
	uint32_t                                         outId = 0;
	GetConsoleHandle(*consoleReferencesManager, &outId);
	return GetRefrFromHandle(outId);
}

RE::Actor* utils::GetSelActorOrPlayer()
{
	auto sel = GetConsoleRefr();

	if (sel != nullptr and sel.get()->IsActor()) {
		return sel->As<RE::Actor>();
	}

	return RE::PlayerCharacter::GetSingleton();
}

bool utils::caseInsensitiveCompare(const std::string& str, const char* cstr)
{
	size_t strLen = str.size();
	size_t cstrLen = std::strlen(cstr);

	if (strLen != cstrLen) {
		return false;
	}

	for (size_t i = 0; i < strLen; ++i) {
		if (std::tolower(static_cast<unsigned char>(str[i])) !=
			std::tolower(static_cast<unsigned char>(cstr[i]))) {
			return false;
		}
	}
	return true;
}