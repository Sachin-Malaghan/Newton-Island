// CURIO ISLES: turns a placed part (type + slot + slider values) into physics geometry. (CLAUDE.md: Parts)
// The renderer draws from the same geometry, so what you see is what the ball hits.
#pragma once

#include "CIContent.h"

#include <string>
#include <vector>

namespace CI
{
	// A polyline the bodies collide with.
	struct FChain
	{
		std::vector<FVec2> Points;
		double Friction = 0;
		double Restitution = 0.2;
	};

	// A patch of ground that decelerates whatever touches it (brake pads, mud).
	struct FBrakeRegion
	{
		FVec2 Min, Max;
		double Decel = 0;   // m/s^2
		bool Contains(const FVec2& P) const { return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y; }
	};

	// Where a body starts when it sits on a part. The body's centre goes to Point + Normal * radius.
	struct FAnchor
	{
		std::string Name;
		FVec2 Point;
		FVec2 Normal;
		FVec2 Velocity;     // launch velocity given at PLAY
	};

	struct FPartGeometry
	{
		std::vector<FChain> Chains;
		std::vector<FBrakeRegion> Brakes;
		std::vector<FAnchor> Anchors;

		// Drawing hints (the renderer owns the look).
		FVec2 Base;           // slot position
		double Facing = 1;
		double Angle = 0;     // launcher barrel angle, radians from +X toward +Y (already mirrored for Facing)
		FVec2 Pivot;          // launcher pivot

		const FAnchor* FindAnchor(const std::string& Name) const;
	};

	// Values are in the same order as Part.Params.
	void BuildPartGeometry(const FPartDef& Part, const std::vector<double>& Values, const FSlotDef& Slot, FPartGeometry& Out);
}
