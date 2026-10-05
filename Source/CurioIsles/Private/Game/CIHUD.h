// CURIO ISLES: paints the whole game onto the canvas each frame. (CLAUDE.md: Rendering / UI)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CIHUD.generated.h"

class UFont;

UCLASS()
class ACIHUD : public AHUD
{
	GENERATED_BODY()

public:
	ACIHUD();
	virtual void DrawHUD() override;

private:
	UPROPERTY() TObjectPtr<UFont> Font;
};
