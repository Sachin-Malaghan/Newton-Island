// CURIO ISLES: the deterministic "teaching" physics. (CLAUDE.md: Deterministic core)
//
// Fixed 1/120 s steps. Between events a body moves with constant acceleration, which is integrated
// exactly (x += v t + a t^2 / 2), so free flight, slopes and braking match the textbook formulas; events
// (hitting a surface, entering a brake pad, stopping) are located by bisection inside the step. Only
// deterministic maths is used (CIMath.h), so the same level + setup gives bit-identical results on
// every run and every device. The sim is copyable by value (the path preview clones it).
#pragma once

#include "CIContent.h"
#include "CIParts.h"

#include <string>
#include <vector>

namespace CI
{
	constexpr double kStep = 1.0 / 120.0;

	// A part the player (or the level) has put in a slot.
	struct FPlacedPart
	{
		int Tray = -1;                  // index into FLevelDef::Tray
		int Slot = -1;                  // index into FLevelDef::Slots
		std::vector<double> Values;     // one per slider, same order as FTrayItem::Params
	};

	// Everything the player controls in a level. Two equal setups always give equal runs.
	struct FSetup
	{
		std::vector<FPlacedPart> Placed;
		std::vector<std::vector<double>> FixedValues;   // per FLevelDef::Fixed entry (tunable sliders)

		static FSetup Defaults(const FLevelDef& Level);
		int PartInSlot(int Slot) const;                  // index into Placed, or -1
		int CountPlaced(int Tray) const;
		int NumPlaced() const { return (int)Placed.size(); }
	};

	struct FSeg
	{
		FVec2 A, B, T;      // T = unit direction A -> B
		double Len = 0;
		double Friction = 0, Restitution = 0.2;
		int Part = -1;      // -1 = level surface
	};

	enum class ESimEvent : unsigned char { Launch, Bounce, Land, Rest, Goal, Fail, Lost };

	struct FSimEvent
	{
		ESimEvent Type = ESimEvent::Bounce;
		int Body = -1;
		FVec2 Pos;
		double Strength = 0;
	};

	struct FBody
	{
		FVec2 P, V;
		double R = 0.15;
		int Contact = -1;       // segment it is sliding/rolling on
		FVec2 N;                // contact normal (points from the surface to the body)
		bool bResting = false;
		bool bGone = false;
		// Measurements (Student Mode readouts, worked examples, predictions).
		FVec2 StartP;
		double Distance = 0;
		double MaxSpeed = 0;
		bool bLanded = false;   // first touch-down after flight
		FVec2 LandP;
		double LandTime = 0;
		double GoalTime = -1, GoalSpeed = 0;
		bool bExited = false;   // first take-off into flight (leaving a surface, or fired by a launcher)
		double ExitSpeed = 0;
	};

	enum class EOutcome { Running, Success, Fail };

	// One placed part after geometry building (level fixed parts first, then the player's).
	struct FPartInstance
	{
		const FPartDef* Part = nullptr;
		int Slot = -1;
		int Tray = -1;          // -1 for fixed parts
		int Fixed = -1;
		std::vector<double> Values;
		FPartGeometry Geo;
	};

	class FSim
	{
	public:
		bool Load(const FLevelDef& InLevel, const FPartCatalog& Catalog, const FSetup& Setup, std::string* OutError = nullptr);
		void Step();
		// Steps until the run ends (or MaxSeconds of sim time). Returns the outcome.
		EOutcome Run(double MaxSeconds = 1e9);

		// Named measurements: "time", "<body>.x", ".y", ".speed", ".distance", ".landX", ".landTime",
		// ".goalTime", ".goalSpeed", ".maxSpeed". False if unknown.
		bool Measure(const std::string& Name, double& Out) const;
		// Measure(), or a slider value by id ("height") or part.id ("ramp.height"). Feeds FormatTemplate.
		bool Variable(const std::string& Name, double& Out) const;

		const FLevelDef* Level = nullptr;
		FVec2 Gravity;
		double Time = 0;
		EOutcome Outcome = EOutcome::Running;
		const std::string* FailText = nullptr;   // points into the level or static text
		int StepCount = 0;
		// Keep moving after success/failure (the solved machine keeps playing on screen).
		bool bContinueAfterEnd = false;

		std::vector<FBody> Bodies;
		std::vector<FSeg> Segs;
		std::vector<FBrakeRegion> Brakes;
		std::vector<FPartInstance> Parts;
		std::vector<unsigned char> GoalDone;
		std::vector<FSimEvent> Events;           // appended during Step; the caller clears it (capacity is fixed)

	private:
		void Advance(int BodyIndex, double H);
		bool EventAt(const FBody& B, const FVec2& A, double S, unsigned BrakeMask, const int* Ignore, int NumIgnore) const;
		int DeepestPenetration(const FBody& B, const int* Ignore, int NumIgnore) const;
		void Collide(int BodyIndex, int SegIndex, int* Ignore, int& NumIgnore);
		void MaintainContact(FBody& B);
		bool IsSmoothJoint(int SegA, int SegB) const;
		FVec2 ContactNormal(const FSeg& S, const FVec2& P) const;
		double BrakeDecel(const FVec2& P) const;
		unsigned BrakeMask(const FVec2& P) const;
		void CheckZones(int BodyIndex);
		void Emit(ESimEvent Type, int Body, const FVec2& Pos, double Strength = 0);
		void Finish(EOutcome Result, const std::string* Text);

		double SubTime = 0;       // time into the current step (for measurements)
		int PendingFail = -1;
		std::vector<int> GoalBody, GoalZone, FailBody, FailZone;   // resolved at load (no string lookups while stepping)
	};
}
