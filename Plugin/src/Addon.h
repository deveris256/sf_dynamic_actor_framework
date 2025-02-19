#pragma once
#include "Utils.h"
#include "DataManipulator.h"

namespace daf
{
	class Addon
	{
	public:
		std::string addon_name;
		std::string addon_folder;
		std::vector<daf::DataManipulator> data_manipulators;

		Addon(std::string daf_addon_folder)
		{
			addon_name = std::filesystem::path(daf_addon_folder).filename().string();
			addon_folder = daf_addon_folder;

			if (std::filesystem::exists(daf_addon_folder + "\\data"))
			{
				LoadManipulators(daf_addon_folder);
			}

		}

	private:
		void LoadManipulators(std::string daf_addon_folder)
		{
			std::vector<nlohmann::json> manipulators_json = utils::GetJsonFiles(daf_addon_folder + "\\data");

			if (manipulators_json.size() > 0) {
				for (nlohmann::json& j_dm : manipulators_json) {
					DataManipulator data_manipulator(j_dm);
					data_manipulators.push_back(data_manipulator);
				}
			}
		}

	};

	void LoadAddons(std::vector<daf::Addon>* addons_var)
	{
		addons_var->clear();

		std::map<std::string, nlohmann::json> addons = utils::GetAddons();

		if (addons.size() > 0)
		{
			for (std::pair<std::string, nlohmann::json> j_add : addons) {
				Addon addon(j_add.first);
				addons_var->push_back(addon);
			}
		}		
	}
}
