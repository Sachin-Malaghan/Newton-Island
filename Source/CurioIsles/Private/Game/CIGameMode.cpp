// CURIO ISLES: game mode. (CLAUDE.md: Architecture)
#include "Game/CIGameMode.h"

#include "Game/CIHUD.h"
#include "Game/CIPlayerController.h"

ACIGameMode::ACIGameMode()
{
	PlayerControllerClass = ACIPlayerController::StaticClass();
	HUDClass = ACIHUD::StaticClass();
	DefaultPawnClass = nullptr;
}
