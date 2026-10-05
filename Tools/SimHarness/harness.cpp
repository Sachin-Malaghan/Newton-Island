// CURIO ISLES: standalone check of the simulation core outside Unreal. (CLAUDE.md: Testing)
// Build + run: powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1 [-Level <id>] [-Trace] [-Fast]
//   (no args)        physics checks, every level (solver, determinism, texts), zero allocations while stepping
//   -level <id>      only levels whose id contains <id>; prints the solver's example setup
//   -trace           print the example run step by step (every 0.1 s)
//   -fast            coarser solver sampling
//   -verbose         print every check's numbers
#include "../../Source/CurioIsles/Private/Core/CIChecks.h"
#include "../../Source/CurioIsles/Private/Core/CIExpr.h"
#include "../../Source/CurioIsles/Private/Core/CISolver.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

// Bytes ever allocated on the CRT heap (debug CRT statistics; run.ps1 builds with /MDd). The sim loop
// must not allocate. (Replacing global operator new instead gets the exe quarantined by the antivirus.)
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
static long long HeapTotal() { _CrtMemState S; _CrtMemCheckpoint(&S); return (long long)S.lTotalCount; }
#else
static long long HeapTotal() { return 0; }
#endif

using namespace CI;

static std::string g_Root;

static bool ReadFile(const std::string& Rel, std::string& Out)
{
	std::ifstream F(g_Root + "/" + Rel, std::ios::binary);
	if (!F) { return false; }
	std::stringstream S;
	S << F.rdbuf();
	Out = S.str();
	return true;
}

static const char* OutcomeName(EOutcome O)
{
	return O == EOutcome::Success ? "SUCCESS" : (O == EOutcome::Fail ? "fail" : "running");
}

static void PrintSetup(const FLevelDef& L, const FSetup& S)
{
	for (const FPlacedPart& P : S.Placed)
	{
		std::printf("    %s in slot '%s':", L.Tray[P.Tray].Part.c_str(), L.Slots[P.Slot].Id.c_str());
		for (size_t K = 0; K < P.Values.size(); ++K) { std::printf(" %s=%g", L.Tray[P.Tray].Params[K].Id.c_str(), P.Values[K]); }
		std::printf("\n");
	}
	for (size_t F = 0; F < S.FixedValues.size(); ++F)
	{
		std::printf("    fixed %s:", L.Fixed[F].Part.c_str());
		for (double V : S.FixedValues[F]) { std::printf(" %g", V); }
		std::printf("\n");
	}
}

int main(int Argc, char** Argv)
{
	std::string Only;
	bool bTrace = false, bFast = false, bVerbose = false;
	for (int I = 1; I < Argc; ++I)
	{
		if (!std::strcmp(Argv[I], "-level") && I + 1 < Argc) { Only = Argv[++I]; }
		else if (!std::strcmp(Argv[I], "-trace")) { bTrace = true; }
		else if (!std::strcmp(Argv[I], "-fast")) { bFast = true; }
		else if (!std::strcmp(Argv[I], "-verbose")) { bVerbose = true; }
		else if (!std::strcmp(Argv[I], "-root") && I + 1 < Argc) { g_Root = Argv[++I]; }
	}
	if (g_Root.empty()) { g_Root = "Content/Islands"; }

	int Failures = 0;
	auto Report = [&](const std::vector<FCheck>& Checks)
	{
		for (const FCheck& C : Checks)
		{
			std::printf("%s  %s\n", C.bPass ? "PASS" : "FAIL", C.Name.c_str());
			if (!C.bPass || !Only.empty() || bVerbose)
			{
				std::istringstream Lines(C.Detail);
				std::string Line;
				while (std::getline(Lines, Line)) { std::printf("        %s\n", Line.c_str()); }
			}
			if (!C.bPass) { ++Failures; }
		}
	};

	const auto T0 = std::chrono::steady_clock::now();
	if (Only.empty())
	{
		std::vector<FCheck> Physics;
		RunPhysicsChecks(Physics);
		Report(Physics);
	}

	FIslandDef Island;
	std::string Err;
	if (!LoadIsland("Newton", ReadFile, Island, Err))
	{
		std::printf("FAIL  load island: %s\n", Err.c_str());
		return 1;
	}

	FIslandDef Filtered = Island;
	for (FWorldDef& W : Filtered.Worlds)
	{
		std::vector<FLevelDef> Keep;
		for (const FLevelDef& L : W.Levels) { if (Only.empty() || L.Id.find(Only) != std::string::npos) { Keep.push_back(L); } }
		W.Levels = Keep;
	}
	std::vector<FCheck> Levels;
	RunIslandChecks(Filtered, Levels, bFast);
	Report(Levels);

	// Zero allocations while stepping, and the example setups (printed on request).
	for (const FWorldDef& W : Filtered.Worlds)
	{
		for (const FLevelDef& L : W.Levels)
		{
			const FSolveReport R = SolveLevel(L, Island.Catalog, bFast ? 6000 : 60000);
			FSim S;
			S.Load(L, Island.Catalog, R.Example);
			const long long Before = HeapTotal();
			while (S.Outcome == EOutcome::Running && S.Time < L.TimeLimit + 1) { S.Step(); S.Events.clear(); }
			const long long Allocated = HeapTotal() - Before;
			const bool bOk = Allocated == 0;
			std::printf("%s  Level.%s.NoAllocations  (%lld bytes allocated in %d steps)\n", bOk ? "PASS" : "FAIL", L.Id.c_str(), Allocated, S.StepCount);
			if (!bOk) { ++Failures; }

			if (!Only.empty())
			{
				std::printf("  example setup (%s at %.3f s):\n", OutcomeName(S.Outcome), S.Time);
				PrintSetup(L, R.Example);
			}
			if (bTrace)
			{
				FSim T;
				T.Load(L, Island.Catalog, R.Example);
				int Next = 0;
				while (T.Outcome == EOutcome::Running && T.Time < L.TimeLimit + 1)
				{
					if (T.StepCount >= Next)
					{
						std::printf("  t=%.3f", T.Time);
						for (const FBody& B : T.Bodies) { std::printf("  p=(%.4f, %.4f) v=(%.4f, %.4f) c=%d%s", B.P.X, B.P.Y, B.V.X, B.V.Y, B.Contact, B.bResting ? " rest" : ""); }
						std::printf("\n");
						Next += 12;
					}
					T.Step();
					for (const FSimEvent& E : T.Events)
					{
						static const char* Names[] = { "launch", "bounce", "land", "rest", "goal", "fail", "lost" };
						std::printf("    event %s at (%.3f, %.3f) strength %.3f\n", Names[(int)E.Type], E.Pos.X, E.Pos.Y, E.Strength);
					}
					T.Events.clear();
				}
				std::printf("  end: %s at %.3f s%s%s\n", OutcomeName(T.Outcome), T.Time, T.FailText ? " - " : "", T.FailText ? T.FailText->c_str() : "");
			}
		}
	}

	const double Secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count();
	std::printf("\n%s (%d failure%s, %.1f s)\n", Failures ? "FAILED" : "ALL PASSED", Failures, Failures == 1 ? "" : "s", Secs);
	return Failures ? 1 : 0;
}
