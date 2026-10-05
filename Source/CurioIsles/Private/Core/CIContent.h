// CURIO ISLES: the content model - islands, worlds, parts and levels, all read from JSON. (CLAUDE.md: Data-driven content)
// Nothing here knows about Unreal; the game hands in file text through FContentReader.
#pragma once

#include "CIJson.h"
#include "CIMath.h"

#include <functional>
#include <string>
#include <vector>

namespace CI
{
	// ---------------------------------------------------------------- parts

	// What a part does in the simulation. New behaviours are code; everything else about a part is data.
	enum class EPartBehavior { Ramp, Launcher, Brake };

	// A slider on a part. Values snap to Step, so the solver can try every value a player can pick.
	struct FParamDef
	{
		std::string Id;
		std::string Label;        // "Height"
		std::string Symbol;       // "h"
		std::string Unit;         // "m" (Student Mode only)
		std::string Less, More;   // Fun Mode words at the two ends: "low" / "high"
		double Min = 0, Max = 1, Step = 0.1, Default = 0.5;
		int Decimals = 1;

		double Snap(double V) const;
		int NumValues() const;            // how many values the slider can take
		double ValueAt(int Index) const;  // Min + Index * Step, clamped
	};

	struct FPartDef
	{
		std::string Id;
		std::string Name;
		std::string Blurb;        // one line under the name in the tray
		std::string Look;         // renderer style key
		EPartBehavior Behavior = EPartBehavior::Ramp;
		std::vector<FParamDef> Params;
		FJson Props;              // behaviour constants (ramp length, pad length...)

		double Prop(const char* Key, double Default) const { return Props.Num(Key, Default); }
		int ParamIndex(const std::string& ParamId) const;
	};

	struct FPartCatalog
	{
		std::vector<FPartDef> Parts;
		const FPartDef* Find(const std::string& Id) const;
	};

	// ---------------------------------------------------------------- levels

	struct FSurfaceDef
	{
		std::vector<FVec2> Points;      // polyline; physics uses its segments
		double Friction = 0;            // Coulomb coefficient (kinetic = static)
		double Restitution = 0.25;
		std::string Look = "ground";    // "ground" fills down to the bottom of the view, "block" fills the polygon, "line" draws a thin rail
	};

	struct FZoneDef
	{
		std::string Id;
		std::string Look;               // "basket", "bell", "pond", "button", "river"
		FVec2 Min, Max;
		double Level = 0;               // water surface height for "pond"/"river" (default: top of the rect)
		bool Contains(const FVec2& P) const { return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y; }
	};

	enum class EBodyKind { Ball, Cart };

	struct FBodyDef
	{
		std::string Id;
		EBodyKind Kind = EBodyKind::Ball;
		std::string Look;
		double Radius = 0.15;           // collision circle (a cart rides on it)
		double Mass = 1;
		FVec2 Pos, Vel;                 // used when not attached to a part
		std::string OnPart;             // attach to this part type's anchor if it is placed
		std::string Anchor;
	};

	struct FSlotDef
	{
		std::string Id;
		FVec2 Pos;
		std::vector<std::string> Accepts;
		double Facing = 1;              // +1 parts face right, -1 left
		bool AcceptsPart(const std::string& PartId) const;
	};

	// A tray entry: a part the player may place, with this level's slider ranges.
	struct FTrayItem
	{
		std::string Part;
		int Count = 1;
		std::vector<FParamDef> Params;  // catalog params with the level's overrides applied
	};

	// A part the level places itself (the player may still be allowed to tune it).
	struct FFixedPart
	{
		std::string Part;
		std::string Slot;
		std::vector<FParamDef> Params;
		std::vector<double> Values;
		bool bTunable = false;
	};

	enum class EGoalType { Enter, Rest };

	struct FGoalDef
	{
		EGoalType Type = EGoalType::Enter;
		std::string Body;
		std::string Zone;
	};

	struct FFailDef
	{
		std::string Body;
		std::string Zone;
		std::string Say;                // "Splash!"
	};

	struct FPredictDef
	{
		std::string Question;           // "Where will the ball land?"
		std::string Measure;            // variable the answer is checked against, e.g. "ball.landX"
		std::vector<std::string> Choices;   // Fun Mode picture choices (labels)
	};

	struct FLevelDef
	{
		std::string Id;
		std::string File;
		std::string Name;
		std::string Goal;               // "Roll the ball into the basket"
		std::string Concept;            // Lab Notebook card id
		std::vector<std::string> Syllabus;
		std::string Why;                // Fun Mode: one sentence, no jargon
		std::string WhyStudent;         // Student Mode explanation
		std::string Formula;            // "v = √(2gh)"
		std::string Example;            // worked example template, see FormatTemplate
		std::string StopText = "It stopped short.";

		double Gravity = 9.81;
		double TimeLimit = 12;
		double PreviewSeconds = 0;      // dotted path preview length (0 = none)
		int Par = 1;
		FVec2 ViewMin, ViewMax;         // camera frames this rectangle (metres)
		FVec2 BoundsMin, BoundsMax;     // a body leaving this is lost

		std::vector<FSurfaceDef> Surfaces;
		std::vector<FZoneDef> Zones;
		std::vector<FBodyDef> Bodies;
		std::vector<FSlotDef> Slots;
		std::vector<FTrayItem> Tray;
		std::vector<FFixedPart> Fixed;
		std::vector<FGoalDef> Goals;
		std::vector<FFailDef> Fails;
		FPredictDef Predict;

		int FindSlot(const std::string& SlotId) const;
		int FindZone(const std::string& ZoneId) const;
		int FindBody(const std::string& BodyId) const;
	};

	// ---------------------------------------------------------------- islands and worlds

	struct FWorldTheme
	{
		// sRGB colours, 0xRRGGBB.
		unsigned SkyTop = 0x8fd3f4, SkyBottom = 0xfdf1c7;
		unsigned Hills[3] = { 0xb8e0a8, 0x8fcf86, 0x6bb870 };
		unsigned Ground = 0x5aa35f, GroundDark = 0x3f7f4c;
		unsigned Accent = 0xffb347;
		unsigned Sun = 0xfff4c2;
	};

	struct FWorldDef
	{
		std::string Id;
		std::string Name;
		std::string Teaches;
		int Number = 1;
		FWorldTheme Theme;
		std::vector<FLevelDef> Levels;
	};

	struct FIslandDef
	{
		std::string Id;
		std::string Name;
		std::string Subject;
		std::string Mentor;
		std::string Folder;             // relative to the Islands root
		FPartCatalog Catalog;
		std::vector<FWorldDef> Worlds;
	};

	// Reads a file below the Islands root ("Newton/island.json"); returns false if missing.
	using FContentReader = std::function<bool(const std::string& RelativePath, std::string& OutText)>;

	// Loads one island (its manifest, part catalog and every level). Errors name the file.
	bool LoadIsland(const std::string& Folder, const FContentReader& Read, FIslandDef& Out, std::string& OutError);

	// Parses one level against a catalog (used by LoadIsland and by tests that build levels in code).
	bool ParseLevel(const FJson& J, const FPartCatalog& Catalog, FLevelDef& Out, std::string& OutError);
	bool ParseCatalog(const FJson& J, FPartCatalog& Out, std::string& OutError);
}
