/* 
 * https://github.com/Starfield-Reverse-Engineering/CommonLibSF
 * This plugin template links against CommonLibSF
 */

#include "Papyrus.h"
#include "LogWrapper.h"
#include "HookManager.h"
#include "SFEventHandler.h"

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

void BindPapyrusFunctions(RE::BSScript::IVirtualMachine** a_vm)
{
	(*a_vm)->BindNativeMethod("DAF", "ChangeScaleValue", &DAFPapyrus::ChangeScaleValue, true, false);
}

DLLEXPORT bool SFSEAPI SFSEPlugin_Load(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse, false);

	SFSE::GetMessagingInterface()->RegisterListener(MessageCallback);
	SFSE::SetPapyrusCallback(&BindPapyrusFunctions);

	return true;
}
