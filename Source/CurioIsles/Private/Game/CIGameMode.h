// CURIO ISLES: wires the controller and HUD; there is no pawn - the machines live in the 2D sim. (CLAUDE.md: Architecture)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CIGameMode.generated.h"

UCLASS()
class ACIGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACIGameMode();
	virtual void RestartPlayer(AController* NewPlayer) override {}
};
