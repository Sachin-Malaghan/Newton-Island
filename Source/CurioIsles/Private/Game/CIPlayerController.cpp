// CURIO ISLES: input, lifecycle, console commands and the capture script. (CLAUDE.md: Game flow / Input)
#include "Game/CIPlayerController.h"

#include "Core/CISolver.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Game/CISaveGame.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogCurioIsles, Log, All);

namespace
{
	ACIPlayerController* FindController(UWorld* World)
	{
		return World ? Cast<ACIPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	// The solver's winning setup for the current level (hints, console, capture).
	bool SolvedSetup(const FCIGame& G, CI::FSetup& Out)
	{
		const CI::FLevelDef* L = G.CurrentLevel();
		if (!L) { return false; }
		const CI::FSolveReport R = CI::SolveLevel(*L, G.Island.Catalog, 4000);
		Out = R.Example;
		return R.Solved > 0;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdPlay(TEXT("ci.Play"), TEXT("ci.Play <world index> <level index>  start a level (0-based)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ACIPlayerController* PC = FindController(World))
			{
				PC->Game.StartLevel(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0, Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSolve(TEXT("ci.Solve"), TEXT("Put the solver's winning setup into the current level"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			CI::FSetup S;
			if (ACIPlayerController* PC = FindController(World); PC && SolvedSetup(PC->Game, S)) { PC->Game.ApplySetup(S, 0); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdMode(TEXT("ci.Mode"), TEXT("ci.Mode fun|student"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ACIPlayerController* PC = FindController(World); PC && PC->Save)
			{
				PC->Save->bStudentMode = Args.Num() > 0 && Args[0].Equals(TEXT("student"), ESearchCase::IgnoreCase);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdHideUI(TEXT("ci.HideUI"), TEXT("ci.HideUI 0|1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ACIPlayerController* PC = FindController(World)) { PC->Game.bHideUI = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0; }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdReset(TEXT("ci.ResetProgress"), TEXT("Forget stars and notebook (keeps settings)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ACIPlayerController* PC = FindController(World); PC && PC->Save)
			{
				PC->Save->Stars.Reset();
				PC->Save->Notebook.Reset();
				PC->Game.SaveProgress();
			}
		}));

	// Capture script: each step sets the game up, waits, then (if named) takes a screenshot.
	struct FCaptureStep
	{
		const TCHAR* Name;   // nullptr = no screenshot
		float Wait;
		TFunction<void(FCIGame&, UCISaveGame*)> Setup;
	};

	TArray<FCaptureStep> MakeCaptureScript()
	{
		auto Level = [](int32 W, int32 L, bool bStudent, int32 Solve) // Solve: 0 empty, 1 solved setup, 2 solved + panel open
		{
			return [=](FCIGame& G, UCISaveGame* S)
			{
				S->bStudentMode = bStudent;
				G.StartLevel(W, L);
				CI::FSetup Setup;
				if (Solve > 0 && SolvedSetup(G, Setup)) { G.ApplySetup(Setup, Solve == 2 ? 0 : -1); }
			};
		};
		auto Run = [](FCIGame& G, UCISaveGame*) { G.Act(ECIAction::Run); };
		return {
			{ TEXT("01_title"), 2.0f, [](FCIGame& G, UCISaveGame* S) { S->bStudentMode = false; G.GoTo(ECIScreen::Title); } },
			{ TEXT("02_levels"), 1.0f, [](FCIGame& G, UCISaveGame*) { G.GoTo(ECIScreen::Levels); } },
			{ TEXT("10_first_roll_build"), 1.0f, Level(0, 0, false, 0) },
			{ TEXT("11_first_roll_sliders"), 1.0f, Level(0, 0, false, 2) },
			{ nullptr, 0.1f, Level(0, 0, false, 1) },
			{ TEXT("12_first_roll_rolling"), 0.55f, Run },
			{ TEXT("13_first_roll_solved"), 4.4f, [](FCIGame&, UCISaveGame*) {} },
			{ TEXT("14_student_build"), 1.0f, Level(0, 0, true, 2) },
			{ nullptr, 0.1f, Level(0, 0, true, 1) },
			{ TEXT("15_student_running"), 0.62f, Run },
			{ TEXT("16_student_solved"), 4.4f, [](FCIGame&, UCISaveGame*) {} },
			{ TEXT("20_brake_build"), 1.0f, Level(0, 1, false, 0) },
			{ nullptr, 0.1f, [](FCIGame& G, UCISaveGame*)
				{
					CI::FSetup S = G.Setup;
					CI::FPlacedPart P;
					P.Tray = 0;
					P.Slot = 1;
					P.Values = { 0.5 };   // too soft: the cart rolls on into the pond
					S.Placed = { P };
					G.ApplySetup(S);
				} },
			{ TEXT("21_brake_splash"), 6.4f, Run },
			{ TEXT("22_brake_solved_student"), 0.1f, Level(0, 1, true, 1) },
			{ nullptr, 0.1f, Run },
			{ TEXT("23_brake_stopped_student"), 6.0f, [](FCIGame&, UCISaveGame*) {} },
			{ TEXT("30_canyon_build"), 1.0f, Level(1, 0, false, 2) },
			{ TEXT("30b_canyon_aiming"), 0.8f, [](FCIGame& G, UCISaveGame*)
				{
					// Hold the launcher pulled back, as a finger would.
					G.GripGrab = 0;
					G.GripKind = ECIGrip::LauncherAim;
					const FVector2D Pv = G.ToScreen(-1.0, 3.35);
					G.DragGrip(Pv + FVector2D(-G.ScreenH * 0.16, G.ScreenH * 0.13));
				} },
			{ nullptr, 0.05f, [](FCIGame& G, UCISaveGame*) { G.GripGrab = -1; } },
			{ TEXT("31_canyon_flight"), 0.75f, Run },
			{ TEXT("32_canyon_solved"), 3.2f, [](FCIGame&, UCISaveGame*) {} },
			{ TEXT("33_canyon_student"), 1.0f, Level(1, 0, true, 2) },
		};
	}
}

ACIPlayerController::ACIPlayerController()
{
	bShowMouseCursor = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ACIPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) { return; }

	ActivateTouchInterface(nullptr);

	Save = Cast<UCISaveGame>(UGameplayStatics::LoadGameFromSlot(UCISaveGame::SlotName, 0));
	if (!Save) { Save = Cast<UCISaveGame>(UGameplayStatics::CreateSaveGameObject(UCISaveGame::StaticClass())); }
	Game.Init(Save);

	// Nothing in the 3D world is drawn; the HUD paints everything.
	if (UGameViewportClient* VC = GetWorld()->GetGameViewport()) { VC->bDisableWorldRendering = true; }

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &ACIPlayerController::HandleBackground);
	DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &ACIPlayerController::HandleBackground);

	const TCHAR* Cmd = FCommandLine::Get();
	FString LevelArg;
	if (FParse::Value(Cmd, TEXT("CILevel="), LevelArg))
	{
		FString W, L;
		if (LevelArg.Split(TEXT("."), &W, &L)) { Game.StartLevel(FCString::Atoi(*W), FCString::Atoi(*L)); }
	}
	if (FParse::Param(Cmd, TEXT("CICapture")))
	{
		bCapture = true;
		Game.bNoSave = true;
		FParse::Value(Cmd, TEXT("CICaptureTag="), CaptureTag);
	}
}

void ACIPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundHandle);
	FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
	if (Save) { Game.SaveProgress(); }
	Super::EndPlay(Reason);
}

void ACIPlayerController::HandleBackground()
{
	Game.OnAppBackground();
}

bool ACIPlayerController::IsTouchDevice() const
{
#if PLATFORM_ANDROID || PLATFORM_IOS
	return true;
#else
	return false;
#endif
}

void ACIPlayerController::UpdateSafeArea()
{
	SafeAreaTimer -= 1.0;
	if (SafeAreaTimer > 0) { return; }
	SafeAreaTimer = 60;
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	const FVector4 P = Metrics.TitleSafePaddingSize;
	Game.Safe = FVector4(FMath::Max(0.0, (double)P.X), FMath::Max(0.0, (double)P.Y), FMath::Max(0.0, (double)P.Z), FMath::Max(0.0, (double)P.W));
}

void ACIPlayerController::GatherInput(FCIKeys& Keys)
{
	auto Pressed = [&](const FKey& K) { return WasInputKeyJustPressed(K); };
	Keys.Confirm = Pressed(EKeys::Enter) || Pressed(EKeys::SpaceBar) || Pressed(EKeys::Gamepad_FaceButton_Bottom);
	Keys.Back = Pressed(EKeys::Escape) || Pressed(EKeys::BackSpace) || Pressed(EKeys::Gamepad_FaceButton_Right) || Pressed(EKeys::Android_Back);
	Keys.Retry = Pressed(EKeys::R);
	Keys.ToggleMode = Pressed(EKeys::Tab) || Pressed(EKeys::M);
	Keys.Pause = Pressed(EKeys::P);

	// One pointer: the first finger on touch screens, else the mouse.
	double X = 0, Y = 0;
	bool bTouch = false;
	GetInputTouchState(ETouchIndex::Touch1, X, Y, bTouch);
	bool bDown = false;
	FVector2D Pos = Pointer.Pos;
	bool bValid = Pointer.bValid;
	if (bTouch)
	{
		Pos = FVector2D(X, Y);
		bDown = true;
		bValid = true;
		Pointer.bTouch = true;
	}
	else if (IsTouchDevice())
	{
		bDown = false;   // finger lifted: keep the last position for the release
	}
	else
	{
		double MX = 0, MY = 0;
		if (GetMousePosition(MX, MY))
		{
			Pos = FVector2D(MX, MY);
			bValid = true;
			Pointer.bTouch = false;
		}
		bDown = IsInputKeyDown(EKeys::LeftMouseButton);
	}
	Pointer.bValid = bValid;
	Pointer.Pos = Pos;
	Pointer.bPressed = bDown && !bWasDown;
	Pointer.bReleased = !bDown && bWasDown;
	Pointer.bDown = bDown;
	bWasDown = bDown;
}

void ACIPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!Save) { return; }

	FVector2D Size(1280, 720);
	if (UGameViewportClient* VC = GetWorld()->GetGameViewport()) { VC->GetViewportSize(Size); }
	UpdateSafeArea();

	FCIKeys Keys;
	GatherInput(Keys);
	if (bCapture)
	{
		Keys = FCIKeys();
		Pointer = FCIPointer();
		TickCapture(DeltaTime);
	}
	Game.Tick(DeltaTime, Pointer, Keys, Size.X, Size.Y);

	if (Game.bQuitRequested)
	{
		Game.bQuitRequested = false;
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
}

void ACIPlayerController::TickCapture(float DeltaTime)
{
	static const TArray<FCaptureStep> Script = MakeCaptureScript();
	const int32 NumSteps = Script.Num();
	CaptureClock += DeltaTime;
	if (CaptureStep >= NumSteps)
	{
		if (CaptureClock > 1.0 && CaptureStep == NumSteps)
		{
			++CaptureStep;
			ConsoleCommand(TEXT("quit"));
		}
		return;
	}
	if (CaptureStep < 0 && CaptureClock < 1.0) { return; }   // let the first frames settle
	if (CaptureStep >= 0 && CaptureClock < Script[CaptureStep].Wait) { return; }

	if (CaptureStep >= 0 && Script[CaptureStep].Name && !bCaptureShotTaken)
	{
		// Request the shot, then let this frame render before the next step changes anything.
		const FString Name = CaptureTag.IsEmpty() ? FString(Script[CaptureStep].Name) : CaptureTag + TEXT("_") + Script[CaptureStep].Name;
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / (TEXT("CurioIsles_") + Name + TEXT(".png")));
		FScreenshotRequest::RequestScreenshot(Path, true, false);
		UE_LOG(LogCurioIsles, Display, TEXT("CICapture: %s"), *Path);
		bCaptureShotTaken = true;
		return;
	}
	if (bCaptureShotTaken && CaptureClock < Script[CaptureStep].Wait + 0.3) { return; }
	bCaptureShotTaken = false;

	++CaptureStep;
	CaptureClock = 0;
	if (CaptureStep >= NumSteps) { return; }
	Script[CaptureStep].Setup(Game, Save);
}
