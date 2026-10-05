// CURIO ISLES: menus and the in-level HUD - tray, sliders, buttons, mode toggle, result cards. (CLAUDE.md: UI)
// Immediate-mode: every frame the UI draws itself and registers its hit boxes on the game, which the
// controller tests against the next frame's input.
#pragma once

#include "CoreMinimal.h"

class FCIDraw;
class FCIGame;
struct FCIPointer;

class FCIUI
{
public:
	// Sets the layout the camera depends on (top bar and tray heights). Call before drawing the world.
	static void Layout(FCIDraw& D, FCIGame& G);
	static void Draw(FCIDraw& D, FCIGame& G, const FCIPointer& Pointer);
};
