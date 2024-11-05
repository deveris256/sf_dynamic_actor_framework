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
