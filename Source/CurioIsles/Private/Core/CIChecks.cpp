// CURIO ISLES: physics and content checks shared by the harness and the Unreal automation tests. (CLAUDE.md: Testing)
#include "CIChecks.h"

#include "CIExpr.h"
#include "CISolver.h"

#include <cstring>

namespace CI
{
	namespace
	{
		// A catalog for synthetic test levels: the launcher fires from ground level (its muzzle sits one
		// ball radius up, so the textbook range formula applies exactly), long ramps and brakes.
		const char* TestCatalog = R"({ "parts": [
			{ "id": "launcher", "behavior": "launcher",
			  "params": [ { "id": "angle", "min": 1, "max": 89, "step": 1, "default": 45 },
			              { "id": "speed", "min": 1, "max": 40, "step": 0.1, "default": 10 } ],
			  "props": { "pivotHeight": 0.1, "barrel": 0 } },
			{ "id": "ramp", "behavior": "ramp",
			  "params": [ { "id": "height", "min": 0.1, "max": 5, "step": 0.01, "default": 1 } ],
			  "props": { "length": 3, "friction": 0 } },
			{ "id": "brake", "behavior": "brake",
			  "params": [ { "id": "strength", "min": 0.1, "max": 10, "step": 0.1, "default": 2 } ],
			  "props": { "length": 100 } } ] })";

		// Flat frictionless ground and one body; Extra adds slots/fixed parts. The goal is unreachable,
		// so runs end by landing/stopping, which is what the checks measure.
		std::string FlatLevel(const std::string& Body, const std::string& Extra)
		{
			return std::string(R"({ "id": "test", "view": [-2, -1, 300, 100], "bounds": [-10, -10, 1000, 1000], "timeLimit": 60,
				"surfaces": [ { "points": [[-5, 0], [900, 0]], "friction": 0, "restitution": 0 } ],
				"zones": [ { "id": "never", "rect": [5000, 5000, 5001, 5001] } ],
				"bodies": [ )") + Body + " ], " + Extra + R"(,
				"goals": [ { "type": "enter", "body": "ball", "zone": "never" } ] })";
		}

		bool Build(const std::string& CatalogText, const std::string& LevelText, FPartCatalog& Catalog, FLevelDef& Level, std::string& Err)
		{
			FJson CJ, LJ;
			if (!FJson::Parse(CatalogText, CJ, Err) || !ParseCatalog(CJ, Catalog, Err)) { return false; }
			if (!FJson::Parse(LevelText, LJ, Err)) { return false; }
			return ParseLevel(LJ, Catalog, Level, Err);
		}

		void Add(std::vector<FCheck>& Out, const std::string& Name, bool bPass, const std::string& Detail)
		{
			FCheck C;
			C.Name = Name;
			C.bPass = bPass;
			C.Detail = Detail;
			Out.push_back(C);
		}

		std::string Num(double V, int D = 4) { return FormatNumber(V, D); }

		unsigned long long Fnv(unsigned long long H, const void* Data, size_t Size)
		{
			const unsigned char* P = (const unsigned char*)Data;
			for (size_t I = 0; I < Size; ++I) { H ^= P[I]; H *= 1099511628211ULL; }
			return H;
		}

		// Runs the sim until the ball has landed (or 60 s).
		void RunUntilLanded(FSim& S)
		{
			while (!S.Bodies[0].bLanded && S.Time < 60) { S.Step(); }
		}
	}

	unsigned long long RunFingerprint(const FLevelDef& Level, const FPartCatalog& Catalog, const FSetup& Setup)
	{
		FSim S;
		unsigned long long H = 1469598103934665603ULL;
		if (!S.Load(Level, Catalog, Setup)) { return 0; }
		while (S.Outcome == EOutcome::Running && S.Time < Level.TimeLimit + 1)
		{
			S.Step();
			for (const FBody& B : S.Bodies)
			{
				H = Fnv(H, &B.P.X, sizeof(double));
				H = Fnv(H, &B.P.Y, sizeof(double));
				H = Fnv(H, &B.V.X, sizeof(double));
				H = Fnv(H, &B.V.Y, sizeof(double));
			}
		}
		const int O = (int)S.Outcome;
		return Fnv(H, &O, sizeof(O));
	}

	void RunPhysicsChecks(std::vector<FCheck>& Out)
	{
		// ---- deterministic Sin/Cos agree with the C library
		{
			double Worst = 0;
			for (int I = -20000; I <= 20000; ++I)
			{
				const double X = I * 0.00251;
				Worst = Max(Worst, Abs(Sin(X) - std::sin(X)));
				Worst = Max(Worst, Abs(Cos(X) - std::cos(X)));
			}
			Add(Out, "Math.SinCos", Worst < 1e-14, "worst difference from the C library " + std::to_string(Worst));
		}

		FPartCatalog Catalog;
		std::string Err;

		// ---- projectile range = v^2 sin(2 theta) / g (acceptance: within 1% for 10 cases)
		{
			FLevelDef L;
			const bool bOk = Build(TestCatalog, FlatLevel(R"({ "id": "ball", "radius": 0.1, "pos": [0, 0.1], "on": { "part": "launcher", "anchor": "muzzle" } })",
				R"("slots": [ { "id": "a", "pos": [0, 0] } ], "fixed": [ { "part": "launcher", "slot": "a", "tunable": true } ])"), Catalog, L, Err);
			if (!bOk) { Add(Out, "Physics.ProjectileRange", false, Err); }
			else
			{
				const double Cases[10][2] = { { 15, 8 }, { 30, 10 }, { 45, 10 }, { 60, 12.5 }, { 75, 6 }, { 20, 20 }, { 40, 15.5 }, { 50, 9 }, { 10, 25 }, { 80, 18 } };
				double Worst = 0;
				std::string Detail;
				for (const auto& C : Cases)
				{
					FSetup Setup = FSetup::Defaults(L);
					Setup.FixedValues[0] = { C[0], C[1] };
					FSim S;
					S.Load(L, Catalog, Setup);
					RunUntilLanded(S);
					const double Range = S.Bodies[0].LandP.X - S.Bodies[0].StartP.X;
					const double Expected = C[1] * C[1] * std::sin(2 * C[0] * 3.14159265358979323846 / 180.0) / 9.81;
					const double Err2 = Abs(Range - Expected) / Expected;
					Worst = Max(Worst, Err2);
					Detail += "  " + Num(C[0], 0) + "deg " + Num(C[1], 1) + "m/s: " + Num(Range) + " vs " + Num(Expected) + "\n";
				}
				Add(Out, "Physics.ProjectileRange", Worst < 0.01, "worst relative error " + std::to_string(Worst) + "\n" + Detail);
			}
		}

		// ---- free fall: t = sqrt(2h/g)
		{
			double Worst = 0;
			std::string Detail;
			for (double H : { 1.0, 5.0, 20.0 })
			{
				FLevelDef L;
				FPartCatalog C2;
				if (!Build(TestCatalog, FlatLevel(R"({ "id": "ball", "radius": 0.1, "pos": [0, )" + Num(H + 0.1, 6) + "] }", R"("slots": [])"), C2, L, Err)) { Worst = 1; Detail = Err; break; }
				FSim S;
				S.Load(L, C2, FSetup::Defaults(L));
				RunUntilLanded(S);
				const double Expected = std::sqrt(2 * H / 9.81);
				Worst = Max(Worst, Abs(S.Bodies[0].LandTime - Expected) / Expected);
				Detail += "  h=" + Num(H, 1) + ": " + Num(S.Bodies[0].LandTime, 6) + " s vs " + Num(Expected, 6) + "\n";
			}
			Add(Out, "Physics.FreeFall", Worst < 1e-6, "worst relative error " + std::to_string(Worst) + "\n" + Detail);
		}

		// ---- a frictionless ramp: v = sqrt(2gh) at the bottom, then constant speed on the flat
		{
			FLevelDef L;
			FPartCatalog C2;
			const bool bOk = Build(TestCatalog, FlatLevel(R"({ "id": "ball", "radius": 0.1, "pos": [-3, 0.1], "on": { "part": "ramp", "anchor": "top" } })",
				R"("slots": [ { "id": "a", "pos": [10, 0] } ], "fixed": [ { "part": "ramp", "slot": "a", "tunable": true } ])"), C2, L, Err);
			double Worst = 0;
			std::string Detail = bOk ? "" : Err;
			for (double H : { 0.5, 1.0, 2.0, 4.0 })
			{
				if (!bOk) { Worst = 1; break; }
				FSetup Setup = FSetup::Defaults(L);
				Setup.FixedValues[0] = { H };
				FSim S;
				S.Load(L, C2, Setup);
				while (S.Bodies[0].Contact != 0 && S.Time < 20) { S.Step(); }
				const double Drop = S.Bodies[0].StartP.Y - S.Bodies[0].P.Y;
				const double Expected = std::sqrt(2 * 9.81 * Drop);
				const double AtBottom = S.Bodies[0].V.Length();
				for (int K = 0; K < 240; ++K) { S.Step(); }
				const double Later = S.Bodies[0].V.Length();
				Worst = Max(Worst, Max(Abs(AtBottom - Expected) / Expected, Abs(Later - AtBottom) / AtBottom));
				Detail += "  h=" + Num(H, 2) + ": " + Num(AtBottom) + " m/s vs " + Num(Expected) + ", 2 s later " + Num(Later) + "\n";
			}
			Add(Out, "Physics.RampSpeed", Worst < 0.005, "worst relative error " + std::to_string(Worst) + "\n" + Detail);
		}

		// ---- braking: stopping distance s = v^2 / 2a
		{
			FLevelDef L;
			FPartCatalog C2;
			const bool bOk = Build(TestCatalog, FlatLevel(R"({ "id": "ball", "kind": "cart", "radius": 0.3, "pos": [0, 0.3], "vel": [5, 0] })",
				R"("slots": [ { "id": "a", "pos": [2, 0] } ], "fixed": [ { "part": "brake", "slot": "a", "tunable": true } ])"), C2, L, Err);
			double Worst = 0;
			std::string Detail = bOk ? "" : Err;
			for (double A : { 1.0, 2.0, 2.5, 4.0 })
			{
				if (!bOk) { Worst = 1; break; }
				FSetup Setup = FSetup::Defaults(L);
				Setup.FixedValues[0] = { A };
				FSim S;
				S.Load(L, C2, Setup);
				S.Run(40);
				const double Stop = S.Bodies[0].P.X - 2.0;
				const double Expected = 25.0 / (2 * A);
				Worst = Max(Worst, Abs(Stop - Expected) / Expected + (S.Bodies[0].bResting ? 0 : 1));
				Detail += "  a=" + Num(A, 1) + ": stops " + Num(Stop) + " m into the pad vs " + Num(Expected) + (S.Bodies[0].bResting ? "" : " (NOT AT REST)") + "\n";
			}
			Add(Out, "Physics.BrakingDistance", Worst < 0.01, "worst relative error " + std::to_string(Worst) + "\n" + Detail);
		}

		// ---- same inputs, same result: 100 runs bit-identical (acceptance criterion)
		{
			FLevelDef L;
			FPartCatalog C2;
			Build(TestCatalog, FlatLevel(R"({ "id": "ball", "radius": 0.1, "pos": [0, 0.1], "on": { "part": "launcher", "anchor": "muzzle" } })",
				R"("slots": [ { "id": "a", "pos": [0, 0] } ], "fixed": [ { "part": "launcher", "slot": "a", "tunable": true } ])"), C2, L, Err);
			FSetup Setup = FSetup::Defaults(L);
			Setup.FixedValues[0] = { 37, 13.3 };
			const unsigned long long First = RunFingerprint(L, C2, Setup);
			int Same = 0;
			for (int I = 0; I < 100; ++I) { if (RunFingerprint(L, C2, Setup) == First) { ++Same; } }
			Add(Out, "Physics.Deterministic100", Same == 100 && First != 0, std::to_string(Same) + "/100 runs identical");
		}
	}

	void RunIslandChecks(const FIslandDef& Island, std::vector<FCheck>& Out, bool bFast)
	{
		for (const FWorldDef& W : Island.Worlds)
		{
			for (const FLevelDef& L : W.Levels)
			{
				const std::string Tag = "Level." + L.Id;
				const FSolveReport R = SolveLevel(L, Island.Catalog, bFast ? 6000 : 60000);
				Add(Out, Tag + ".Solvable", R.Solved > 0 && R.MinParts >= 0 && R.MinParts <= L.Par,
					std::to_string(R.Solved) + " of " + std::to_string(R.Tried) + " setups win (" + Num(R.WinFraction() * 100, 1) + "%), fewest parts " + std::to_string(R.MinParts) + ", par " + std::to_string(L.Par));
				Add(Out, Tag + ".NotTrivial", !R.bEmptySolves && !R.bDefaultsSolve,
					R.bEmptySolves ? "wins with nothing placed" : (R.bDefaultsSolve ? "wins with untouched sliders" : "untouched sliders do not win"));
				if (R.Solved == 0) { continue; }

				const unsigned long long First = RunFingerprint(L, Island.Catalog, R.Example);
				int Same = 0;
				for (int I = 0; I < 100; ++I) { if (RunFingerprint(L, Island.Catalog, R.Example) == First) { ++Same; } }
				Add(Out, Tag + ".Deterministic100", Same == 100, std::to_string(Same) + "/100 runs identical");

				FSim S;
				RunSetup(L, Island.Catalog, R.Example, &S);
				const FVarLookup Vars = [&S](const std::string& N, double& V) { return S.Variable(N, V); };
				std::string Texts = FormatTemplate(L.Example, Vars) + " | " + FormatTemplate(L.Why, Vars) + " | " + FormatTemplate(L.WhyStudent, Vars);
				Add(Out, Tag + ".TextsRender", Texts.find("[?]") == std::string::npos, Texts);
			}
		}
	}
}
