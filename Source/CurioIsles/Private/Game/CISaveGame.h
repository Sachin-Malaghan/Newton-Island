// CURIO ISLES: saved progress and settings (one slot, all platforms). (CLAUDE.md: Game flow)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CISaveGame.generated.h"

UCLASS()
class UCISaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* SlotName = TEXT("CurioIsles");

	UPROPERTY() int32 Version = 1;
	// Presentation only: Student Mode shows numbers, units, grids and formulas. Never changes the physics.
	UPROPERTY() bool bStudentMode = false;
	UPROPERTY() TMap<FString, int32> Stars;        // level id -> best stars (0..3)
	UPROPERTY() TArray<FString> Notebook;          // concept card ids collected
	UPROPERTY() int32 LastWorld = 0;
	UPROPERTY() int32 LastLevel = 0;
	UPROPERTY() bool bSound = true;
	UPROPERTY() bool bMusic = true;
};
