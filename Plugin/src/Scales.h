#pragma once
#include "Utils.h"
#include <yaml-cpp/yaml.h>

namespace scales
{
	class Scale
	{
	public:
		float         value;
		float         min;
		float         max;
		bool          start_on;
		std::string   id;

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


	scales::Scale GetScaleByID(std::string addon_id, std::string id);
}