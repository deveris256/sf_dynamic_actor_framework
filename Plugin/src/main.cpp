/* 
 * https://github.com/Starfield-Reverse-Engineering/CommonLibSF
 * This plugin template links against CommonLibSF
 */

#include "Papyrus.h"

namespace
{
	void MessageCallback(SFSE::MessagingInterface::Message* a_msg) noexcept
	{
		switch (a_msg->type) {
		case SFSE::MessagingInterface::kPostLoad:
			{
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
}

DLLEXPORT bool SFSEAPI SFSEPlugin_Load(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse, false);

	SFSE::GetMessagingInterface()->RegisterListener(MessageCallback);
	SFSE::SetPapyrusCallback(&BindPapyrusFunctions);

	return true;
}
