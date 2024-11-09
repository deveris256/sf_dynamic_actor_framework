#include "HookManager.h"

namespace hooks
{
	const ActorUpdateFuncHook const* g_actorUpdateFuncHook{ ActorUpdateFuncHook::GetSingleton() };
}
