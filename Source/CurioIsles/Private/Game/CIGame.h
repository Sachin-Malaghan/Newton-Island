// CURIO ISLES: game flow and the level controller (Build -> Play -> Result). (CLAUDE.md: Game flow)
// Plain C++ owned by ACIPlayerController; ACIHUD draws it. Fun/Student Mode changes only what the UI
// shows - the simulation never sees it.
#pragma once

#include "CoreMinimal.h"
#include "Core/CISim.h"

class UCISaveGame;

enum class ECIScreen : uint8 { Title, Levels, Playing };
enum class ECIPhase : uint8 { Build, Running, Result };

enum class ECIAction : uint8
{
	None, OpenLevels, StartLevel, Back, Run, Retry, Pause, SlowMo, ToggleMode, NextLevel, RemovePart, Quit
};

struct FCIButton
{
	FBox2D Box = FBox2D(ForceInit);
	ECIAction Action = ECIAction::None;
	int32 Param = 0;
	bool bEnabled = true;
};

// Widgets the UI lays out each frame and the controller hit-tests the next.
struct FCITrayCard
{
	FBox2D Box = FBox2D(ForceInit);
	int32 Tray = -1;
};

struct FCISliderWidget
{
	FBox2D Track = FBox2D(ForceInit);   // the draggable span
	int32 Placed = -1;                  // Setup.Placed index, or -1 for a tunable fixed part
	int32 Fixed = -1;
	int32 Param = -1;
};

enum class ECIGrip : uint8 { RampHeight, LauncherAim, BrakeStrength };

struct FCIGrip
{
	int32 Placed = -1;
	ECIGrip Kind = ECIGrip::RampHeight;
	FVector2D World = FVector2D::ZeroVector;   // where the grip is drawn and grabbed
};

// Mouse or first finger.
struct FCIPointer
{
	bool bValid = false;
	FVector2D Pos = FVector2D::ZeroVector;
	bool bDown = false;
	bool bPressed = false;    // went down this frame
	bool bReleased = false;   // went up this frame
	bool bTouch = false;
};

struct FCIKeys
{
	bool Confirm = false, Back = false, Retry = false, ToggleMode = false, Pause = false;
};

// Decorative only (confetti, splashes): never part of the deterministic simulation.
struct FCIParticle
{
	FVector2D P, V;             // world metres
	FLinearColor Color;
	double Life = 1, Age = 0, Size = 0.05, Spin = 0, Angle = 0;
	bool bGravity = true;
	bool bRect = true;
};

class FCIGame
{
public:
	bool Init(UCISaveGame* InSave);
	void Tick(float DeltaSeconds, const FCIPointer& Pointer, const FCIKeys& Keys, double InScreenW, double InScreenH);

	void GoTo(ECIScreen NewScreen);
	void StartLevel(int32 World, int32 Level);
	void Activate(const FCIButton& Button);
	void SaveProgress();
	void OnAppBackground();
	// Replaces the player's setup (hints, console, capture script) and returns to the Build phase.
	void ApplySetup(const CI::FSetup& NewSetup, int32 SelectPlaced = -1);
	void Act(ECIAction Action, int32 Param = 0) { FCIButton B; B.Action = Action; B.Param = Param; Activate(B); }

	// Content
	CI::FIslandDef Island;
	bool bLoaded = false;
	FString LoadError;
	const CI::FLevelDef* CurrentLevel() const;
	const CI::FWorldDef* CurrentWorld() const;
	int32 NumLevels() const;
	bool FindNextLevel(int32& OutWorld, int32& OutLevel) const;

	// Screens
	ECIScreen Screen = ECIScreen::Title;
	double ScreenTime = 0;
	double RealTime = 0;

	// Level state
	int32 WorldIndex = 0, LevelIndex = 0;
	CI::FSetup Setup;
	CI::FSim Sim;                  // Build: loaded (shows the setup); Running/Result: stepped
	ECIPhase Phase = ECIPhase::Build;
	bool bPaused = false;
	bool bSlow = false;
	double Accumulator = 0;
	double PhaseTime = 0;          // real time in the current phase
	TArray<FVector2D> PrevPos;     // body positions one step ago (render interpolation)
	double Alpha = 1;
	double AliveTime = -1;         // seconds since the machine came alive (success); -1 = not yet
	FVector2D AliveOrigin = FVector2D::ZeroVector;
	double Shake = 0;
	bool bSinking = false;         // failed into water: the body sinks instead of rolling on

	// Editing
	int32 Selected = -1;           // Setup.Placed index with its slider panel open
	int32 SelectedFixed = -1;      // or a tunable fixed part
	struct FDrag
	{
		bool bActive = false;
		bool bPending = false;     // pressed on a placed part, not yet moved far enough to drag
		int32 Tray = -1;
		int32 FromPlaced = -1;
		FVector2D Screen = FVector2D::ZeroVector;
		FVector2D Start = FVector2D::ZeroVector;
		int32 HoverSlot = -1;
		bool bValid = false;
	} Drag;
	int32 SliderGrab = -1;

	// Direct manipulation (Angry Birds style): every placed part carries a grip you drag right on the
	// machine - pull the launcher back and let go to fire, lift the ramp by its flag, slide the brake knob.
	TArray<FCIGrip> Grips() const;
	void DragGrip(const FVector2D& Screen);
	int32 GripGrab = -1;           // Setup.Placed index being tuned
	ECIGrip GripKind = ECIGrip::RampHeight;
	FVector2D GripPointer = FVector2D::ZeroVector;
	bool bAimArmed = false;        // launcher pulled far enough that letting go fires
	double AimPower = 0;           // 0..1 while aiming
	TArray<FVector2D> Trail;       // recent positions of the first body (motion streak)
	TArray<FVector2D> Preview;     // dotted path (world metres)

	// Result
	bool bSuccess = false;
	int32 Stars = 0;
	bool bNewBest = false;
	FString FailText;

	// Camera: screen = Offset + (x, -y) * Scale
	double CamScale = 60, CamOffX = 0, CamOffY = 0;
	FBox2D WorldBox = FBox2D(ForceInit);
	double ScreenW = 1280, ScreenH = 720;
	FVector2D ToScreen(double X, double Y) const { return FVector2D(CamOffX + X * CamScale, CamOffY - Y * CamScale); }
	CI::FVec2 ToWorld(const FVector2D& S) const { return CI::FVec2((S.X - CamOffX) / CamScale, (CamOffY - S.Y) / CamScale); }

	// Layout (pixels), set by the UI each frame
	double TopBarH = 0, TrayH = 0;
	FVector4 Safe = FVector4(0, 0, 0, 0);    // left, top, right, bottom insets
	TArray<FCIButton> Buttons;
	TArray<FCITrayCard> TrayCards;
	TArray<FCISliderWidget> Sliders;
	FBox2D PanelBox = FBox2D(ForceInit);
	FBox2D TrayBox = FBox2D(ForceInit);
	int32 HoverButton = -1;

	TArray<FCIParticle> Particles;
	FString Toast;
	double ToastUntil = 0;

	UCISaveGame* Save = nullptr;
	bool IsStudent() const;
	bool bQuitRequested = false;
	bool bNoSave = false;
	bool bHideUI = false;
	static bool PlatformHasQuitButton() { return !(PLATFORM_ANDROID || PLATFORM_IOS); }

	// Slider value helpers (UI + input)
	const CI::FParamDef* SliderParam(const FCISliderWidget& S) const;
	double SliderValue(const FCISliderWidget& S) const;
	void SetSliderValue(const FCISliderWidget& S, double Value);

	int32 TrayRemaining(int32 Tray) const;
	FVector2D SlotScreen(int32 Slot) const;

private:
	void TickLevel(double Dt);
	void HandlePointer(const FCIPointer& Pointer);
	void HandleKeys(const FCIKeys& Keys);
	void RebuildSim();
	void Run();
	void Retry();
	void Finish();
	void UpdateCamera();
	void UpdatePreview();
	void ProcessSimEvents();
	void TickParticles(double Dt);
	void Burst(const FVector2D& World, int32 Count, const TArray<FLinearColor>& Colors, double Speed, bool bConfetti);
	int32 BestSlotFor(const FVector2D& Screen, const std::string& PartId, int32 IgnorePlaced) const;
	int32 PlacedPartAt(const FVector2D& Screen) const;
	void Place(int32 Tray, int32 FromPlaced, int32 Slot);
	void Remove(int32 Placed);
	void ShowToast(const FString& Text, double Seconds = 2.5);
	void SetParam(int32 Placed, const char* ParamId, double Value);

	FVector2D PressPos = FVector2D::ZeroVector;
	int32 PressButton = -1;
	bool bPreviewDirty = true;
	uint32 FxSeed = 12345;
	double FxRand();
};
