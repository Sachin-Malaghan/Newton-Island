// CURIO ISLES: the deterministic "teaching" physics. (CLAUDE.md: Deterministic core)
#include "CISim.h"

namespace CI
{
	namespace
	{
		constexpr double kPen = 1e-9;          // penetration tolerance (m)
		constexpr double kTouch = 1e-6;        // a body this close to a surface at load time sits on it
		constexpr double kVelEps = 1e-9;
		constexpr double kLandSpeed = 0.35;    // bounces slower than this settle into rolling contact
		constexpr double kQuietLand = 0.05;    // touch-downs slower than this are not "landings" (joints, settling)
		constexpr double kJointGap = 0.03;     // surfaces this close count as joined
		constexpr int kBisect = 48;
		constexpr int kMaxIgnore = 8;
		constexpr size_t kEventCapacity = 256;

		const std::string TextLost = "Lost it!";
		const std::string TextTime = "Out of time.";

		double SegDistance(const FSeg& S, const FVec2& P, double& OutT, FVec2& OutC)
		{
			double Along = (P - S.A).Dot(S.T);
			if (Along < 0) { Along = 0; }
			if (Along > S.Len) { Along = S.Len; }
			OutT = S.Len > 0 ? Along / S.Len : 0;
			OutC = S.A + S.T * Along;
			return (P - OutC).Length();
		}

		double SegDistance(const FSeg& S, const FVec2& P)
		{
			double T;
			FVec2 C;
			return SegDistance(S, P, T, C);
		}

		bool Ignored(int I, const int* Ignore, int NumIgnore)
		{
			for (int K = 0; K < NumIgnore; ++K) { if (Ignore[K] == I) { return true; } }
			return false;
		}
	}

	// ---------------------------------------------------------------- setup

	FSetup FSetup::Defaults(const FLevelDef& Level)
	{
		FSetup S;
		for (const FFixedPart& F : Level.Fixed) { S.FixedValues.push_back(F.Values); }
		return S;
	}

	int FSetup::PartInSlot(int Slot) const
	{
		for (int I = 0; I < (int)Placed.size(); ++I) { if (Placed[I].Slot == Slot) { return I; } }
		return -1;
	}

	int FSetup::CountPlaced(int Tray) const
	{
		int N = 0;
		for (const FPlacedPart& P : Placed) { if (P.Tray == Tray) { ++N; } }
		return N;
	}

	// ---------------------------------------------------------------- load

	bool FSim::Load(const FLevelDef& InLevel, const FPartCatalog& Catalog, const FSetup& Setup, std::string* OutError)
	{
		auto Fail = [&](const std::string& Msg) { if (OutError) { *OutError = Msg; } return false; };

		Level = &InLevel;
		Gravity = FVec2(0, -InLevel.Gravity);
		Time = 0;
		StepCount = 0;
		SubTime = 0;
		Outcome = EOutcome::Running;
		FailText = nullptr;
		PendingFail = -1;
		Bodies.clear();
		Segs.clear();
		Brakes.clear();
		Parts.clear();
		Events.clear();
		Events.reserve(kEventCapacity);

		// Parts: the level's fixed ones, then the player's.
		std::vector<int> SlotUsed(InLevel.Slots.size(), 0);
		for (int I = 0; I < (int)InLevel.Fixed.size(); ++I)
		{
			const FFixedPart& F = InLevel.Fixed[I];
			FPartInstance Inst;
			Inst.Part = Catalog.Find(F.Part);
			Inst.Slot = InLevel.FindSlot(F.Slot);
			Inst.Fixed = I;
			if (!Inst.Part || Inst.Slot < 0) { return Fail("bad fixed part " + F.Part); }
			Inst.Values = F.Values;
			if (I < (int)Setup.FixedValues.size() && Setup.FixedValues[I].size() == F.Params.size())
			{
				for (size_t K = 0; K < F.Params.size(); ++K) { Inst.Values[K] = F.Params[K].Snap(Setup.FixedValues[I][K]); }
			}
			SlotUsed[Inst.Slot] = 1;
			Parts.push_back(Inst);
		}
		for (const FPlacedPart& P : Setup.Placed)
		{
			if (P.Tray < 0 || P.Tray >= (int)InLevel.Tray.size()) { return Fail("placed part has no tray entry"); }
			if (P.Slot < 0 || P.Slot >= (int)InLevel.Slots.size()) { return Fail("placed part has no slot"); }
			const FTrayItem& T = InLevel.Tray[P.Tray];
			if (!InLevel.Slots[P.Slot].AcceptsPart(T.Part)) { return Fail("slot " + InLevel.Slots[P.Slot].Id + " does not take " + T.Part); }
			if (SlotUsed[P.Slot]) { return Fail("slot " + InLevel.Slots[P.Slot].Id + " is taken"); }
			if (Setup.CountPlaced(P.Tray) > T.Count) { return Fail("too many " + T.Part); }
			SlotUsed[P.Slot] = 1;
			FPartInstance Inst;
			Inst.Part = Catalog.Find(T.Part);
			if (!Inst.Part) { return Fail("unknown part " + T.Part); }
			Inst.Slot = P.Slot;
			Inst.Tray = P.Tray;
			for (size_t K = 0; K < T.Params.size(); ++K)
			{
				Inst.Values.push_back(K < P.Values.size() ? T.Params[K].Snap(P.Values[K]) : T.Params[K].Default);
			}
			Parts.push_back(Inst);
		}
		for (FPartInstance& Inst : Parts) { BuildPartGeometry(*Inst.Part, Inst.Values, InLevel.Slots[Inst.Slot], Inst.Geo); }

		// Collision segments.
		auto AddChain = [&](const std::vector<FVec2>& Points, double Friction, double Restitution, int Part)
		{
			for (size_t K = 0; K + 1 < Points.size(); ++K)
			{
				FSeg S;
				S.A = Points[K];
				S.B = Points[K + 1];
				S.Len = (S.B - S.A).Length();
				if (S.Len <= 0) { continue; }
				S.T = (S.B - S.A) / S.Len;
				S.Friction = Friction;
				S.Restitution = Restitution;
				S.Part = Part;
				Segs.push_back(S);
			}
		};
		for (const FSurfaceDef& S : InLevel.Surfaces) { AddChain(S.Points, S.Friction, S.Restitution, -1); }
		for (int I = 0; I < (int)Parts.size(); ++I)
		{
			for (const FChain& C : Parts[I].Geo.Chains) { AddChain(C.Points, C.Friction, C.Restitution, I); }
			for (const FBrakeRegion& R : Parts[I].Geo.Brakes) { Brakes.push_back(R); }
		}
		if (Brakes.size() > 32) { return Fail("too many brake regions"); }

		// Bodies: on their part's anchor if that part is placed, else where the level puts them.
		for (const FBodyDef& D : InLevel.Bodies)
		{
			FBody B;
			B.R = D.Radius;
			B.P = D.Pos;
			B.V = D.Vel;
			if (!D.OnPart.empty())
			{
				for (const FPartInstance& Inst : Parts)
				{
					if (Inst.Part->Id != D.OnPart) { continue; }
					if (const FAnchor* A = Inst.Geo.FindAnchor(D.Anchor))
					{
						B.P = A->Point + A->Normal * B.R;
						B.V = A->Velocity;
					}
					break;
				}
			}
			B.StartP = B.P;
			// Resting on a surface already? Then it starts in contact.
			for (int I = 0; I < (int)Segs.size(); ++I)
			{
				double T;
				FVec2 C;
				const double Dist = SegDistance(Segs[I], B.P, T, C);
				if (Abs(Dist - B.R) > kTouch || T <= 0 || T >= 1) { continue; }
				const FVec2 N = ContactNormal(Segs[I], B.P);
				if (Gravity.Dot(N) < 0 && B.V.Dot(N) <= kVelEps)
				{
					B.Contact = I;
					B.N = N;
					B.P = C + N * B.R;
					B.V -= N * B.V.Dot(N);
					break;
				}
			}
			Bodies.push_back(B);
		}

		GoalDone.assign(InLevel.Goals.size(), 0);
		GoalBody.clear(); GoalZone.clear(); FailBody.clear(); FailZone.clear();
		for (const FGoalDef& G : InLevel.Goals) { GoalBody.push_back(InLevel.FindBody(G.Body)); GoalZone.push_back(InLevel.FindZone(G.Zone)); }
		for (const FFailDef& F : InLevel.Fails) { FailBody.push_back(InLevel.FindBody(F.Body)); FailZone.push_back(InLevel.FindZone(F.Zone)); }

		for (int I = 0; I < (int)Bodies.size(); ++I)
		{
			if (Bodies[I].V.LengthSq() > 0 && Bodies[I].Contact < 0)
			{
				Bodies[I].bExited = true;
				Bodies[I].ExitSpeed = Bodies[I].V.Length();
			}
			if (Bodies[I].V.LengthSq() > 0) { Emit(ESimEvent::Launch, I, Bodies[I].P, Bodies[I].V.Length()); }
		}
		return true;
	}

	// ---------------------------------------------------------------- stepping

	void FSim::Step()
	{
		if (Outcome != EOutcome::Running && !bContinueAfterEnd) { return; }
		const double G = Gravity.Length();
		for (int I = 0; I < (int)Bodies.size(); ++I)
		{
			FBody& B = Bodies[I];
			SubTime = 0;
			if (B.bGone || B.bResting) { continue; }
			// Substeps keep each move under 0.4 radii, so nothing tunnels through a thin surface.
			const double Reach = B.V.Length() * kStep + 0.5 * G * kStep * kStep;
			int N = (int)std::ceil(Reach / (0.4 * B.R));
			N = N < 1 ? 1 : (N > 64 ? 64 : N);
			const double H = kStep / N;
			for (int K = 0; K < N && !B.bGone && !B.bResting; ++K) { Advance(I, H); }
		}
		++StepCount;
		Time = StepCount * kStep;
		SubTime = 0;
		if (Outcome != EOutcome::Running) { return; }

		bool bAll = true;
		for (int G2 = 0; G2 < (int)GoalDone.size(); ++G2)
		{
			if (Level->Goals[G2].Type == EGoalType::Rest)
			{
				const FBody& B = Bodies[GoalBody[G2]];
				GoalDone[G2] = B.bResting && Level->Zones[GoalZone[G2]].Contains(B.P) ? 1 : 0;
				if (GoalDone[G2] && B.GoalTime < 0) { Bodies[GoalBody[G2]].GoalTime = Time; }
			}
			bAll = bAll && GoalDone[G2];
		}
		if (bAll) { Finish(EOutcome::Success, nullptr); return; }
		if (PendingFail >= 0) { Finish(EOutcome::Fail, &Level->Fails[PendingFail].Say); return; }

		bool bAllGone = true, bAllStill = true;
		for (const FBody& B : Bodies)
		{
			bAllGone = bAllGone && B.bGone;
			bAllStill = bAllStill && (B.bGone || B.bResting);
		}
		if (bAllGone) { Finish(EOutcome::Fail, &TextLost); return; }
		if (bAllStill) { Finish(EOutcome::Fail, &Level->StopText); return; }
		if (Time >= Level->TimeLimit) { Finish(EOutcome::Fail, &TextTime); }
	}

	EOutcome FSim::Run(double MaxSeconds)
	{
		while (Outcome == EOutcome::Running && Time < MaxSeconds) { Step(); }
		return Outcome;
	}

	void FSim::Advance(int BodyIndex, double H)
	{
		FBody& B = Bodies[BodyIndex];
		int Ignore[kMaxIgnore];
		int NumIgnore = 0;
		// Surfaces a body already overlaps (a level-design slip) are ignored rather than fought.
		for (int I = 0; I < (int)Segs.size() && NumIgnore < kMaxIgnore; ++I)
		{
			if (I != B.Contact && SegDistance(Segs[I], B.P) < B.R - kPen) { Ignore[NumIgnore++] = I; }
		}

		auto Move = [&](const FVec2& A, double Dt)
		{
			const FVec2 NewP = B.P + B.V * Dt + A * (0.5 * Dt * Dt);
			B.Distance += (NewP - B.P).Length();
			B.P = NewP;
			B.V = B.V + A * Dt;
			const double Speed = B.V.Length();
			if (Speed > B.MaxSpeed) { B.MaxSpeed = Speed; }
			SubTime += Dt;
		};

		double Remaining = H;
		for (int Iter = 0; Iter < 16 && Remaining > 0 && !B.bGone && !B.bResting; ++Iter)
		{
			// Acceleration for this stretch: gravity, or its component along the surface minus friction/brakes.
			FVec2 A = Gravity;
			double Tau = Remaining;
			bool bStop = false;
			if (B.Contact >= 0)
			{
				const FSeg& S = Segs[B.Contact];
				const double GN = Gravity.Dot(B.N);
				if (GN >= 0) { B.Contact = -1; }
				else
				{
					const double Decel = S.Friction * (-GN) + BrakeDecel(B.P);
					const double VT = B.V.Dot(S.T);
					const double GT = Gravity.Dot(S.T);
					double AT;
					if (Abs(VT) <= kVelEps)
					{
						if (Abs(GT) <= Decel)
						{
							B.V = FVec2();
							B.bResting = true;
							Emit(ESimEvent::Rest, BodyIndex, B.P);
							CheckZones(BodyIndex);
							return;
						}
						B.V = FVec2();
						AT = GT - Decel * Sign(GT);
					}
					else
					{
						AT = GT - Decel * Sign(VT);
						// Friction/brakes would reverse it: stop exactly when the speed reaches zero.
						if (VT * AT < 0)
						{
							const double Ts = -VT / AT;
							if (Ts <= Remaining) { Tau = Ts; bStop = true; }
						}
					}
					A = S.T * AT;
				}
			}

			const unsigned Mask = B.Contact >= 0 ? BrakeMask(B.P) : 0;
			if (EventAt(B, A, Tau, Mask, Ignore, NumIgnore))
			{
				double Lo = 0, Hi = Tau;
				for (int K = 0; K < kBisect; ++K)
				{
					const double Mid = 0.5 * (Lo + Hi);
					if (EventAt(B, A, Mid, Mask, Ignore, NumIgnore)) { Hi = Mid; } else { Lo = Mid; }
				}
				FBody Probe = B;
				Probe.P = B.P + B.V * Hi + A * (0.5 * Hi * Hi);
				const int Seg = DeepestPenetration(Probe, Ignore, NumIgnore);
				// A collision is resolved just before contact; a brake edge is crossed just after it.
				const double Dt = Seg >= 0 ? Lo : Hi;
				Move(A, Dt);
				Remaining -= Dt;
				if (Seg >= 0) { Collide(BodyIndex, Seg, Ignore, NumIgnore); }
				else { MaintainContact(B); }
				CheckZones(BodyIndex);
				continue;
			}

			Move(A, Tau);
			Remaining -= Tau;
			if (bStop) { B.V = FVec2(); }
			MaintainContact(B);
			CheckZones(BodyIndex);
		}
	}

	bool FSim::EventAt(const FBody& B, const FVec2& A, double S, unsigned Mask, const int* Ignore, int NumIgnore) const
	{
		const FVec2 P = B.P + B.V * S + A * (0.5 * S * S);
		for (int I = 0; I < (int)Segs.size(); ++I)
		{
			if (I == B.Contact || Ignored(I, Ignore, NumIgnore)) { continue; }
			if (SegDistance(Segs[I], P) < B.R - kPen) { return true; }
		}
		return B.Contact >= 0 && BrakeMask(P) != Mask;
	}

	int FSim::DeepestPenetration(const FBody& B, const int* Ignore, int NumIgnore) const
	{
		int Best = -1;
		double BestDepth = -kPen;
		for (int I = 0; I < (int)Segs.size(); ++I)
		{
			if (I == B.Contact || Ignored(I, Ignore, NumIgnore)) { continue; }
			const double Depth = SegDistance(Segs[I], B.P) - B.R;
			if (Depth < BestDepth) { BestDepth = Depth; Best = I; }
		}
		return Best;
	}

	FVec2 FSim::ContactNormal(const FSeg& S, const FVec2& P) const
	{
		FVec2 N = S.T.Perp();
		if (N.Dot(P - S.A) < 0) { N = -N; }
		return N;
	}

	void FSim::Collide(int BodyIndex, int SegIndex, int* Ignore, int& NumIgnore)
	{
		FBody& B = Bodies[BodyIndex];
		const FSeg& S = Segs[SegIndex];
		double T;
		FVec2 C;
		const double Dist = SegDistance(S, B.P, T, C);
		const FVec2 Nrm = Dist > 0 ? (B.P - C) / Dist : ContactNormal(S, B.P);
		const double VN = B.V.Dot(Nrm);
		if (VN >= 0)
		{
			if (NumIgnore < kMaxIgnore) { Ignore[NumIgnore++] = SegIndex; }
			return;
		}

		// Track joints (ramp foot onto the floor): the surface turns and the body follows it at the
		// same speed, as on a smoothly curved track, so energy is conserved as in the textbook.
		if (B.Contact >= 0 && IsSmoothJoint(B.Contact, SegIndex))
		{
			double Dir = Sign(B.V.Dot(S.T));
			if (Dir == 0) { Dir = Sign(Gravity.Dot(S.T)); }
			if (Dir == 0) { Dir = 1; }
			B.V = S.T * (Dir * B.V.Length());
			B.Contact = SegIndex;
			B.N = ContactNormal(S, B.P);
			return;
		}

		// Impact: restitution along the normal, Coulomb friction impulse along the surface.
		const double E = S.Restitution;
		const double J = -(1 + E) * VN;
		FVec2 VT = B.V - Nrm * VN;
		const double VTL = VT.Length();
		if (VTL > 0)
		{
			const double Fr = Min(VTL, S.Friction * J);
			VT = VT * ((VTL - Fr) / VTL);
		}
		const double Out = -E * VN;
		const FVec2 SegN = ContactNormal(S, B.P);
		if (-VN >= kQuietLand && !B.bLanded)
		{
			B.bLanded = true;
			B.LandP = B.P;
			B.LandTime = Time + SubTime;
		}
		if (Out < kLandSpeed && T > 0 && T < 1 && Gravity.Dot(SegN) < 0)
		{
			B.V = VT - SegN * VT.Dot(SegN);
			B.Contact = SegIndex;
			B.N = SegN;
			if (-VN >= kQuietLand) { Emit(ESimEvent::Land, BodyIndex, B.P, -VN); }
		}
		else
		{
			B.V = VT + Nrm * Out;
			if (B.Contact >= 0 && B.V.Dot(B.N) > kVelEps) { B.Contact = -1; }
			Emit(ESimEvent::Bounce, BodyIndex, B.P, -VN);
		}
	}

	void FSim::MaintainContact(FBody& B)
	{
		if (B.Contact < 0) { return; }
		const FSeg& S = Segs[B.Contact];
		const double Along = (B.P - S.A).Dot(S.T);
		if (Along < 0 || Along > S.Len)
		{
			// Past the end of the surface: it flies (and lands on whatever comes next).
			B.Contact = -1;
			if (!B.bExited) { B.bExited = true; B.ExitSpeed = B.V.Length(); }
			return;
		}
		// Stay exactly on the surface (removes round-off drift).
		const double Dn = (B.P - S.A).Dot(B.N);
		B.P += B.N * (B.R - Dn);
		B.V -= B.N * B.V.Dot(B.N);
	}

	bool FSim::IsSmoothJoint(int SegA, int SegB) const
	{
		const FSeg& A = Segs[SegA];
		const FSeg& B = Segs[SegB];
		if (Abs(A.T.Dot(B.T)) < 0.5) { return false; }   // turns by more than 60 degrees: a wall, not a track
		return SegDistance(B, A.A) <= kJointGap || SegDistance(B, A.B) <= kJointGap
			|| SegDistance(A, B.A) <= kJointGap || SegDistance(A, B.B) <= kJointGap;
	}

	double FSim::BrakeDecel(const FVec2& P) const
	{
		double D = 0;
		for (const FBrakeRegion& R : Brakes) { if (R.Contains(P)) { D += R.Decel; } }
		return D;
	}

	unsigned FSim::BrakeMask(const FVec2& P) const
	{
		unsigned M = 0;
		for (int I = 0; I < (int)Brakes.size(); ++I) { if (Brakes[I].Contains(P)) { M |= 1u << I; } }
		return M;
	}

	void FSim::CheckZones(int BodyIndex)
	{
		FBody& B = Bodies[BodyIndex];
		for (int G = 0; G < (int)GoalDone.size(); ++G)
		{
			if (GoalDone[G] || GoalBody[G] != BodyIndex || Level->Goals[G].Type != EGoalType::Enter) { continue; }
			if (Level->Zones[GoalZone[G]].Contains(B.P))
			{
				GoalDone[G] = 1;
				if (B.GoalTime < 0) { B.GoalTime = Time + SubTime; B.GoalSpeed = B.V.Length(); }
			}
		}
		if (PendingFail < 0 && Outcome == EOutcome::Running)
		{
			for (int F = 0; F < (int)FailBody.size(); ++F)
			{
				if (FailBody[F] == BodyIndex && Level->Zones[FailZone[F]].Contains(B.P)) { PendingFail = F; break; }
			}
		}
		const FVec2& Lo = Level->BoundsMin;
		const FVec2& Hi = Level->BoundsMax;
		if (!B.bGone && (B.P.X < Lo.X || B.P.X > Hi.X || B.P.Y < Lo.Y || B.P.Y > Hi.Y))
		{
			B.bGone = true;
			B.V = FVec2();
			Emit(ESimEvent::Lost, BodyIndex, B.P);
		}
	}

	void FSim::Emit(ESimEvent Type, int Body, const FVec2& Pos, double Strength)
	{
		// Fixed capacity: the sim loop never allocates.
		if (Events.size() >= Events.capacity()) { return; }
		FSimEvent E;
		E.Type = Type;
		E.Body = Body;
		E.Pos = Pos;
		E.Strength = Strength;
		Events.push_back(E);
	}

	void FSim::Finish(EOutcome Result, const std::string* Text)
	{
		Outcome = Result;
		FailText = Text;
		Emit(Result == EOutcome::Success ? ESimEvent::Goal : ESimEvent::Fail, -1, FVec2());
	}

	// ---------------------------------------------------------------- measurements

	bool FSim::Measure(const std::string& Name, double& Out) const
	{
		if (Name == "time") { Out = Time; return true; }
		if (Name == "g") { Out = Level ? Level->Gravity : 9.81; return true; }
		const size_t Dot = Name.find('.');
		if (Dot == std::string::npos || !Level) { return false; }
		const int I = Level->FindBody(Name.substr(0, Dot));
		if (I < 0 || I >= (int)Bodies.size()) { return false; }
		const FBody& B = Bodies[I];
		const std::string F = Name.substr(Dot + 1);
		if (F == "x") { Out = B.P.X; return true; }
		if (F == "y") { Out = B.P.Y; return true; }
		if (F == "speed") { Out = B.V.Length(); return true; }
		if (F == "distance") { Out = B.Distance; return true; }
		if (F == "travelX") { Out = B.P.X - B.StartP.X; return true; }
		if (F == "maxSpeed") { Out = B.MaxSpeed; return true; }
		if (F == "landX") { Out = B.bLanded ? B.LandP.X : B.P.X; return true; }
		if (F == "landTime") { Out = B.bLanded ? B.LandTime : Time; return true; }
		if (F == "goalTime") { Out = B.GoalTime; return true; }
		if (F == "goalSpeed") { Out = B.GoalSpeed; return true; }
		if (F == "startY") { Out = B.StartP.Y; return true; }
		if (F == "exitSpeed") { Out = B.ExitSpeed; return true; }
		return false;
	}

	bool FSim::Variable(const std::string& Name, double& Out) const
	{
		if (Measure(Name, Out)) { return true; }
		const size_t Dot = Name.find('.');
		const std::string PartId = Dot == std::string::npos ? std::string() : Name.substr(0, Dot);
		const std::string ParamId = Dot == std::string::npos ? Name : Name.substr(Dot + 1);
		for (const FPartInstance& Inst : Parts)
		{
			if (!PartId.empty() && Inst.Part->Id != PartId) { continue; }
			const int I = Inst.Part->ParamIndex(ParamId);
			if (I >= 0 && I < (int)Inst.Values.size()) { Out = Inst.Values[I]; return true; }
		}
		return false;
	}
}
