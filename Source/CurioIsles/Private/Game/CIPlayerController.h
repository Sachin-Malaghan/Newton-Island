// CURIO ISLES: owns the game flow; gathers mouse, touch and keyboard input; app lifecycle; capture script. (CLAUDE.md: Game flow / Input)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/CIGame.h"
#include "CIPlayerController.generated.h"

class UCISaveGame;

UCLASS()
class ACIPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACIPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void PlayerTick(float DeltaTime) override;
	// The game draws its own controls; never create Unreal's default virtual joysticks (they would sit on
	// top and swallow taps).
	virtual void CreateTouchInterface() override {}

	bool IsTouchDevice() const;

	FCIGame Game;
	FCIPointer Pointer;

	UPROPERTY() TObjectPtr<UCISaveGame> Save;

private:
	void GatherInput(FCIKeys& Keys);
	void UpdateSafeArea();
	void TickCapture(float DeltaTime);
	void HandleBackground();

	bool bWasDown = false;
	double SafeAreaTimer = 0;
	FDelegateHandle BackgroundHandle, DeactivateHandle;

	// Capture script (-CICapture): screenshots of every screen and level state, then quit.
	bool bCapture = false;
	int32 CaptureStep = -1;
	bool bCaptureShotTaken = false;
	double CaptureClock = 0;
	FString CaptureTag;
};
