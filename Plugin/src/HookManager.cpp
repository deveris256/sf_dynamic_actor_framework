#include "HookManager.h"

namespace hooks
{
	const ActorUpdateFuncHook const* g_actorUpdateFuncHook{ ActorUpdateFuncHook::GetSingleton() };

	const ActorEquipManagerEquipFuncHook const* g_actorEquipManagerEquipFuncHook{ ActorEquipManagerEquipFuncHook::GetSingleton() };

	const ActorEquipManagerUnequipFuncHook const* g_actorEquipManagerUnequipFuncHook{ ActorEquipManagerUnequipFuncHook::GetSingleton() };
}
