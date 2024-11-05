#include "Scales.h"

// Utils
scales::Scale scales::GetScaleByID(std::string addon_id, std::string scale_id)
{
	YAML::Node addonFile;

	try {
		addonFile = utils::GetAddonFile(addon_id);
	}
	catch (InvalidAddonFileException) {
		throw new InvalidAddonFileException;
	}

	bool scale_found = false;
	Scale scaleObj;

	for (const auto& scale : addonFile["scales"]) {
		if (scale[scale_id])
		{
			scale_found = true;

			scaleObj.id = scale["id"].as<std::string>();
			scaleObj.start_on = scale["start_on"].as<bool>();
			break;
		}
	}

	if (!scale_found) {
		throw new InvalidScaleException;
	}

	return scaleObj;
}
