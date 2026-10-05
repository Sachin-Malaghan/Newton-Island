// CURIO ISLES: the level solver - proves every level can be solved within par. (CLAUDE.md: Level validation)
//
// Sliders snap to their steps, so a level's whole setup space is finite: every way of putting the
// tray's parts into slots, times every slider notch. The solver runs the real sim over that space
// (evenly sampled when it is huge) and reports how many setups win. It doubles as the hint system's
// source of a correct answer and as a difficulty meter (the winning fraction).
#pragma once

#include "CISim.h"

namespace CI
{
	struct FSolveReport
	{
		long long Tried = 0;
		long long Solved = 0;
		int MinParts = -1;              // fewest parts in a winning setup (-1 = unsolvable)
		bool bDefaultsSolve = false;    // some placement wins with untouched sliders (too easy)
		bool bEmptySolves = false;      // winning without placing anything (broken level)
		FSetup Example;                 // a winning setup with MinParts parts (sliders nearest their defaults)
		double WinFraction() const { return Tried > 0 ? double(Solved) / double(Tried) : 0.0; }
	};

	// MaxRuns caps the number of simulations (the grid is sampled evenly beyond it).
	FSolveReport SolveLevel(const FLevelDef& Level, const FPartCatalog& Catalog, long long MaxRuns = 120000);

	// Runs one setup to the end; true if it wins.
	bool RunSetup(const FLevelDef& Level, const FPartCatalog& Catalog, const FSetup& Setup, FSim* OutSim = nullptr);
}
