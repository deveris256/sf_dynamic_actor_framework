#pragma once
#include "Utils.h"
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

namespace daf
{
	class DataManipulator
	{
	public:
		float         value;
		float         min;
		float         max;
		bool          start_on;
		std::string   id;
		std::string   addon_id;

		DataManipulator(std::string dm_addon_id, std::string dm_id)
		{
			value = 0.0f;
			min = 0.0f;
			max = 0.0f;
			start_on = false;
			id = dm_id;
			addon_id = dm_addon_id;
		}

		DataManipulator(nlohmann::json data)
		{
			value = 0.0f;
			min = 0.0f;
			max = 0.0f;
			start_on = false;
			id = data.value("dm_id", "unknown");
			addon_id = data.value("addon_id", "unknown");
		}

		int ChangeValue(int amount)
		{
			int sum;

			sum = value + amount;
			
			if (sum >= max) {
				this->value = max;
			} else if (sum <= min) {
				this->value = min;
			} else {
				this->value = sum;
			}

			return this->value;
		}
	};
}