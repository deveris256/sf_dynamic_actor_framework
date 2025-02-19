/* 
 * https://github.com/Starfield-Reverse-Engineering/CommonLibSF
 * This plugin template links against CommonLibSF
 */

#define BETTERAPI_IMPLEMENTATION

#include "betterapi.h"

static const BetterAPI*            API = NULL;
static const struct simple_draw_t* UI = NULL;

#include "Papyrus.h"
#include "LogWrapper.h"
#include "HookManager.h"
#include "SFEventHandler.h"

#include "Addon.h"
static std::vector<daf::Addon> daf_addons;

// Modules
#include "MorphEvaluationRuleSet.h"
#include "ConditionalMorphManager.h"

void MessageCallback(SFSE::MessagingInterface::Message* a_msg) noexcept
{
	events::GameDataLoadedEventDispatcher::GetSingleton()->Dispatch({ SFSE::MessagingInterface::MessageType(a_msg->type) });

	switch (a_msg->type) {
	case SFSE::MessagingInterface::kPostDataLoad:
		{
			logger::info("Initializing core components.", utils::GetPluginName());
			
			daf::LoadAddons(&daf_addons);

			daf::ActorAppearanceUpdator::GetSingleton().Register();

			daf::ConditionalChargenMorphManager::GetSingleton().Register();
		}
		break;
	case SFSE::MessagingInterface::kPostLoad:
		{
			logger::info("{} loaded.", utils::GetPluginName());

			hooks::InstallHooks();
		}
		break;
	default:
		break;
	}
}

namespace daf_ui
{
	static void DrawCallback(void* imgui_context)
	{
		UI->Text("DAF");

		if (UI->Button("Reload addons")) {
			daf::LoadAddons(&daf_addons);
		}

		UI->Text("Addon Information");
		for (auto& addon : daf_addons)
		{
			UI->Text(addon.addon_name.c_str());
			UI->Text(addon.addon_folder.c_str());
		}
	}
}

static int OnBetterConsoleLoad(const struct better_api_t* BetterAPI)
{
	RegistrationHandle ec_mod_handle = BetterAPI->Callback->RegisterMod("DAF");

	BetterAPI->Callback->RegisterDrawCallback(ec_mod_handle, &daf_ui::DrawCallback);
	API = BetterAPI;

	UI = BetterAPI->SimpleDraw;

	return 0;
}

/*
void BindPapyrusFunctions(RE::BSScript::IVirtualMachine** a_vm)
{
	(*a_vm)->BindNativeMethod("DAF", "ChangeScaleValue", &DAFPapyrus::ChangeScaleValue, true, false);
}
*/

DLLEXPORT bool SFSEAPI SFSEPlugin_Load(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse, false);

	SFSE::GetMessagingInterface()->RegisterListener(MessageCallback);
	//SFSE::SetPapyrusCallback(&BindPapyrusFunctions);

	return true;
}
