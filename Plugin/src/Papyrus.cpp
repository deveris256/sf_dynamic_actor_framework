#include "Papyrus.h"

namespace DAFPapyrus
{
	// Scales
	int DAFPapyrus::ChangeScaleValue(std::monostate, RE::Actor* a_actor, std::string s_addon_id, std::string s_scale_id, int i_change_amount)
	{
		scales::Scale scale;

		if (a_actor == nullptr) {
			return -1;
		}

		try {
			scale = scales::GetScaleByID(s_addon_id, s_scale_id);
		} catch (InvalidAddonFileException) {
			return -1;
		}
		catch (InvalidScaleException) {
			return -1;
		}

		int sum = scale.ChangeValue(i_change_amount);

		return sum;
	}
}