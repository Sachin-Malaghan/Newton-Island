// CURIO ISLES: draws the world - backdrop, level surfaces, parts, bodies, goals, overlays, effects. (CLAUDE.md: Rendering)
// Reads the game and the simulation; never changes them.
#pragma once

#include "CoreMinimal.h"

class FCIDraw;
class FCIGame;

class FCIWorldRenderer
{
public:
	static void Draw(FCIDraw& D, FCIGame& G);
};
