// CURIO ISLES: menus and the in-level HUD - tray, sliders, buttons, mode toggle, result cards. (CLAUDE.md: UI)
// Look: big rounded cards, soft shadows, colour-coded buttons (Play = green, Predict = yellow).
#include "UI/CIUI.h"

#include "Core/CIExpr.h"
#include "Core/CIParts.h"
#include "Game/CIGame.h"
#include "Game/CISaveGame.h"
#include "Render/CIDraw.h"

#include <cstdlib>

namespace
{
	constexpr uint32 Ink = 0x1f2440, InkSoft = 0x59607a, Cream = 0xfffaf0, Green = 0x34c26b, Yellow = 0xffc93c,
		Blue = 0x5b7cfa, Coral = 0xff6b5a, Muted = 0xb8bfd1;

	FString ToF(const std::string& S) { return UTF8_TO_TCHAR(S.c_str()); }
	FLinearColor Col(uint32 RGB, float A = 1.f) { return CIColor(RGB, A); }

	enum class EIcon { None, Play, Pause, Retry, Back, Next, Slow, Trash, Lock };

	struct FUi
	{
		FCIDraw& D;
		FCIGame& G;
		const FCIPointer& P;
		double W, H, U;
		double SL, ST, SR, SB;
	};

	bool Hot(const FUi& Ui, const FBox2D& B) { return Ui.P.bValid && B.IsInside(Ui.P.Pos) && (!Ui.P.bTouch || Ui.P.bDown); }


	void DrawIcon(FCIDraw& D, EIcon Icon, double X, double Y, double S, const FLinearColor& C)
	{
		switch (Icon)
		{
		case EIcon::Play: D.Tri(X - S * 0.35, Y - S * 0.45, X - S * 0.35, Y + S * 0.45, X + S * 0.45, Y, C); break;
		case EIcon::Pause: D.RoundRect(X - S * 0.38, Y - S * 0.42, X - S * 0.1, Y + S * 0.42, S * 0.08, C); D.RoundRect(X + S * 0.1, Y - S * 0.42, X + S * 0.38, Y + S * 0.42, S * 0.08, C); break;
		case EIcon::Retry:
			D.Arc(X, Y, S * 0.28, S * 0.42, PI * 0.15, PI * 1.75, C, 20);
			D.Tri(X + S * 0.46, Y - S * 0.34, X + S * 0.46, Y + S * 0.02, X + S * 0.12, Y - S * 0.12, C);
			break;
		case EIcon::Back: D.RoundLine(X + S * 0.15, Y - S * 0.35, X - S * 0.2, Y, S * 0.16, C); D.RoundLine(X - S * 0.2, Y, X + S * 0.15, Y + S * 0.35, S * 0.16, C); break;
		case EIcon::Next:
			D.Tri(X - S * 0.45, Y - S * 0.4, X - S * 0.45, Y + S * 0.4, X, Y, C);
			D.Tri(X, Y - S * 0.4, X, Y + S * 0.4, X + S * 0.45, Y, C);
			break;
		case EIcon::Slow:
			// A snail: shell spiral and body.
			D.Ring(X + S * 0.05, Y - S * 0.05, S * 0.22, S * 0.36, C, 24);
			D.Circle(X + S * 0.05, Y - S * 0.05, S * 0.08, C, 10);
			D.RoundLine(X - S * 0.45, Y + S * 0.34, X + S * 0.45, Y + S * 0.34, S * 0.14, C);
			D.RoundLine(X - S * 0.38, Y + S * 0.3, X - S * 0.46, Y + S * 0.02, S * 0.08, C);
			break;
		case EIcon::Trash:
			D.RoundRect(X - S * 0.3, Y - S * 0.22, X + S * 0.3, Y + S * 0.45, S * 0.08, C);
			D.RoundRect(X - S * 0.42, Y - S * 0.38, X + S * 0.42, Y - S * 0.26, S * 0.05, C);
			D.RoundRect(X - S * 0.12, Y - S * 0.48, X + S * 0.12, Y - S * 0.36, S * 0.04, C);
			break;
		case EIcon::Lock:
			D.Arc(X, Y - S * 0.1, S * 0.2, S * 0.32, PI, 2 * PI, C, 12);
			D.RoundRect(X - S * 0.36, Y - S * 0.1, X + S * 0.36, Y + S * 0.42, S * 0.08, C);
			break;
		default: break;
		}
	}

	void Card(FUi& Ui, const FBox2D& B, uint32 Color = Cream, double Radius = 0)
	{
		const double R = Radius > 0 ? Radius : Ui.U * 2.4;
		Ui.D.Shadow(B.Min.X, B.Min.Y + Ui.U * 0.6, B.Max.X, B.Max.Y + Ui.U * 0.6, R, Ui.U * 1.4, FLinearColor(0.1f, 0.08f, 0.2f, 0.16f));
		Ui.D.RoundRect(B.Min.X, B.Min.Y, B.Max.X, B.Max.Y, R, Col(Color));
	}

	// A chunky rounded button with a darker lip underneath (reads as pressable), icon and label.
	void Button(FUi& Ui, const FBox2D& B, const FString& Label, EIcon Icon, uint32 Color, ECIAction Action, int32 Param = 0, bool bEnabled = true, double Px = 0)
	{
		FCIDraw& D = Ui.D;
		FCIButton Btn;
		Btn.Box = B;
		Btn.Action = Action;
		Btn.Param = Param;
		Btn.bEnabled = bEnabled;
		Ui.G.Buttons.Add(Btn);

		const bool bHot = bEnabled && Hot(Ui, B);
		const bool bDown = bHot && Ui.P.bDown;
		const double Lip = Ui.U * 0.7;
		const double Sink = bDown ? Lip * 0.7 : 0;
		const double R = FMath::Min(B.GetSize().Y * 0.5, Ui.U * 3.0);
		FLinearColor Face = bEnabled ? Col(Color) : Col(Muted);
		if (bHot && !bDown) { Face = CIMix(Face, FLinearColor::White, 0.12f); }
		const FLinearColor Under = CIMix(Face, FLinearColor::Black, 0.25f);
		D.Shadow(B.Min.X, B.Min.Y + Lip, B.Max.X, B.Max.Y + Lip, R, Ui.U * 0.9, FLinearColor(0.1f, 0.08f, 0.2f, 0.14f));
		D.RoundRect(B.Min.X, B.Min.Y + Lip, B.Max.X, B.Max.Y + Lip, R, Under);
		D.RoundRectV(B.Min.X, B.Min.Y + Sink, B.Max.X, B.Max.Y + Sink, R, CIMix(Face, FLinearColor::White, 0.14f), Face);

		const double Fs = Px > 0 ? Px : FMath::Min(B.GetSize().Y * 0.4, Ui.U * 3.6);
		const FVector2D C = B.GetCenter() + FVector2D(0, Sink);
		const FLinearColor TextC = FLinearColor::White;
		if (Icon != EIcon::None && Label.IsEmpty()) { DrawIcon(D, Icon, C.X, C.Y, B.GetSize().Y * 0.5, TextC); return; }
		const FVector2D Sz = D.MeasureText(Label, Fs, true);
		const double IconS = Icon != EIcon::None ? Fs * 1.1 : 0;
		const double Gap = Icon != EIcon::None ? Fs * 0.45 : 0;
		const double X0 = C.X - (Sz.X + IconS + Gap) * 0.5;
		if (Icon != EIcon::None) { DrawIcon(D, Icon, X0 + IconS * 0.5, C.Y, IconS, TextC); }
		D.Text(Label, X0 + IconS + Gap, C.Y, Fs, TextC, 0, 0.5, true);
	}

	void StarShape(FCIDraw& D, double X, double Y, double R, const FLinearColor& C)
	{
		TArray<FVector2D> P;
		for (int I = 0; I < 10; ++I)
		{
			const double A = -PI * 0.5 + I * PI / 5;
			const double Rr = I % 2 == 0 ? R : R * 0.45;
			P.Add(FVector2D(X + FMath::Cos(A) * Rr, Y + FMath::Sin(A) * Rr));
		}
		D.Polygon(P, C);
	}

	void Stars(FCIDraw& D, double X, double Y, double R, int32 Count, bool bDark = false)
	{
		for (int I = 0; I < 3; ++I)
		{
			const double SX = X + (I - 1) * R * 2.3;
			const bool bOn = I < Count;
			if (bOn) { StarShape(D, SX, Y + R * 0.08, R * 1.05, Col(0xd99a1e)); }
			StarShape(D, SX, Y, R, bOn ? Col(0xffc93c) : (bDark ? Col(0xffffff, 0.25f) : Col(0xd6dae6)));
		}
	}

	// FUN | STUDENT pill. The whole pill toggles (one tap), the active half is filled.
	void ModeToggle(FUi& Ui, double Right, double CY)
	{
		FCIDraw& D = Ui.D;
		const double Fs = Ui.U * 2.4;
		const double Hh = Ui.U * 6.0;
		const double HalfW = FMath::Max(D.MeasureText(TEXT("STUDENT"), Fs, true).X + Fs * 1.6, Ui.U * 11);
		const FBox2D B(FVector2D(Right - HalfW * 2, CY - Hh * 0.5), FVector2D(Right, CY + Hh * 0.5));
		FCIButton Btn;
		Btn.Box = B;
		Btn.Action = ECIAction::ToggleMode;
		Ui.G.Buttons.Add(Btn);
		const bool bStudent = Ui.G.IsStudent();
		D.Shadow(B.Min.X, B.Min.Y + Ui.U * 0.4, B.Max.X, B.Max.Y + Ui.U * 0.4, Hh * 0.5, Ui.U * 0.8, FLinearColor(0.1f, 0.08f, 0.2f, 0.14f));
		D.RoundRect(B.Min.X, B.Min.Y, B.Max.X, B.Max.Y, Hh * 0.5, Col(Cream, Hot(Ui, B) ? 1.f : 0.95f));
		const double Pad = Ui.U * 0.5;
		const double Knob0 = bStudent ? B.Min.X + HalfW : B.Min.X;
		D.RoundRect(Knob0 + Pad, B.Min.Y + Pad, Knob0 + HalfW - Pad, B.Max.Y - Pad, Hh * 0.5 - Pad, bStudent ? Col(Blue) : Col(Coral));
		D.Text(TEXT("FUN"), B.Min.X + HalfW * 0.5, CY, Fs, bStudent ? Col(InkSoft) : FLinearColor::White, 0.5, 0.5, true);
		D.Text(TEXT("STUDENT"), B.Min.X + HalfW * 1.5, CY, Fs, bStudent ? FLinearColor::White : Col(InkSoft), 0.5, 0.5, true);
	}

	// Professor Newton: a grumpy but lovable pigeon in round glasses and a tiny graduation cap.
	void Newton(FCIDraw& D, double X, double Y, double S, double Time)
	{
		const double Bob = FMath::Sin(Time * 2.2) * S * 0.03;
		D.Ellipse(X, Y + S * 0.55 + Bob, S * 0.62, S * 0.5, Col(0x8f97ad), 28);
		D.Ellipse(X - S * 0.05, Y + S * 0.45 + Bob, S * 0.34, S * 0.3, Col(0x9fd3c7), 20);
		D.Circle(X, Y + Bob, S * 0.42, Col(0xa7afc4), 28);
		D.Tri(X + S * 0.34, Y + S * 0.02 + Bob, X + S * 0.64, Y + S * 0.12 + Bob, X + S * 0.34, Y + S * 0.2 + Bob, Col(0xffb347));
		for (double Ex : { -0.13, 0.17 })
		{
			D.Circle(X + S * Ex, Y - S * 0.04 + Bob, S * 0.1, FLinearColor::White, 16);
			D.Circle(X + S * Ex + S * 0.02, Y - S * 0.03 + Bob, S * 0.045, Col(Ink), 10);
			D.Ring(X + S * Ex, Y - S * 0.04 + Bob, S * 0.11, S * 0.15, Col(0x3a3f5c), 20);
		}
		D.Line(X + S * -0.02, Y - S * 0.05 + Bob, X + S * 0.06, Y - S * 0.05 + Bob, S * 0.04, Col(0x3a3f5c));
		// Grumpy brows.
		D.Line(X - S * 0.25, Y - S * 0.22 + Bob, X - S * 0.03, Y - S * 0.16 + Bob, S * 0.05, Col(0x5b6070));
		D.Line(X + S * 0.07, Y - S * 0.16 + Bob, X + S * 0.29, Y - S * 0.22 + Bob, S * 0.05, Col(0x5b6070));
		// Cap.
		D.Quad(FVector2D(X - S * 0.4, Y - S * 0.44 + Bob), FVector2D(X + S * 0.12, Y - S * 0.58 + Bob), FVector2D(X + S * 0.46, Y - S * 0.44 + Bob), FVector2D(X - S * 0.06, Y - S * 0.32 + Bob), Col(0x2b2f45));
		D.Rect(X - S * 0.14, Y - S * 0.42 + Bob, X + S * 0.2, Y - S * 0.3 + Bob, Col(0x2b2f45));
		D.Line(X + S * 0.3, Y - S * 0.46 + Bob, X + S * 0.36, Y - S * 0.22 + Bob, S * 0.03, Col(0xffc93c));
	}

	// ------------------------------------------------------------ screens

	void DrawTitle(FUi& Ui)
	{
		FCIDraw& D = Ui.D;
		const double In = FMath::Clamp(Ui.G.ScreenTime / 0.6, 0.0, 1.0);
		const double Ts = Ui.H * 0.13;
		const double TY = Ui.H * 0.2;
		D.Text(TEXT("CURIO ISLES"), Ui.W * 0.5 + Ts * 0.04, TY + Ts * 0.06, Ts, Col(0x1d4f7a, 0.35f * (float)In), 0.5, 0.5, true);
		D.Text(TEXT("CURIO ISLES"), Ui.W * 0.5, TY, Ts, Col(Cream, (float)In), 0.5, 0.5, true);
		D.Text(TEXT("Break it.  Fix it.  Understand it."), Ui.W * 0.5, TY + Ts * 0.75, Ui.U * 3.4, Col(0x1d3f63, (float)In * 0.9f), 0.5, 0.5, true);

		const double BW = Ui.U * 30, BH = Ui.U * 11;
		Button(Ui, FBox2D(FVector2D(Ui.W * 0.5 - BW * 0.5, Ui.H * 0.4), FVector2D(Ui.W * 0.5 + BW * 0.5, Ui.H * 0.4 + BH)), TEXT("PLAY"), EIcon::Play, Green, ECIAction::OpenLevels, 0, true, Ui.U * 4.6);

		const FString Island = Ui.G.bLoaded ? FString::Printf(TEXT("Island 1 · %s · %s"), *ToF(Ui.G.Island.Name), *ToF(Ui.G.Island.Subject)) : Ui.G.LoadError;
		const double Fs = Ui.U * 2.6;
		const FVector2D Sz = D.MeasureText(Island, Fs, true);
		const double BY = Ui.H - Ui.SB - Ui.U * 6;
		D.RoundRect(Ui.W - Ui.SR - Ui.U * 3 - Sz.X - Fs, BY - Fs, Ui.W - Ui.SR - Ui.U * 3 + Fs * 0.2, BY + Fs, Fs, Col(0x10284a, 0.45f));
		D.Text(Island, Ui.W - Ui.SR - Ui.U * 3 - Fs * 0.4, BY, Fs, FLinearColor::White, 1, 0.5, true);

		if (FCIGame::PlatformHasQuitButton())
		{
			const double S = Ui.U * 7;
			Button(Ui, FBox2D(FVector2D(Ui.W - Ui.SR - Ui.U * 3 - S * 2.2, Ui.ST + Ui.U * 3), FVector2D(Ui.W - Ui.SR - Ui.U * 3, Ui.ST + Ui.U * 3 + S)), TEXT("QUIT"), EIcon::None, Coral, ECIAction::Quit, 0, true, Ui.U * 2.6);
		}
	}

	void DrawTopBar(FUi& Ui, const FString& Title, const FString& Sub)
	{
		FCIDraw& D = Ui.D;
		const double CY = Ui.ST + Ui.U * 6.5;
		const double S = Ui.U * 8;
		Button(Ui, FBox2D(FVector2D(Ui.SL + Ui.U * 2.5, CY - S * 0.5), FVector2D(Ui.SL + Ui.U * 2.5 + S, CY + S * 0.5)), FString(), EIcon::Back, Blue, ECIAction::Back);
		const double TX = Ui.SL + Ui.U * 2.5 + S + Ui.U * 2.5;
		D.Text(Title, TX + Ui.U * 0.15, CY - Ui.U * 1.9 + Ui.U * 0.25, Ui.U * 4.0, Col(0x10284a, 0.3f), 0, 0.5, true);
		D.Text(Title, TX, CY - Ui.U * 1.9, Ui.U * 4.0, FLinearColor::White, 0, 0.5, true);
		D.Text(Sub, TX, CY + Ui.U * 2.4, Ui.U * 2.6, Col(0xffffff, 0.95f), 0, 0.5, true);
		ModeToggle(Ui, Ui.W - Ui.SR - Ui.U * 2.5, CY);
	}

	// A round button with just an icon (the in-level controls and the result card).
	void RoundButton(FUi& Ui, double CX, double CY, double R, EIcon Icon, uint32 Color, ECIAction Action, int32 Param = 0)
	{
		FCIDraw& D = Ui.D;
		const FBox2D B(FVector2D(CX - R, CY - R), FVector2D(CX + R, CY + R));
		FCIButton Btn;
		Btn.Box = B.ExpandBy(R * 0.15);
		Btn.Action = Action;
		Btn.Param = Param;
		Ui.G.Buttons.Add(Btn);
		const bool bHot = Hot(Ui, Btn.Box);
		const bool bDown = bHot && Ui.P.bDown;
		const double Lip = R * 0.14, Sink = bDown ? Lip * 0.7 : 0;
		FLinearColor Face = Col(Color);
		if (bHot && !bDown) { Face = CIMix(Face, FLinearColor::White, 0.12f); }
		D.Circle(CX, CY + Lip * 2.2, R * 1.04, FLinearColor(0.1f, 0.08f, 0.2f, 0.18f), 40);
		D.Circle(CX, CY + Lip, R, CIMix(Face, FLinearColor::Black, 0.25f), 40);
		D.Circle(CX, CY + Sink, R, Face, 40);
		D.Ellipse(CX, CY + Sink - R * 0.35, R * 0.78, R * 0.5, FLinearColor(1, 1, 1, 0.14f), 32);
		DrawIcon(D, Icon, CX + (Icon == EIcon::Play ? R * 0.06 : 0), CY + Sink, R * 1.0, FLinearColor::White);
	}

	// Level select, Angry Birds style: a row of big numbered tiles per world, stars under each, locks ahead.
	void DrawLevels(FUi& Ui)
	{
		FCIDraw& D = Ui.D;
		FCIGame& G = Ui.G;
		if (!G.bLoaded) { D.Text(G.LoadError, Ui.W * 0.5, Ui.H * 0.5, Ui.U * 3, FLinearColor::White, 0.5, 0.5, true); return; }
		DrawTopBar(Ui, ToF(G.Island.Name), FString::Printf(TEXT("%s  ·  with %s"), *ToF(G.Island.Subject), *ToF(G.Island.Mentor)));

		const int32 NumWorlds = (int32)G.Island.Worlds.size();
		const double Top = Ui.ST + Ui.U * 16, Bottom = Ui.H - Ui.SB - Ui.U * 3;
		const double BandH = FMath::Min(Ui.U * 38, (Bottom - Top) / FMath::Max(1, NumWorlds) - Ui.U * 2);
		const double X0 = Ui.SL + Ui.U * 4, X1 = Ui.W - Ui.SR - Ui.U * 4;
		for (int32 Wi = 0; Wi < NumWorlds; ++Wi)
		{
			const CI::FWorldDef& W = G.Island.Worlds[Wi];
			const double Y0 = Top + Wi * (BandH + Ui.U * 2);
			const FBox2D B(FVector2D(X0, Y0), FVector2D(X1, Y0 + BandH));
			Card(Ui, B, Cream, Ui.U * 3);
			// A strip in the world's own colours down the left edge.
			D.RoundRectV(B.Min.X, B.Min.Y, B.Min.X + Ui.U * 34, B.Max.Y, Ui.U * 3, Col(W.Theme.SkyTop), Col(W.Theme.SkyBottom));
			D.RoundRect(B.Min.X, B.Max.Y - BandH * 0.26, B.Min.X + Ui.U * 34, B.Max.Y, Ui.U * 3, Col(W.Theme.Ground));
			D.Rect(B.Min.X, B.Max.Y - BandH * 0.32, B.Min.X + Ui.U * 34, B.Max.Y - BandH * 0.2, Col(W.Theme.Hills[2]));
			D.Text(FString::Printf(TEXT("WORLD %d"), W.Number), B.Min.X + Ui.U * 3, B.Min.Y + Ui.U * 4, Ui.U * 2.2, Col(0x10284a, 0.75f), 0, 0.5, true);
			D.TextWrapped(ToF(W.Name), B.Min.X + Ui.U * 3, B.Min.Y + Ui.U * 6.5, Ui.U * 29, Ui.U * 3.8, 1.15, Col(Ink), 0, true);

			const double Tile = FMath::Min(BandH - Ui.U * 12, Ui.U * 19);
			const double Gap = Ui.U * 3;
			double TX = B.Min.X + Ui.U * 38;
			const double TY = B.GetCenter().Y - Tile * 0.5 - Ui.U * 2.5;
			for (int32 Li = 0; Li < (int32)W.Levels.size() && TX + Tile < B.Max.X - Ui.U * 2; ++Li, TX += Tile + Gap)
			{
				const CI::FLevelDef& L = W.Levels[Li];
				const bool bOpen = G.IsUnlocked(Wi, Li);
				const FBox2D TB(FVector2D(TX, TY), FVector2D(TX + Tile, TY + Tile));
				FCIButton Btn;
				Btn.Box = TB;
				Btn.Action = ECIAction::StartLevel;
				Btn.Param = Wi * 100 + Li;
				Btn.bEnabled = bOpen;
				G.Buttons.Add(Btn);
				const bool bHot = bOpen && Hot(Ui, TB);
				const double Sink = bHot && Ui.P.bDown ? Ui.U * 0.6 : 0;
				const FLinearColor Face = bOpen ? Col(W.Theme.Accent) : Col(Muted);
				D.RoundRect(TB.Min.X, TB.Min.Y + Ui.U * 1.0, TB.Max.X, TB.Max.Y + Ui.U * 1.0, Ui.U * 3.5, CIMix(Face, FLinearColor::Black, 0.25f));
				D.RoundRectV(TB.Min.X, TB.Min.Y + Sink, TB.Max.X, TB.Max.Y + Sink, Ui.U * 3.5, CIMix(Face, FLinearColor::White, bHot ? 0.3f : 0.18f), Face);
				if (bOpen) { D.Text(FString::Printf(TEXT("%d"), Li + 1), TB.GetCenter().X, TB.GetCenter().Y + Sink - Ui.U * 0.5, Tile * 0.5, FLinearColor::White, 0.5, 0.5, true); }
				else { DrawIcon(D, EIcon::Lock, TB.GetCenter().X, TB.GetCenter().Y, Tile * 0.42, FLinearColor(1, 1, 1, 0.85f)); }
				const int32* Best = G.Save ? G.Save->Stars.Find(ToF(L.Id)) : nullptr;
				if (bOpen) { Stars(D, TB.GetCenter().X, TB.Max.Y + Ui.U * 4.2, Tile * 0.11, Best ? *Best : 0); }
			}
		}
	}

	// ------------------------------------------------------------ in-level HUD

	void DrawResultCard(FUi& Ui)
	{
		FCIDraw& D = Ui.D;
		FCIGame& G = Ui.G;
		const CI::FLevelDef& L = *G.CurrentLevel();
		const bool bStudent = G.IsStudent();
		const double In = FMath::Clamp((G.PhaseTime - 1.0) / 0.35, 0.0, 1.0);
		if (In <= 0) { return; }
		const double Ease = 1.0 - FMath::Pow(1.0 - In, 3.0);

		const double CW = FMath::Min(Ui.U * 86, Ui.W * 0.7);
		const double Pad = Ui.U * 4;
		const CI::FVarLookup Vars = [&G](const std::string& N, double& V) { return G.Sim.Variable(N, V); };
		const FString Why = ToF(CI::FormatTemplate(bStudent ? L.WhyStudent : L.Why, Vars));
		const FString Example = ToF(CI::FormatTemplate(L.Example, Vars));
		const TArray<FString> WhyLines = D.Wrap(Why, CW - Pad * 2, Ui.U * 2.6);
		const TArray<FString> ExLines = bStudent ? D.Wrap(Example, CW - Pad * 2, Ui.U * 2.4) : TArray<FString>();
		double CH = Ui.U * 25 + WhyLines.Num() * Ui.U * 3.6 + Ui.U * 22;
		if (bStudent) { CH += Ui.U * 8 + ExLines.Num() * Ui.U * 3.3 + Ui.U * 6; }
		const double X0 = (G.WorldBox.Min.X + G.WorldBox.Max.X) * 0.5 - CW * 0.5;
		const double Y0 = FMath::Max(G.WorldBox.Min.Y + Ui.U * 1, (G.WorldBox.Min.Y + G.WorldBox.Max.Y) * 0.5 - CH * 0.5) + (1 - Ease) * Ui.U * 8;
		const FBox2D B(FVector2D(X0, Y0), FVector2D(X0 + CW, Y0 + CH));
		D.Rect(0, 0, Ui.W, Ui.H, FLinearColor(0.05f, 0.05f, 0.15f, 0.25f * (float)Ease));
		Card(Ui, B);

		// Ribbon.
		D.RoundRectV(X0 + Pad, Y0 - Ui.U * 3, X0 + CW - Pad, Y0 + Ui.U * 5, Ui.U * 2.5, Col(0x4fd382), Col(Green));
		D.Text(TEXT("MACHINE FIXED!"), X0 + CW * 0.5, Y0 + Ui.U * 1, Ui.U * 4, FLinearColor::White, 0.5, 0.5, true);
		// Stars pop in one after another.
		const int32 Shown = FMath::Clamp((int32)((G.PhaseTime - 1.2) / 0.3) + 1, 0, G.Stars);
		Stars(D, X0 + CW * 0.5, Y0 + Ui.U * 11, Ui.U * (3.2 + 0.5 * FMath::Max(0.0, 1.0 - FMath::Fmod(FMath::Max(0.0, G.PhaseTime - 1.2), 0.3) * 6.0) * (Shown < G.Stars ? 1 : 0)), Shown);
		const FString Tries = G.Attempts == 1 ? FString(TEXT("First try!")) : FString::Printf(TEXT("Solved in %d tries"), G.Attempts);
		D.Text(Tries, X0 + CW * 0.5, Y0 + Ui.U * 16.5, Ui.U * 1.9, Col(InkSoft), 0.5, 0.5, false);

		double Y = Y0 + Ui.U * 21;
		if (bStudent)
		{
			D.RoundRect(X0 + Pad, Y, X0 + CW - Pad, Y + Ui.U * 7, Ui.U * 1.6, Col(0xeef1fb));
			D.Text(ToF(L.Formula), X0 + CW * 0.5, Y + Ui.U * 3.5, Ui.U * 4.2, Col(Blue), 0.5, 0.5, true);
			Y += Ui.U * 9;
			for (const FString& Line : ExLines) { D.Text(Line, X0 + Pad, Y + Ui.U * 1.4, Ui.U * 2.4, Col(Ink), 0, 0.5, true); Y += Ui.U * 3.3; }
			Y += Ui.U * 1.5;
		}
		D.Text(TEXT("WHY IT WORKED"), X0 + Pad, Y + Ui.U * 1.2, Ui.U * 2, Col(Coral), 0, 0.5, true);
		Y += Ui.U * 3.6;
		for (const FString& Line : WhyLines) { D.Text(Line, X0 + Pad, Y + Ui.U * 1.4, Ui.U * 2.6, Col(Ink), 0, 0.5, false); Y += Ui.U * 3.6; }
		if (bStudent && !L.Syllabus.empty())
		{
			double CX = X0 + Pad;
			Y += Ui.U * 1.5;
			for (const std::string& Tag : L.Syllabus)
			{
				const FString T = ToF(Tag);
				const FVector2D Sz = D.MeasureText(T, Ui.U * 1.9, true);
				D.RoundRect(CX, Y, CX + Sz.X + Ui.U * 2.4, Y + Ui.U * 3.6, Ui.U * 1.8, Col(0xfff0c9));
				D.Text(T, CX + Ui.U * 1.2, Y + Ui.U * 1.8, Ui.U * 1.9, Col(0x9a6a10), 0, 0.5, true);
				CX += Sz.X + Ui.U * 3.4;
			}
		}
		// Levels / again / next, as three round buttons.
		int32 NW, NL;
		const bool bNext = G.FindNextLevel(NW, NL);
		const double BY = B.Max.Y - Ui.U * 7.5;
		RoundButton(Ui, X0 + CW * 0.5 - Ui.U * 17, BY, Ui.U * 5, EIcon::Back, Blue, ECIAction::Back);
		RoundButton(Ui, X0 + CW * 0.5, BY, Ui.U * 5, EIcon::Retry, Coral, ECIAction::Retry);
		RoundButton(Ui, X0 + CW * 0.5 + Ui.U * 18, BY - Ui.U * 0.5, Ui.U * 6.5, bNext ? EIcon::Next : EIcon::Back, Green, bNext ? ECIAction::NextLevel : ECIAction::Back);
	}

	void DrawFailBubble(FUi& Ui)
	{
		FCIDraw& D = Ui.D;
		FCIGame& G = Ui.G;
		const double In = FMath::Clamp((G.PhaseTime - 0.5) / 0.3, 0.0, 1.0);
		if (In <= 0) { return; }
		const double S = Ui.U * 10;
		const double NX = G.WorldBox.Min.X + Ui.U * 9, NY = G.WorldBox.Min.Y + Ui.U * 8;
		Newton(D, NX, NY, S, G.RealTime);
		const FString Line1 = G.FailText;
		const FString Line2 = TEXT("Tweak it and try again.");
		const double W1 = FMath::Max(D.MeasureText(Line1, Ui.U * 3.4, true).X, D.MeasureText(Line2, Ui.U * 2.4).X);
		const double BX = NX + S * 0.8, BY = NY - S * 0.6;
		const FBox2D B(FVector2D(BX, BY), FVector2D(BX + W1 + Ui.U * 6, BY + Ui.U * 11));
		Card(Ui, B, Cream, Ui.U * 3);
		D.Tri(BX + Ui.U * 0.5, BY + Ui.U * 6, BX - Ui.U * 2.5, BY + Ui.U * 8.5, BX + Ui.U * 0.5, BY + Ui.U * 9, Col(Cream));
		D.Text(Line1, BX + Ui.U * 3, BY + Ui.U * 3.8, Ui.U * 3.4, Col(Coral), 0, 0.5, true);
		D.Text(Line2, BX + Ui.U * 3, BY + Ui.U * 8, Ui.U * 2.4, Col(InkSoft), 0, 0.5, false);
	}

	void DrawReadout(FUi& Ui)
	{
		FCIDraw& D = Ui.D;
		const FCIGame& G = Ui.G;
		if (G.Sim.Bodies.empty()) { return; }
		const CI::FBody& B = G.Sim.Bodies[0];
		const FString Lines[3] = {
			FString::Printf(TEXT("t   %.2f s"), G.Sim.Time),
			FString::Printf(TEXT("v   %.2f m/s"), B.V.Length()),
			FString::Printf(TEXT("d   %.2f m"), B.Distance)
		};
		const double X0 = G.WorldBox.Min.X + Ui.U * 2.5, Y0 = G.WorldBox.Min.Y + Ui.U * 2;
		D.RoundRect(X0, Y0, X0 + Ui.U * 22, Y0 + Ui.U * 13, Ui.U * 2, Col(0x1f2440, 0.78f));
		for (int I = 0; I < 3; ++I) { D.Text(Lines[I], X0 + Ui.U * 2.2, Y0 + Ui.U * (2.8 + I * 3.8), Ui.U * 2.5, I == 1 ? Col(0xffd23f) : FLinearColor::White, 0, 0.5, true); }
	}

	// In a level there is almost no interface: the machine fills the screen, one big round button
	// starts it, and a miss resets by itself.
	void DrawPlaying(FUi& Ui)
	{
		FCIDraw& D = Ui.D;
		FCIGame& G = Ui.G;
		const CI::FLevelDef* LP = G.CurrentLevel();
		const CI::FWorldDef* WP = G.CurrentWorld();
		if (!LP || !WP) { return; }
		const CI::FLevelDef& L = *LP;

		DrawTopBar(Ui, FString::Printf(TEXT("%d-%d  %s"), WP->Number, G.LevelIndex + 1, *ToF(L.Name)), ToF(L.Goal));
		{
			const int32* Best = G.Save ? G.Save->Stars.Find(ToF(L.Id)) : nullptr;
			const double SX = Ui.W - Ui.SR - Ui.U * 2.5 - Ui.U * 30 - Ui.U * 10;
			Stars(D, SX, Ui.ST + Ui.U * 6.5, Ui.U * 1.7, Best ? *Best : 0, true);
		}
		if (G.IsStudent() && G.Phase != ECIPhase::Build) { DrawReadout(Ui); }

		const double R = Ui.U * 8.5;
		const double BX = Ui.W - Ui.SR - Ui.U * 4 - R, BY = Ui.H - Ui.SB - Ui.U * 4.5 - R;
		const double LX = Ui.SL + Ui.U * 4 + R * 0.7;
		bool bLauncher = false;
		for (const CI::FPlacedPart& Pp : G.Setup.Placed) { bLauncher = bLauncher || L.Tray[Pp.Tray].Part == "launcher"; }

		if (G.Phase == ECIPhase::Build)
		{
			// One line that says what to do, sitting under the machine.
			const FString Hint = bLauncher ? TEXT("Pull the ball back and let go!")
				: (L.Slots.size() > 1 ? TEXT("Drag the glowing grip. Drag the part to move it. Then press play.") : TEXT("Drag the glowing grip, then press play."));
			const double Fs = Ui.U * 2.7;
			const FVector2D Sz = D.MeasureText(Hint, Fs, true);
			const double HY = Ui.H - Ui.SB - Ui.U * 6;
			D.RoundRect(Ui.W * 0.5 - Sz.X * 0.5 - Fs, HY - Fs * 1.1, Ui.W * 0.5 + Sz.X * 0.5 + Fs, HY + Fs * 1.1, Fs * 1.1, Col(0x10284a, 0.55f));
			D.Text(Hint, Ui.W * 0.5, HY, Fs, FLinearColor::White, 0.5, 0.5, true);
			// Launchers fire when you let go, so their PLAY button is small and secondary.
			const double Pulse = bLauncher ? 0.7 : 1.0 + 0.04 * FMath::Sin(G.RealTime * 4.0);
			RoundButton(Ui, BX + R * (1 - Pulse) * 0.5, BY + R * (1 - Pulse) * 0.5, R * Pulse, EIcon::Play, Green, ECIAction::Run);
		}
		else if (G.Phase == ECIPhase::Running)
		{
			RoundButton(Ui, BX + R * 0.15, BY + R * 0.15, R * 0.85, EIcon::Retry, Coral, ECIAction::Retry);
			RoundButton(Ui, LX, BY + R * 0.3, R * 0.7, EIcon::Slow, G.bSlow ? Yellow : 0x8d93a8, ECIAction::SlowMo);
		}
		else if (G.bSuccess) { DrawResultCard(Ui); }
		else { DrawFailBubble(Ui); }
	}

	void DrawToast(FUi& Ui)
	{
		FCIGame& G = Ui.G;
		if (G.Toast.IsEmpty() || G.RealTime > G.ToastUntil) { return; }
		const float A = (float)FMath::Clamp((G.ToastUntil - G.RealTime) / 0.3, 0.0, 1.0);
		const double Fs = Ui.U * 2.6;
		const FVector2D Sz = Ui.D.MeasureText(G.Toast, Fs, true);
		const double CX = Ui.W * 0.5, CY = Ui.ST + Ui.U * 17;
		Ui.D.RoundRect(CX - Sz.X * 0.5 - Fs, CY - Fs * 1.1, CX + Sz.X * 0.5 + Fs, CY + Fs * 1.1, Fs * 1.1, Col(0x1f2440, 0.85f * A));
		Ui.D.Text(G.Toast, CX, CY, Fs, FLinearColor(1, 1, 1, A), 0.5, 0.5, true);
	}
}

void FCIUI::Layout(FCIDraw& D, FCIGame& G)
{
	const double U = D.ScreenH / 100.0;
	G.TopBarH = G.Safe.Y + U * 13;
	G.TrayH = 0;   // no tray: the machine fills the screen
}

void FCIUI::Draw(FCIDraw& D, FCIGame& G, const FCIPointer& Pointer)
{
	D.SetScreenSpace();
	G.Buttons.Reset();
	G.TrayCards.Reset();
	G.Sliders.Reset();
	G.PanelBox = FBox2D(ForceInit);
	G.TrayBox = FBox2D(ForceInit);
	const double U = D.ScreenH / 100.0;
	FUi Ui{ D, G, Pointer, D.ScreenW, D.ScreenH, U, G.Safe.X, G.Safe.Y, G.Safe.Z, G.Safe.W };

	switch (G.Screen)
	{
	case ECIScreen::Title: DrawTitle(Ui); break;
	case ECIScreen::Levels: DrawLevels(Ui); break;
	case ECIScreen::Playing: DrawPlaying(Ui); break;
	}
	DrawToast(Ui);
}
