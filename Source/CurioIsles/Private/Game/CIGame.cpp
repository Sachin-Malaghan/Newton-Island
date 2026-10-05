// CURIO ISLES: game flow and the level controller (Build -> Play -> Result). (CLAUDE.md: Game flow)
#include "Game/CIGame.h"

#include "Game/CIContentLoader.h"
#include "Game/CISaveGame.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogCurioGame, Log, All);

namespace
{
	FString ToF(const std::string& S) { return UTF8_TO_TCHAR(S.c_str()); }

	bool IsWater(const std::string& Look) { return Look == "pond" || Look == "river"; }
}

// ---------------------------------------------------------------- setup

bool FCIGame::Init(UCISaveGame* InSave)
{
	Save = InSave;
	const TArray<FString> Found = CIContent::FindIslands();
	// Island 1 ships in the app; later islands are content packs found the same way.
	const FString Folder = Found.Contains(TEXT("Newton")) ? FString(TEXT("Newton")) : (Found.Num() > 0 ? Found[0] : FString());
	if (Folder.IsEmpty())
	{
		LoadError = TEXT("No island content found in ") + CIContent::IslandsRoot();
	}
	else
	{
		bLoaded = CIContent::LoadIsland(Folder, Island, LoadError);
	}
	if (!bLoaded) { UE_LOG(LogCurioGame, Error, TEXT("Content: %s"), *LoadError); }
	else { UE_LOG(LogCurioGame, Display, TEXT("Loaded %s: %d worlds, %d levels"), *ToF(Island.Name), (int32)Island.Worlds.size(), NumLevels()); }
	return bLoaded;
}

bool FCIGame::IsStudent() const
{
	return Save && Save->bStudentMode;
}

const CI::FWorldDef* FCIGame::CurrentWorld() const
{
	return bLoaded && WorldIndex >= 0 && WorldIndex < (int32)Island.Worlds.size() ? &Island.Worlds[WorldIndex] : nullptr;
}

const CI::FLevelDef* FCIGame::CurrentLevel() const
{
	const CI::FWorldDef* W = CurrentWorld();
	return W && LevelIndex >= 0 && LevelIndex < (int32)W->Levels.size() ? &W->Levels[LevelIndex] : nullptr;
}

int32 FCIGame::NumLevels() const
{
	int32 N = 0;
	for (const CI::FWorldDef& W : Island.Worlds) { N += (int32)W.Levels.size(); }
	return N;
}

bool FCIGame::FindNextLevel(int32& OutWorld, int32& OutLevel) const
{
	int32 W = WorldIndex, L = LevelIndex + 1;
	while (W < (int32)Island.Worlds.size())
	{
		if (L < (int32)Island.Worlds[W].Levels.size()) { OutWorld = W; OutLevel = L; return true; }
		++W;
		L = 0;
	}
	return false;
}

// ---------------------------------------------------------------- flow

void FCIGame::GoTo(ECIScreen NewScreen)
{
	Screen = NewScreen;
	ScreenTime = 0;
	Drag = FDrag();
	SliderGrab = -1;
	PressButton = -1;
	Buttons.Reset();
	TrayCards.Reset();
	Sliders.Reset();
}

void FCIGame::StartLevel(int32 World, int32 Level)
{
	WorldIndex = World;
	LevelIndex = Level;
	const CI::FLevelDef* L = CurrentLevel();
	if (!L) { return; }
	Setup = CI::FSetup::Defaults(*L);
	Phase = ECIPhase::Build;
	PhaseTime = 0;
	Selected = -1;
	SelectedFixed = -1;
	bPaused = false;
	bSlow = false;
	bSuccess = false;
	AliveTime = -1;
	Particles.Reset();
	RebuildSim();
	if (Save)
	{
		Save->LastWorld = World;
		Save->LastLevel = Level;
	}
	GoTo(ECIScreen::Playing);
	UpdateCamera();
}

void FCIGame::Activate(const FCIButton& B)
{
	if (!B.bEnabled) { return; }
	switch (B.Action)
	{
	case ECIAction::OpenLevels: GoTo(ECIScreen::Levels); break;
	case ECIAction::StartLevel: StartLevel(B.Param / 100, B.Param % 100); break;
	case ECIAction::Back:
		if (Screen == ECIScreen::Playing) { GoTo(ECIScreen::Levels); }
		else { GoTo(ECIScreen::Title); }
		break;
	case ECIAction::Run: Run(); break;
	case ECIAction::Retry: Retry(); break;
	case ECIAction::Pause: bPaused = !bPaused; break;
	case ECIAction::SlowMo: bSlow = !bSlow; break;
	case ECIAction::ToggleMode:
		if (Save) { Save->bStudentMode = !Save->bStudentMode; SaveProgress(); }
		ShowToast(IsStudent() ? TEXT("Student Mode: real numbers, units and formulas") : TEXT("Fun Mode: just play"), 2.0);
		break;
	case ECIAction::NextLevel:
	{
		int32 W, L;
		if (FindNextLevel(W, L)) { StartLevel(W, L); }
		else { GoTo(ECIScreen::Levels); }
		break;
	}
	case ECIAction::RemovePart: if (Selected >= 0) { Remove(Selected); } break;
	case ECIAction::Quit: bQuitRequested = true; break;
	default: break;
	}
}

void FCIGame::SaveProgress()
{
	if (Save && !bNoSave) { UGameplayStatics::SaveGameToSlot(Save, UCISaveGame::SlotName, 0); }
}

void FCIGame::OnAppBackground()
{
	if (Screen == ECIScreen::Playing && Phase == ECIPhase::Running) { bPaused = true; }
	SaveProgress();
}

void FCIGame::ApplySetup(const CI::FSetup& NewSetup, int32 SelectPlaced)
{
	if (!CurrentLevel()) { return; }
	Setup = NewSetup;
	Retry();
	(void)SelectPlaced;   // parts are tuned by their grips now; nothing to select
	Selected = -1;
}

void FCIGame::ShowToast(const FString& Text, double Seconds)
{
	Toast = Text;
	ToastUntil = RealTime + Seconds;
}

// ---------------------------------------------------------------- tick

void FCIGame::Tick(float DeltaSeconds, const FCIPointer& Pointer, const FCIKeys& Keys, double InScreenW, double InScreenH)
{
	const double Dt = FMath::Clamp((double)DeltaSeconds, 0.0, 0.1);
	RealTime += Dt;
	ScreenTime += Dt;
	ScreenW = InScreenW;
	ScreenH = InScreenH;

	HandleKeys(Keys);
	HandlePointer(Pointer);
	if (Screen == ECIScreen::Playing && CurrentLevel())
	{
		UpdateCamera();
		TickLevel(Dt);
	}
	TickParticles(Dt);
	Shake = FMath::Max(0.0, Shake - Dt * 3.0);
}

void FCIGame::HandleKeys(const FCIKeys& K)
{
	if (K.ToggleMode) { FCIButton B; B.Action = ECIAction::ToggleMode; Activate(B); }
	switch (Screen)
	{
	case ECIScreen::Title:
		if (K.Confirm) { GoTo(ECIScreen::Levels); }
		if (K.Back && !PlatformHasQuitButton()) { bQuitRequested = true; }
		break;
	case ECIScreen::Levels:
		if (K.Confirm && Save) { StartLevel(Save->LastWorld, Save->LastLevel); }
		if (K.Back) { GoTo(ECIScreen::Title); }
		break;
	case ECIScreen::Playing:
		if (K.Back)
		{
			if (Drag.bActive) { Drag = FDrag(); }
			else if (Selected >= 0) { Selected = -1; }
			else { GoTo(ECIScreen::Levels); }
		}
		if (K.Retry) { Retry(); }
		if (K.Pause && Phase != ECIPhase::Build) { bPaused = !bPaused; }
		if (K.Confirm)
		{
			if (Phase == ECIPhase::Build) { Run(); }
			else if (Phase == ECIPhase::Result && bSuccess) { FCIButton B; B.Action = ECIAction::NextLevel; Activate(B); }
			else { Retry(); }
		}
		break;
	}
}

void FCIGame::HandlePointer(const FCIPointer& P)
{
	HoverButton = -1;
	if (P.bValid)
	{
		for (int32 I = Buttons.Num() - 1; I >= 0; --I)
		{
			if (Buttons[I].Box.IsInside(P.Pos)) { HoverButton = I; break; }
		}
	}
	const bool bEditing = Screen == ECIScreen::Playing && Phase == ECIPhase::Build && CurrentLevel();

	if (P.bPressed)
	{
		PressPos = P.Pos;
		PressButton = HoverButton;
		if (PressButton >= 0 || !bEditing) { return; }

		// Grips on the machine come first: grab and drag, no selecting.
		{
			int32 Best = -1;
			double BestD = 1e18;
			const TArray<FCIGrip> All = Grips();
			for (int32 I = 0; I < All.Num(); ++I)
			{
				const double Reach = ScreenH * (All[I].Kind == ECIGrip::LauncherAim ? 0.11 : 0.085);
				const double D = FVector2D::Distance(P.Pos, ToScreen(All[I].World.X, All[I].World.Y));
				if (D < Reach && D < BestD) { BestD = D; Best = I; }
			}
			if (Best >= 0)
			{
				GripGrab = All[Best].Placed;
				GripKind = All[Best].Kind;
				GripPointer = P.Pos;
				bAimArmed = false;
				AimPower = 0;
				Selected = -1;
				DragGrip(P.Pos);
				return;
			}
		}

		const double Grab = ScreenH * 0.035;
		for (int32 I = 0; I < Sliders.Num(); ++I)
		{
			const FBox2D B = Sliders[I].Track.ExpandBy(FVector2D(Grab * 0.5, Grab));
			if (B.IsInside(P.Pos))
			{
				SliderGrab = I;
				const double T = FMath::Clamp((P.Pos.X - Sliders[I].Track.Min.X) / FMath::Max(1.0, Sliders[I].Track.GetSize().X), 0.0, 1.0);
				const CI::FParamDef* Q = SliderParam(Sliders[I]);
				if (Q) { SetSliderValue(Sliders[I], Q->Min + T * (Q->Max - Q->Min)); }
				return;
			}
		}
		for (const FCITrayCard& C : TrayCards)
		{
			if (C.Box.IsInside(P.Pos) && TrayRemaining(C.Tray) > 0)
			{
				Drag = FDrag();
				Drag.bActive = true;
				Drag.Tray = C.Tray;
				Drag.Screen = P.Pos;
				Drag.Start = P.Pos;
				Selected = -1;
				return;
			}
		}
		if (PanelBox.bIsValid && PanelBox.IsInside(P.Pos)) { return; }
		const int32 Placed = PlacedPartAt(P.Pos);
		if (Placed >= 0)
		{
			Drag = FDrag();
			Drag.bPending = true;
			Drag.FromPlaced = Placed;
			Drag.Tray = Setup.Placed[Placed].Tray;
			Drag.Start = P.Pos;
			Drag.Screen = P.Pos;
			return;
		}
		return;
	}

	if (P.bDown)
	{
		if (GripGrab >= 0 && bEditing) { DragGrip(P.Pos); }
		if (SliderGrab >= 0 && SliderGrab < Sliders.Num())
		{
			const FCISliderWidget& S = Sliders[SliderGrab];
			const double T = FMath::Clamp((P.Pos.X - S.Track.Min.X) / FMath::Max(1.0, S.Track.GetSize().X), 0.0, 1.0);
			if (const CI::FParamDef* Q = SliderParam(S)) { SetSliderValue(S, Q->Min + T * (Q->Max - Q->Min)); }
		}
		if (Drag.bPending && FVector2D::Distance(P.Pos, Drag.Start) > ScreenH * 0.025)
		{
			Drag.bPending = false;
			Drag.bActive = true;
			Selected = -1;
		}
		if (Drag.bActive && bEditing)
		{
			Drag.Screen = P.Pos;
			const std::string& PartId = CurrentLevel()->Tray[Drag.Tray].Part;
			Drag.HoverSlot = BestSlotFor(P.Pos, PartId, Drag.FromPlaced);
			Drag.bValid = Drag.HoverSlot >= 0;
		}
	}

	if (P.bReleased)
	{
		if (PressButton >= 0 && PressButton == HoverButton && PressButton < Buttons.Num()) { Activate(Buttons[PressButton]); }
		else if (bEditing)
		{
			if (GripGrab >= 0)
			{
				// Let go of a pulled-back launcher and it fires, like a slingshot.
				const bool bFire = GripKind == ECIGrip::LauncherAim && bAimArmed;
				GripGrab = -1;
				bPreviewDirty = true;
				if (bFire) { Run(); }
			}
			else if (SliderGrab >= 0) { /* value already applied while dragging */ }
			else if (Drag.bPending)
			{
				ShowToast(TEXT("Drag its glowing grip to tune it"), 1.6);
			}
			else if (Drag.bActive)
			{
				if (Drag.bValid) { Place(Drag.Tray, Drag.FromPlaced, Drag.HoverSlot); }
				else if (Drag.FromPlaced >= 0 && TrayBox.bIsValid && TrayBox.IsInside(P.Pos)) { Remove(Drag.FromPlaced); }
				else if (Drag.FromPlaced < 0 && FVector2D::Distance(P.Pos, Drag.Start) < ScreenH * 0.02)
				{
					// A tap on a tray card: place it in the first free slot that takes it.
					const std::string& PartId = CurrentLevel()->Tray[Drag.Tray].Part;
					for (int32 S = 0; S < (int32)CurrentLevel()->Slots.size(); ++S)
					{
						if (BestSlotFor(SlotScreen(S), PartId, -1) == S) { Place(Drag.Tray, -1, S); break; }
					}
				}
				else if (Drag.FromPlaced < 0) { ShowToast(TEXT("Drop it on a glowing spot"), 1.8); }
			}
			else if (PressButton < 0 && !(PanelBox.bIsValid && PanelBox.IsInside(P.Pos)) && !(TrayBox.bIsValid && TrayBox.IsInside(P.Pos)))
			{
				Selected = -1;
				SelectedFixed = -1;
			}
		}
		SliderGrab = -1;
		GripGrab = -1;
		Drag = FDrag();
		PressButton = -1;
	}
}

// ---------------------------------------------------------------- grips (direct manipulation)

TArray<FCIGrip> FCIGame::Grips() const
{
	TArray<FCIGrip> Out;
	const CI::FLevelDef* L = CurrentLevel();
	if (!L || Phase != ECIPhase::Build) { return Out; }
	for (int32 I = 0; I < (int32)Setup.Placed.size(); ++I)
	{
		const int32 Inst = (int32)L->Fixed.size() + I;
		if (Inst >= (int32)Sim.Parts.size()) { continue; }
		const CI::FPartInstance& PI2 = Sim.Parts[Inst];
		const CI::FPartGeometry& G = PI2.Geo;
		FCIGrip Grip;
		Grip.Placed = I;
		switch (PI2.Part->Behavior)
		{
		case CI::EPartBehavior::Ramp:
			if (G.Chains.empty() || G.Chains[0].Points.size() < 2) { continue; }
			Grip.Kind = ECIGrip::RampHeight;
			Grip.World = FVector2D(G.Chains[0].Points[0].X + 0.05, G.Chains[0].Points[1].Y + 0.75);
			break;
		case CI::EPartBehavior::Launcher:
			if (G.Anchors.empty()) { continue; }
			Grip.Kind = ECIGrip::LauncherAim;
			Grip.World = FVector2D(G.Anchors[0].Point.X, G.Anchors[0].Point.Y);
			break;
		case CI::EPartBehavior::Brake:
		{
			if (G.Brakes.empty() || PI2.Values.empty()) { continue; }
			const CI::FParamDef& Q = L->Tray[PI2.Tray].Params[0];
			const double T = Q.Max > Q.Min ? (PI2.Values[0] - Q.Min) / (Q.Max - Q.Min) : 0;
			Grip.Kind = ECIGrip::BrakeStrength;
			Grip.World = FVector2D(FMath::Lerp(G.Brakes[0].Min.X + 0.3, G.Brakes[0].Max.X - 0.3, T), G.Base.Y + 0.55);
			break;
		}
		}
		Out.Add(Grip);
	}
	return Out;
}

void FCIGame::SetParam(int32 Placed, const char* ParamId, double Value)
{
	const CI::FLevelDef* L = CurrentLevel();
	if (!L || Placed < 0 || Placed >= (int32)Setup.Placed.size()) { return; }
	const CI::FPartDef* Def = Island.Catalog.Find(L->Tray[Setup.Placed[Placed].Tray].Part);
	if (!Def) { return; }
	FCISliderWidget S;
	S.Placed = Placed;
	S.Param = Def->ParamIndex(ParamId);
	if (S.Param >= 0) { SetSliderValue(S, Value); }
}

void FCIGame::DragGrip(const FVector2D& Screen2)
{
	const CI::FLevelDef* L = CurrentLevel();
	if (!L || GripGrab < 0 || GripGrab >= (int32)Setup.Placed.size()) { return; }
	GripPointer = Screen2;
	const int32 Inst = (int32)L->Fixed.size() + GripGrab;
	if (Inst >= (int32)Sim.Parts.size()) { return; }
	const CI::FPartGeometry G = Sim.Parts[Inst].Geo;
	const CI::FTrayItem& Item = L->Tray[Setup.Placed[GripGrab].Tray];
	const CI::FVec2 W = ToWorld(Screen2);
	switch (GripKind)
	{
	case ECIGrip::RampHeight:
		SetParam(GripGrab, "height", W.Y - 0.75 - G.Base.Y);
		break;
	case ECIGrip::BrakeStrength:
		if (!G.Brakes.empty() && !Item.Params.empty())
		{
			const CI::FParamDef& Q = Item.Params[0];
			const double T = FMath::Clamp((W.X - (G.Brakes[0].Min.X + 0.3)) / FMath::Max(0.1, G.Brakes[0].Max.X - G.Brakes[0].Min.X - 0.6), 0.0, 1.0);
			SetParam(GripGrab, "strength", Q.Min + T * (Q.Max - Q.Min));
		}
		break;
	case ECIGrip::LauncherAim:
	{
		// Pull back from the pivot: the shot goes the opposite way, farther pull = more speed.
		const FVector2D Pv = ToScreen(G.Pivot.X, G.Pivot.Y);
		const double PX = (Pv.X - Screen2.X) * G.Facing, PY = Screen2.Y - Pv.Y;
		const double Len = FMath::Sqrt(PX * PX + PY * PY);
		const double MaxPull = ScreenH * 0.3;
		bAimArmed = Len > ScreenH * 0.045;
		AimPower = FMath::Clamp((Len - ScreenH * 0.045) / (MaxPull - ScreenH * 0.045), 0.0, 1.0);
		if (bAimArmed)
		{
			const int32 SI = 1;
			if ((int32)Item.Params.size() > SI)
			{
				SetParam(GripGrab, "angle", FMath::RadiansToDegrees(FMath::Atan2(PY, FMath::Max(PX, 1e-3))));
				SetParam(GripGrab, "speed", Item.Params[SI].Min + AimPower * (Item.Params[SI].Max - Item.Params[SI].Min));
			}
		}
		break;
	}
	}
}

// ---------------------------------------------------------------- level

void FCIGame::RebuildSim()
{
	const CI::FLevelDef* L = CurrentLevel();
	if (!L) { return; }
	std::string Err;
	if (!Sim.Load(*L, Island.Catalog, Setup, &Err)) { ShowToast(ToF(Err), 3.0); }
	Sim.bContinueAfterEnd = false;
	Trail.Reset();
	PrevPos.SetNum((int32)Sim.Bodies.size());
	for (int32 I = 0; I < PrevPos.Num(); ++I) { PrevPos[I] = FVector2D(Sim.Bodies[I].P.X, Sim.Bodies[I].P.Y); }
	Alpha = 1;
	bPreviewDirty = true;
}

void FCIGame::Run()
{
	if (Phase != ECIPhase::Build) { return; }
	RebuildSim();
	Sim.Events.clear();
	Phase = ECIPhase::Running;
	PhaseTime = 0;
	bSinking = false;
	Accumulator = 0;
	bPaused = false;
	Selected = -1;
	SelectedFixed = -1;
	AliveTime = -1;
}

void FCIGame::Retry()
{
	if (Screen != ECIScreen::Playing) { return; }
	bSinking = false;
	Phase = ECIPhase::Build;
	PhaseTime = 0;
	bPaused = false;
	bSuccess = false;
	AliveTime = -1;
	Particles.Reset();
	RebuildSim();
}

void FCIGame::TickLevel(double Dt)
{
	PhaseTime += Dt;
	if (AliveTime >= 0) { AliveTime += Dt; }
	if (Phase == ECIPhase::Build)
	{
		if (bPreviewDirty) { UpdatePreview(); }
		Alpha = 1;
		return;
	}
	if (bPaused) { return; }

	// After the result the machine keeps moving for a while (the solved machine plays on).
	const bool bStep = Phase == ECIPhase::Running || (!bSinking && PhaseTime < (bSuccess ? 8.0 : 2.5));
	if (!bStep) { Alpha = 1; return; }
	Accumulator += Dt * (bSlow ? 0.25 : 1.0);
	int32 Guard = 0;
	while (Accumulator >= CI::kStep && Guard++ < 24)
	{
		for (int32 I = 0; I < PrevPos.Num(); ++I) { PrevPos[I] = FVector2D(Sim.Bodies[I].P.X, Sim.Bodies[I].P.Y); }
		Sim.Step();
		Accumulator -= CI::kStep;
		if (!Sim.Bodies.empty() && !Sim.Bodies[0].bResting && Sim.StepCount % 2 == 0)
		{
			Trail.Add(FVector2D(Sim.Bodies[0].P.X, Sim.Bodies[0].P.Y));
			if (Trail.Num() > 26) { Trail.RemoveAt(0); }
		}
		ProcessSimEvents();
		if (Phase == ECIPhase::Running && Sim.Outcome != CI::EOutcome::Running)
		{
			Finish();
			break;
		}
	}
	Alpha = FMath::Clamp(Accumulator / CI::kStep, 0.0, 1.0);
}

void FCIGame::Finish()
{
	const CI::FLevelDef* L = CurrentLevel();
	Phase = ECIPhase::Result;
	PhaseTime = 0;
	bSuccess = Sim.Outcome == CI::EOutcome::Success;
	Sim.bContinueAfterEnd = true;
	if (bSuccess)
	{
		// Stars: solved; within par; (M2) a correct prediction.
		Stars = 1 + (Setup.NumPlaced() <= L->Par ? 1 : 0);
		const FString Id = ToF(L->Id);
		int32& Best = Save ? Save->Stars.FindOrAdd(Id) : Stars;
		bNewBest = Stars > Best;
		Best = FMath::Max(Best, Stars);
		if (Save && !L->Concept.empty()) { Save->Notebook.AddUnique(ToF(L->Concept)); }
		SaveProgress();

		const int32 Zone = L->Goals.empty() ? -1 : L->FindZone(L->Goals[0].Zone);
		AliveOrigin = Zone >= 0 ? FVector2D((L->Zones[Zone].Min.X + L->Zones[Zone].Max.X) * 0.5, (L->Zones[Zone].Min.Y + L->Zones[Zone].Max.Y) * 0.5) : FVector2D(0, 0);
		AliveTime = 0;
		const TArray<FLinearColor> Confetti = { FLinearColor(1.f, 0.36f, 0.3f), FLinearColor(1.f, 0.78f, 0.2f), FLinearColor(0.3f, 0.8f, 0.45f), FLinearColor(0.3f, 0.6f, 1.f), FLinearColor(0.85f, 0.45f, 1.f) };
		Burst(AliveOrigin, 90, Confetti, 6.0, true);
	}
	else
	{
		FailText = Sim.FailText ? ToF(*Sim.FailText) : FString(TEXT("Not quite!"));
		Shake = 0.35;
		// A splash if it ended in water.
		for (const CI::FBody& B : Sim.Bodies)
		{
			for (const CI::FZoneDef& Z : L->Zones)
			{
				if (IsWater(Z.Look) && Z.Contains(B.P))
				{
					bSinking = true;
					const TArray<FLinearColor> Water = { FLinearColor(0.55f, 0.8f, 1.f, 0.9f), FLinearColor(0.85f, 0.95f, 1.f, 0.9f) };
					Burst(FVector2D(B.P.X, B.P.Y), 40, Water, 4.0, false);
				}
			}
		}
	}
}

void FCIGame::ProcessSimEvents()
{
	for (const CI::FSimEvent& E : Sim.Events)
	{
		if ((E.Type == CI::ESimEvent::Bounce || E.Type == CI::ESimEvent::Land) && E.Strength > 1.2)
		{
			const TArray<FLinearColor> Dust = { FLinearColor(1.f, 0.95f, 0.85f, 0.7f) };
			Burst(FVector2D(E.Pos.X, E.Pos.Y - 0.1), FMath::Min(14, 3 + (int32)(E.Strength * 2)), Dust, 1.2 + E.Strength * 0.2, false);
			Shake = FMath::Max(Shake, FMath::Min(0.25, E.Strength * 0.03));
		}
	}
	Sim.Events.clear();
}

void FCIGame::UpdatePreview()
{
	bPreviewDirty = false;
	Preview.Reset();
	const CI::FLevelDef* L = CurrentLevel();
	// While aiming a launcher the first stretch of the real flight is shown, slingshot style.
	const bool bAiming = GripGrab >= 0 && GripKind == ECIGrip::LauncherAim && bAimArmed;
	const double Seconds = L ? FMath::Max(L->PreviewSeconds, bAiming ? 0.7 : 0.0) : 0.0;
	if (!L || Seconds <= 0 || Sim.Bodies.empty()) { return; }
	CI::FSim Copy = Sim;   // the sim is copyable: the preview is the real physics, just cut short
	Copy.bContinueAfterEnd = true;
	const int32 Steps = (int32)(Seconds / CI::kStep);
	for (int32 I = 0; I <= Steps; ++I)
	{
		if (I % 3 == 0) { Preview.Add(FVector2D(Copy.Bodies[0].P.X, Copy.Bodies[0].P.Y)); }
		Copy.Step();
		Copy.Events.clear();
	}
}

void FCIGame::UpdateCamera()
{
	const CI::FLevelDef* L = CurrentLevel();
	if (!L) { return; }
	const double X0 = Safe.X, X1 = ScreenW - Safe.Z;
	const double Y0 = FMath::Max(TopBarH, Safe.Y), Y1 = ScreenH - FMath::Max(TrayH, Safe.W);
	WorldBox = FBox2D(FVector2D(X0, Y0), FVector2D(X1, FMath::Max(Y0 + 10, Y1)));
	const double VW = L->ViewMax.X - L->ViewMin.X, VH = L->ViewMax.Y - L->ViewMin.Y;
	const FVector2D Size = WorldBox.GetSize();
	CamScale = FMath::Min(Size.X * 0.96 / VW, Size.Y * 0.94 / VH);
	const FVector2D C = WorldBox.GetCenter();
	CamOffX = C.X - (L->ViewMin.X + VW * 0.5) * CamScale;
	// Sit the view on the bottom of the world box (extra room goes to the sky).
	CamOffY = WorldBox.Max.Y - Size.Y * 0.03 + L->ViewMin.Y * CamScale;
}

// ---------------------------------------------------------------- editing helpers

int32 FCIGame::TrayRemaining(int32 Tray) const
{
	const CI::FLevelDef* L = CurrentLevel();
	if (!L || Tray < 0 || Tray >= (int32)L->Tray.size()) { return 0; }
	return L->Tray[Tray].Count - Setup.CountPlaced(Tray);
}

FVector2D FCIGame::SlotScreen(int32 Slot) const
{
	const CI::FLevelDef* L = CurrentLevel();
	return L && Slot >= 0 && Slot < (int32)L->Slots.size() ? ToScreen(L->Slots[Slot].Pos.X, L->Slots[Slot].Pos.Y) : FVector2D::ZeroVector;
}

int32 FCIGame::BestSlotFor(const FVector2D& Screen2, const std::string& PartId, int32 IgnorePlaced) const
{
	const CI::FLevelDef* L = CurrentLevel();
	if (!L) { return -1; }
	const double Snap = FMath::Max(ScreenH * 0.14, CamScale * 1.2);
	int32 Best = -1;
	double BestD = Snap;
	for (int32 S = 0; S < (int32)L->Slots.size(); ++S)
	{
		if (!L->Slots[S].AcceptsPart(PartId)) { continue; }
		const int32 Occupant = Setup.PartInSlot(S);
		if (Occupant >= 0 && Occupant != IgnorePlaced) { continue; }
		bool bFixed = false;
		for (const CI::FFixedPart& F : L->Fixed) { if (L->FindSlot(F.Slot) == S) { bFixed = true; } }
		if (bFixed) { continue; }
		const double D = FVector2D::Distance(Screen2, SlotScreen(S));
		if (D < BestD) { BestD = D; Best = S; }
	}
	return Best;
}

int32 FCIGame::PlacedPartAt(const FVector2D& Screen2) const
{
	const CI::FLevelDef* L = CurrentLevel();
	if (!L) { return -1; }
	const double Pad = ScreenH * 0.035;
	for (int32 I = (int32)Setup.Placed.size() - 1; I >= 0; --I)
	{
		const int32 Inst = (int32)L->Fixed.size() + I;
		if (Inst >= (int32)Sim.Parts.size()) { continue; }
		const CI::FPartGeometry& G = Sim.Parts[Inst].Geo;
		FBox2D Box(ForceInit);
		Box += SlotScreen(Setup.Placed[I].Slot);
		for (const CI::FChain& C : G.Chains) { for (const CI::FVec2& V : C.Points) { Box += ToScreen(V.X, V.Y); } }
		for (const CI::FBrakeRegion& B : G.Brakes) { Box += ToScreen(B.Min.X, B.Min.Y + 0.25); Box += ToScreen(B.Max.X, B.Min.Y + 0.6); }
		for (const CI::FAnchor& A : G.Anchors) { Box += ToScreen(A.Point.X, A.Point.Y); }
		if (Sim.Parts[Inst].Part->Behavior == CI::EPartBehavior::Launcher) { Box += ToScreen(G.Pivot.X - 0.6, G.Pivot.Y - 0.4); Box += ToScreen(G.Pivot.X + 0.6, G.Pivot.Y + 0.9); }
		if (Box.ExpandBy(Pad).IsInside(Screen2)) { return I; }
	}
	return -1;
}

void FCIGame::Place(int32 Tray, int32 FromPlaced, int32 Slot)
{
	const CI::FLevelDef* L = CurrentLevel();
	if (!L) { return; }
	if (FromPlaced >= 0)
	{
		Setup.Placed[FromPlaced].Slot = Slot;
		Selected = FromPlaced;
	}
	else
	{
		CI::FPlacedPart P;
		P.Tray = Tray;
		P.Slot = Slot;
		for (const CI::FParamDef& Q : L->Tray[Tray].Params) { P.Values.push_back(Q.Default); }
		Setup.Placed.push_back(P);
		Selected = (int32)Setup.Placed.size() - 1;
	}
	SelectedFixed = -1;
	RebuildSim();
	const CI::FVec2 At = L->Slots[Slot].Pos;
	const TArray<FLinearColor> Puff = { FLinearColor(1.f, 1.f, 1.f, 0.8f), FLinearColor(1.f, 0.9f, 0.6f, 0.8f) };
	Burst(FVector2D(At.X, At.Y + 0.1), 12, Puff, 1.5, false);
}

void FCIGame::Remove(int32 Placed)
{
	if (Placed < 0 || Placed >= (int32)Setup.Placed.size()) { return; }
	Setup.Placed.erase(Setup.Placed.begin() + Placed);
	Selected = -1;
	RebuildSim();
}

const CI::FParamDef* FCIGame::SliderParam(const FCISliderWidget& S) const
{
	const CI::FLevelDef* L = CurrentLevel();
	if (!L) { return nullptr; }
	if (S.Placed >= 0 && S.Placed < (int32)Setup.Placed.size())
	{
		const CI::FTrayItem& T = L->Tray[Setup.Placed[S.Placed].Tray];
		return S.Param >= 0 && S.Param < (int32)T.Params.size() ? &T.Params[S.Param] : nullptr;
	}
	if (S.Fixed >= 0 && S.Fixed < (int32)L->Fixed.size())
	{
		const CI::FFixedPart& F = L->Fixed[S.Fixed];
		return S.Param >= 0 && S.Param < (int32)F.Params.size() ? &F.Params[S.Param] : nullptr;
	}
	return nullptr;
}

double FCIGame::SliderValue(const FCISliderWidget& S) const
{
	if (S.Placed >= 0 && S.Placed < (int32)Setup.Placed.size() && S.Param < (int32)Setup.Placed[S.Placed].Values.size()) { return Setup.Placed[S.Placed].Values[S.Param]; }
	if (S.Fixed >= 0 && S.Fixed < (int32)Setup.FixedValues.size() && S.Param < (int32)Setup.FixedValues[S.Fixed].size()) { return Setup.FixedValues[S.Fixed][S.Param]; }
	return 0;
}

void FCIGame::SetSliderValue(const FCISliderWidget& S, double Value)
{
	const CI::FParamDef* Q = SliderParam(S);
	if (!Q || Phase != ECIPhase::Build) { return; }
	const double V = Q->Snap(Value);
	double* Slot = nullptr;
	if (S.Placed >= 0) { Slot = &Setup.Placed[S.Placed].Values[S.Param]; }
	else if (S.Fixed >= 0) { Slot = &Setup.FixedValues[S.Fixed][S.Param]; }
	if (Slot && *Slot != V)
	{
		*Slot = V;
		RebuildSim();
	}
}

// ---------------------------------------------------------------- effects

double FCIGame::FxRand()
{
	FxSeed = FxSeed * 1664525u + 1013904223u;
	return (FxSeed >> 8) / 16777216.0;
}

void FCIGame::Burst(const FVector2D& World, int32 Count, const TArray<FLinearColor>& Colors, double Speed, bool bConfetti)
{
	for (int32 I = 0; I < Count; ++I)
	{
		FCIParticle P;
		const double A = bConfetti ? FMath::Lerp(0.15, PI - 0.15, FxRand()) : FxRand() * 2 * PI;
		const double S = Speed * FMath::Lerp(0.35, 1.0, FxRand());
		P.P = World;
		P.V = FVector2D(FMath::Cos(A) * S, FMath::Abs(FMath::Sin(A)) * S * (bConfetti ? 1.4 : 0.8));
		P.Color = Colors[I % Colors.Num()];
		P.Life = bConfetti ? FMath::Lerp(1.8, 3.2, FxRand()) : FMath::Lerp(0.4, 0.9, FxRand());
		P.Size = bConfetti ? FMath::Lerp(0.05, 0.1, FxRand()) : FMath::Lerp(0.03, 0.07, FxRand());
		P.Spin = (FxRand() - 0.5) * 16;
		P.Angle = FxRand() * PI;
		P.bRect = bConfetti;
		P.bGravity = true;
		Particles.Add(P);
	}
}

void FCIGame::TickParticles(double Dt)
{
	for (int32 I = Particles.Num() - 1; I >= 0; --I)
	{
		FCIParticle& P = Particles[I];
		P.Age += Dt;
		if (P.Age >= P.Life) { Particles.RemoveAtSwap(I); continue; }
		if (P.bGravity) { P.V.Y -= (P.bRect ? 3.5 : 9.81) * Dt; }
		const double Drag2 = P.bRect ? 1.6 : 0.6;
		P.V *= FMath::Max(0.0, 1.0 - Drag2 * Dt);
		P.P += P.V * Dt;
		P.Angle += P.Spin * Dt;
	}
}
