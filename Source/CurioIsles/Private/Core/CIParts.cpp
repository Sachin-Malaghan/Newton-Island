// CURIO ISLES: turns a placed part (type + slot + slider values) into physics geometry. (CLAUDE.md: Parts)
#include "CIParts.h"

namespace CI
{
	const FAnchor* FPartGeometry::FindAnchor(const std::string& Name) const
	{
		for (const FAnchor& A : Anchors) { if (Name.empty() || A.Name == Name) { return &A; } }
		return nullptr;
	}

	namespace
	{
		double Value(const FPartDef& Part, const std::vector<double>& Values, const char* Id, double Default)
		{
			const int I = Part.ParamIndex(Id);
			return I >= 0 && I < (int)Values.size() ? Values[I] : Default;
		}

		// A straight incline: foot at the slot, rising away from the facing direction, with a short
		// flat lip at the top. The ball starts just below the lip.
		void BuildRamp(const FPartDef& Part, const std::vector<double>& Values, const FSlotDef& Slot, FPartGeometry& Out)
		{
			const double F = Slot.Facing;
			const double L = Part.Prop("length", 2.4);
			const double Lip = Part.Prop("lip", 0.35);
			const double H = Value(Part, Values, "height", 1.0);
			const FVec2 Foot = Slot.Pos;
			const FVec2 Top = Foot + FVec2(-F * L, H);

			FChain C;
			C.Friction = Part.Prop("friction", 0.0);
			C.Restitution = Part.Prop("restitution", 0.1);
			C.Points = { Top + FVec2(-F * Lip, 0), Top, Foot };
			Out.Chains.push_back(C);

			const FVec2 Down = (Foot - Top).Normalized();
			FVec2 N = Down.Perp();
			if (N.Y < 0) { N = -N; }
			FAnchor A;
			A.Name = "top";
			A.Point = Top + Down * Part.Prop("startOffset", 0.3);
			A.Normal = N;
			Out.Anchors.push_back(A);
		}

		// A cannon on a pivot. Angle is measured up from the horizontal, toward the facing side.
		void BuildLauncher(const FPartDef& Part, const std::vector<double>& Values, const FSlotDef& Slot, FPartGeometry& Out)
		{
			const double F = Slot.Facing;
			const double Deg = Value(Part, Values, "angle", 45);
			const double Speed = Value(Part, Values, "speed", 8);
			const double Rad = Deg * kDegToRad;
			const FVec2 Dir(F * Cos(Rad), Sin(Rad));
			Out.Pivot = Slot.Pos + FVec2(0, Part.Prop("pivotHeight", 0.35));
			Out.Angle = F > 0 ? Rad : kPi - Rad;

			FAnchor A;
			A.Name = "muzzle";
			A.Point = Out.Pivot + Dir * Part.Prop("barrel", 0.7);
			A.Normal = FVec2();
			A.Velocity = Dir * Speed;
			Out.Anchors.push_back(A);
		}

		// A flat pad on the ground, starting at the slot and running in the facing direction.
		void BuildBrake(const FPartDef& Part, const std::vector<double>& Values, const FSlotDef& Slot, FPartGeometry& Out)
		{
			const double F = Slot.Facing;
			const double L = Part.Prop("length", 4.0);
			FBrakeRegion R;
			const double X0 = Slot.Pos.X, X1 = Slot.Pos.X + F * L;
			R.Min = FVec2(Min(X0, X1), Slot.Pos.Y - 0.25);
			R.Max = FVec2(Max(X0, X1), Slot.Pos.Y + Part.Prop("reach", 1.5));
			R.Decel = Value(Part, Values, "strength", 2);
			Out.Brakes.push_back(R);
		}
	}

	void BuildPartGeometry(const FPartDef& Part, const std::vector<double>& Values, const FSlotDef& Slot, FPartGeometry& Out)
	{
		Out = FPartGeometry();
		Out.Base = Slot.Pos;
		Out.Facing = Slot.Facing;
		switch (Part.Behavior)
		{
		case EPartBehavior::Ramp: BuildRamp(Part, Values, Slot, Out); break;
		case EPartBehavior::Launcher: BuildLauncher(Part, Values, Slot, Out); break;
		case EPartBehavior::Brake: BuildBrake(Part, Values, Slot, Out); break;
		}
	}
}
