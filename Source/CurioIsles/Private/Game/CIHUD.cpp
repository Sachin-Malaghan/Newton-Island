// CURIO ISLES: paints the whole game onto the canvas each frame. (CLAUDE.md: Rendering / UI)
#include "Game/CIHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Game/CIPlayerController.h"
#include "Render/CIDraw.h"
#include "Render/CIWorldRenderer.h"
#include "UI/CIUI.h"
#include "UObject/ConstructorHelpers.h"

ACIHUD::ACIHUD()
{
	static ConstructorHelpers::FObjectFinder<UFont> Roboto(TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	Font = Roboto.Object;
}

void ACIHUD::DrawHUD()
{
	Super::DrawHUD();
	ACIPlayerController* PC = Cast<ACIPlayerController>(PlayerOwner);
	if (!PC || !Canvas) { return; }
	FCIGame& Game = PC->Game;

	FCIDraw D(Canvas, Font);
	FCIUI::Layout(D, Game);
	FCIWorldRenderer::Draw(D, Game);
	if (!Game.bHideUI) { FCIUI::Draw(D, Game, PC->Pointer); }
	else
	{
		Game.Buttons.Reset();
		Game.TrayCards.Reset();
		Game.Sliders.Reset();
	}
	D.Flush();
}
