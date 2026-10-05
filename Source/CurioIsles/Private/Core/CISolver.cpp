// CURIO ISLES: the level solver - proves every level can be solved within par. (CLAUDE.md: Level validation)
#include "CISolver.h"

namespace CI
{
	bool RunSetup(const FLevelDef& Level, const FPartCatalog& Catalog, const FSetup& Setup, FSim* OutSim)
	{
		FSim Local;
		FSim& S = OutSim ? *OutSim : Local;
		if (!S.Load(Level, Catalog, Setup)) { return false; }
		return S.Run(Level.TimeLimit + 1.0) == EOutcome::Success;
	}

	namespace
	{
		struct FKnob
		{
			const FParamDef* Param;
			int Placed;     // index into FSetup::Placed, or -1 for a tunable fixed part
			int Fixed;      // index into FSetup::FixedValues
			int Value;      // index into that part's values
		};

		void EnumerateAssignments(const FLevelDef& L, const std::vector<int>& Inst, const std::vector<int>& SlotFree,
		                          size_t Index, std::vector<int>& Current, std::vector<std::vector<int>>& Out)
		{
			if (Index == Inst.size()) { Out.push_back(Current); return; }
			Current[Index] = -1;
			EnumerateAssignments(L, Inst, SlotFree, Index + 1, Current, Out);
			for (int S = 0; S < (int)L.Slots.size(); ++S)
			{
				if (!SlotFree[S] || !L.Slots[S].AcceptsPart(L.Tray[Inst[Index]].Part)) { continue; }
				bool bTaken = false;
				for (size_t K = 0; K < Index; ++K)
				{
					if (Current[K] == S) { bTaken = true; }
					// Two copies of the same part are interchangeable: only try them in slot order.
					if (Inst[K] == Inst[Index] && Current[K] >= 0 && Current[K] > S) { bTaken = true; }
				}
				if (bTaken) { continue; }
				Current[Index] = S;
				EnumerateAssignments(L, Inst, SlotFree, Index + 1, Current, Out);
			}
			Current[Index] = -1;
		}
	}

	FSolveReport SolveLevel(const FLevelDef& L, const FPartCatalog& Catalog, long long MaxRuns)
	{
		FSolveReport R;
		std::vector<int> Inst;
		for (int T = 0; T < (int)L.Tray.size(); ++T) { for (int C = 0; C < L.Tray[T].Count; ++C) { Inst.push_back(T); } }
		std::vector<int> SlotFree(L.Slots.size(), 1);
		for (const FFixedPart& F : L.Fixed) { const int S = L.FindSlot(F.Slot); if (S >= 0) { SlotFree[S] = 0; } }

		std::vector<std::vector<int>> Assignments;
		std::vector<int> Current(Inst.size(), -1);
		EnumerateAssignments(L, Inst, SlotFree, 0, Current, Assignments);
		const long long PerAssignment = MaxRuns / (long long)(Assignments.empty() ? 1 : Assignments.size()) + 1;

		double BestDistance = 1e300;
		FSim Sim;
		for (const std::vector<int>& A : Assignments)
		{
			FSetup Base = FSetup::Defaults(L);
			for (size_t K = 0; K < A.size(); ++K)
			{
				if (A[K] < 0) { continue; }
				FPlacedPart P;
				P.Tray = Inst[K];
				P.Slot = A[K];
				for (const FParamDef& Q : L.Tray[Inst[K]].Params) { P.Values.push_back(Q.Default); }
				Base.Placed.push_back(P);
			}
			const int NumParts = Base.NumPlaced();

			std::vector<FKnob> Knobs;
			for (int P = 0; P < (int)Base.Placed.size(); ++P)
			{
				const FTrayItem& T = L.Tray[Base.Placed[P].Tray];
				for (int V = 0; V < (int)T.Params.size(); ++V) { Knobs.push_back({ &T.Params[V], P, -1, V }); }
			}
			for (int F = 0; F < (int)L.Fixed.size(); ++F)
			{
				if (!L.Fixed[F].bTunable) { continue; }
				for (int V = 0; V < (int)L.Fixed[F].Params.size(); ++V) { Knobs.push_back({ &L.Fixed[F].Params[V], -1, F, V }); }
			}

			// Untouched sliders first.
			const bool bDefaultWin = RunSetup(L, Catalog, Base, &Sim);
			if (bDefaultWin)
			{
				if (NumParts == 0) { R.bEmptySolves = true; }
				else { R.bDefaultsSolve = true; }
			}

			long long Total = 1;
			for (const FKnob& K : Knobs) { Total *= K.Param->NumValues(); if (Total > (1LL << 40)) { break; } }
			const long long Stride = Total > PerAssignment ? (Total + PerAssignment - 1) / PerAssignment : 1;

			for (long long Code = 0; Code < Total; Code += Stride)
			{
				FSetup S = Base;
				long long Rest = Code;
				double Distance = 0;
				for (const FKnob& K : Knobs)
				{
					const int N = K.Param->NumValues();
					const int Idx = (int)(Rest % N);
					Rest /= N;
					const double V = K.Param->ValueAt(Idx);
					if (K.Placed >= 0) { S.Placed[K.Placed].Values[K.Value] = V; }
					else { S.FixedValues[K.Fixed][K.Value] = V; }
					const double Span = K.Param->Max - K.Param->Min;
					Distance += Span > 0 ? Abs(V - K.Param->Default) / Span : 0;
				}
				++R.Tried;
				if (!RunSetup(L, Catalog, S, &Sim)) { continue; }
				++R.Solved;
				if (R.MinParts < 0 || NumParts < R.MinParts || (NumParts == R.MinParts && Distance < BestDistance))
				{
					R.MinParts = NumParts;
					BestDistance = Distance;
					R.Example = S;
				}
			}
		}
		return R;
	}
}
