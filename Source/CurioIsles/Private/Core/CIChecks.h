// CURIO ISLES: physics and content checks shared by the harness and the Unreal automation tests. (CLAUDE.md: Testing)
#pragma once

#include "CISim.h"

#include <string>
#include <vector>

namespace CI
{
	struct FCheck
	{
		std::string Name;
		bool bPass = false;
		std::string Detail;
	};

	// Textbook formulas (projectile range, v = sqrt(2gh), v^2 = 2as, free fall), deterministic maths,
	// and bit-identical repeat runs.
	void RunPhysicsChecks(std::vector<FCheck>& Out);

	// Every level: solvable within par, not solved by untouched sliders, deterministic over 100 runs,
	// worked example renders. bFast samples the setup space more coarsely.
	void RunIslandChecks(const FIslandDef& Island, std::vector<FCheck>& Out, bool bFast = false);

	// FNV-1a over every body's position and velocity after each step of a run (bit-exact fingerprint).
	unsigned long long RunFingerprint(const FLevelDef& Level, const FPartCatalog& Catalog, const FSetup& Setup);
}
