#pragma once
#include <yaml-cpp/yaml.h>
#include "Exceptions.h"

namespace utils
{
	std::string GetPluginFolder();

	std::string GetAddonsFolder();

	std::string GetAddonFilePath(std::string addonFileName);

	YAML::Node GetAddonFile(std::string addonFileName);
}