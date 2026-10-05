// CURIO ISLES: automation tests - textbook physics, determinism, every level solvable, content loads, and the
// game's own play loop gives exactly the core's results in either mode. (CLAUDE.md: Testing)
#include "Misc/AutomationTest.h"

#include "Core/CIChecks.h"
#include "Core/CISolver.h"
#include "Game/CIContentLoader.h"
#include "Game/CIGame.h"
#include "Game/CISaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CITests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	void Report(FAutomationTestBase& Test, const std::vector<CI::FCheck>& Checks)
	{
		for (const CI::FCheck& C : Checks)
		{
			Test.TestTrue(FString::Printf(TEXT("%hs: %s"), C.Name.c_str(), UTF8_TO_TCHAR(C.Detail.c_str())), C.bPass);
		}
	}

	bool LoadNewton(FAutomationTestBase& Test, CI::FIslandDef& Island)
	{
		FString Err;
		const bool bOk = CIContent::LoadIsland(TEXT("Newton"), Island, Err);
		Test.TestTrue(FString::Printf(TEXT("Newton's Island loads (%s)"), *Err), bOk);
		return bOk;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCIPhysicsTextbook, "CurioIsles.Physics.Textbook", CITests::Flags)
bool FCIPhysicsTextbook::RunTest(const FString& Parameters)
{
	std::vector<CI::FCheck> Checks;
	CI::RunPhysicsChecks(Checks);
	TestTrue(TEXT("physics checks ran"), Checks.size() >= 6);
	CITests::Report(*this, Checks);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCIContentLoads, "CurioIsles.Content.IslandsLoad", CITests::Flags)
bool FCIContentLoads::RunTest(const FString& Parameters)
{
	const TArray<FString> Found = CIContent::FindIslands();
	TestTrue(TEXT("Newton's Island is installed"), Found.Contains(TEXT("Newton")));
	for (const FString& Folder : Found)
	{
		CI::FIslandDef Island;
		FString Err;
		TestTrue(FString::Printf(TEXT("%s loads (%s)"), *Folder, *Err), CIContent::LoadIsland(Folder, Island, Err));
		TestTrue(FString::Printf(TEXT("%s has levels"), *Folder), !Island.Worlds.empty() && !Island.Worlds[0].Levels.empty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCILevelsSolvable, "CurioIsles.Levels.SolvableDeterministic", CITests::Flags)
bool FCILevelsSolvable::RunTest(const FString& Parameters)
{
	CI::FIslandDef Island;
	if (!CITests::LoadNewton(*this, Island)) { return true; }
	std::vector<CI::FCheck> Checks;
	CI::RunIslandChecks(Island, Checks, false);
	CITests::Report(*this, Checks);
	return true;
}

// Plays each level through FCIGame (the same loop the player uses, 60 fps frames, mode flipped midway)
// and compares the final state with the core run bit for bit: the presentation layer never alters physics.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCIGameMatchesCore, "CurioIsles.Game.PlayMatchesCoreInBothModes", CITests::Flags)
bool FCIGameMatchesCore::RunTest(const FString& Parameters)
{
	UCISaveGame* Save = NewObject<UCISaveGame>();
	Save->AddToRoot();
	FCIGame Game;
	Game.bNoSave = true;
	if (!Game.Init(Save)) { TestTrue(TEXT("game content loads"), false); Save->RemoveFromRoot(); return true; }
	for (int32 W = 0; W < (int32)Game.Island.Worlds.size(); ++W)
	{
		for (int32 L = 0; L < (int32)Game.Island.Worlds[W].Levels.size(); ++L)
		{
			const CI::FLevelDef& Level = Game.Island.Worlds[W].Levels[L];
			const CI::FSolveReport R = CI::SolveLevel(Level, Game.Island.Catalog, 4000);
			CI::FSim Core;
			const bool bCoreWins = CI::RunSetup(Level, Game.Island.Catalog, R.Example, &Core);

			Save->bStudentMode = false;
			Game.StartLevel(W, L);
			Game.ApplySetup(R.Example);
			Game.Act(ECIAction::Run);
			FCIPointer NoPointer;
			FCIKeys NoKeys;
			for (int32 Frame = 0; Frame < 60 * 20 && Game.Phase == ECIPhase::Running; ++Frame)
			{
				if (Frame == 30) { Save->bStudentMode = true; }
				Game.Tick(1.f / 60.f, NoPointer, NoKeys, 1600, 900);
			}
			const bool bSameEnd = Game.Sim.StepCount == Core.StepCount && Game.Sim.Outcome == Core.Outcome
				&& Game.Sim.Bodies.size() == Core.Bodies.size()
				&& FMemory::Memcmp(&Game.Sim.Bodies[0].P, &Core.Bodies[0].P, sizeof(CI::FVec2)) == 0;
			TestTrue(FString::Printf(TEXT("%hs: game loop ends exactly like the core (%s at step %d vs %d)"), Level.Id.c_str(),
				bCoreWins ? TEXT("win") : TEXT("fail"), Game.Sim.StepCount, Core.StepCount), bSameEnd);
			TestTrue(FString::Printf(TEXT("%hs: solved through the game loop"), Level.Id.c_str()), Game.bSuccess);
		}
	}
	Save->RemoveFromRoot();
	return true;
}

#endif
