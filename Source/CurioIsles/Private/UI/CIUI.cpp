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

	// "speed-from-height" -> "Speed from height"
	FString Pretty(const std::string& Id)
	{
		FString S = ToF(Id).Replace(TEXT("-"), TEXT(" "));
		if (S.Len() > 0) { S[0] = FChar::ToUpper(S[0]); }
		return S;
	}
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

	int32 LevelNumber(const std::string& Id)
	{
		const size_t Dot = Id.rfind('.');
		return Dot == std::string::npos ? 0 : std::atoi(Id.c_str() + Dot + 1);
	}

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

	void DrawLevels(FUi& Ui)
	{
		FCIDraw& D = Ui.D;
		FCIGame& G = Ui.G;
		if (!G.bLoaded) { D.Text(G.LoadError, Ui.W * 0.5, Ui.H * 0.5, Ui.U * 3, FLinearColor::White, 0.5, 0.5, true); return; }
		DrawTopBar(Ui, ToF(G.Island.Name), FString::Printf(TEXT("%s  ·  with %s"), *ToF(G.Island.Subject), *ToF(G.Island.Mentor)));

		const int32 NumCards = (int32)G.Island.Worlds.size() + 1;
		const double Gap = Ui.U * 3;
		const double Top = Ui.ST + Ui.U * 16, Bottom = Ui.H - Ui.SB - Ui.U * 9;
		const double Avail = Ui.W - Ui.SL - Ui.SR - Ui.U * 6;
		const double CW = FMath::Min(Ui.U * 52, (Avail - Gap * (NumCards - 1)) / NumCards);
		double X = (Ui.W - (CW * NumCards + Gap * (NumCards - 1))) * 0.5;
		for (int32 Wi = 0; Wi < (int32)G.Island.Worlds.size(); ++Wi, X += CW + Gap)
		{
			const CI::FWorldDef& W = G.Island.Worlds[Wi];
			const FBox2D B(FVector2D(X, Top), FVector2D(X + CW, Bottom));
			Card(Ui, B);
			// Header painted in the world's own palette.
			const double HH = Ui.U * 15;
			D.RoundRectV(B.Min.X, B.Min.Y, B.Max.X, B.Min.Y + HH, Ui.U * 2.4, Col(W.Theme.SkyTop), Col(W.Theme.SkyBottom));
			D.Rect(B.Min.X, B.Min.Y + HH - Ui.U * 3.4, B.Max.X, B.Min.Y + HH, Col(W.Theme.Hills[1]));
			D.Rect(B.Min.X, B.Min.Y + HH - Ui.U * 1.4, B.Max.X, B.Min.Y + HH, Col(W.Theme.Ground));
			D.Text(FString::Printf(TEXT("WORLD %d"), W.Number), B.Min.X + Ui.U * 2.5, B.Min.Y + Ui.U * 3.3, Ui.U * 2.2, Col(0x10284a, 0.8f), 0, 0.5, true);
			D.Text(ToF(W.Name), B.Min.X + Ui.U * 2.5, B.Min.Y + Ui.U * 7.5, Ui.U * 3.8, Col(Ink), 0, 0.5, true);
			D.TextWrapped(ToF(W.Teaches), B.Min.X + Ui.U * 2.5, B.Min.Y + HH + Ui.U * 1.5, CW - Ui.U * 5, Ui.U * 2.2, 1.3, Col(InkSoft));

			double Y = B.Min.Y + HH + Ui.U * 8;
			for (int32 Li = 0; Li < (int32)W.Levels.size(); ++Li)
			{
				const CI::FLevelDef& L = W.Levels[Li];
				const FBox2D RB(FVector2D(B.Min.X + Ui.U * 2, Y), FVector2D(B.Max.X - Ui.U * 2, Y + Ui.U * 9));
				FCIButton Btn;
				Btn.Box = RB;
				Btn.Action = ECIAction::StartLevel;
				Btn.Param = Wi * 100 + Li;
				G.Buttons.Add(Btn);
				const bool bHot = Hot(Ui, RB);
				D.RoundRect(RB.Min.X, RB.Min.Y, RB.Max.X, RB.Max.Y, Ui.U * 2, bHot ? Col(0xfff0c9) : Col(0xf3eee2));
				const double CX = RB.Min.X + Ui.U * 4.8, CY = RB.GetCenter().Y;
				D.Circle(CX, CY + Ui.U * 0.4, Ui.U * 3.3, Col(0x23984f), 28);
				D.Circle(CX, CY, Ui.U * 3.3, Col(Green), 28);
				D.Text(FString::Printf(TEXT("%d"), LevelNumber(L.Id)), CX, CY, Ui.U * 3, FLinearColor::White, 0.5, 0.5, true);
				D.Text(ToF(L.Name), CX + Ui.U * 5, CY - Ui.U * 1.3, Ui.U * 2.8, Col(Ink), 0, 0.5, true);
				D.Text(Pretty(L.Concept), CX + Ui.U * 5, CY + Ui.U * 1.9, Ui.U * 1.9, Col(InkSoft), 0, 0.5, false);
				const int32* Best = G.Save ? G.Save->Stars.Find(ToF(L.Id)) : nullptr;
				Stars(D, RB.Max.X - Ui.U * 6.5, CY, Ui.U * 1.4, Best ? *Best : 0);
				Y += Ui.U * 11;
			}
		}
		// The rest of the island is on its way.
		const FBox2D B(FVector2D(X, Top), FVector2D(X + CW, Bottom));
		Card(Ui, B, 0xe9ecf5);
		DrawIcon(D, EIcon::Lock, B.GetCenter().X, B.Min.Y + Ui.U * 10, Ui.U * 7, Col(Muted));
		D.Text(TEXT("More worlds"), B.GetCenter().X, B.Min.Y + Ui.U * 18, Ui.U * 3.4, Col(InkSoft), 0.5, 0.5, true);
		D.TextWrapped(TEXT("Force Forest, Energy Waterworks, Fluid Lagoon, Wave Beach, Heat Volcano, Light Caves, Spark City, Sky Station"),
			B.Min.X + Ui.U * 3, B.Min.Y + Ui.U * 23, CW - Ui.U * 6, Ui.U * 2.2, 1.35, Col(InkSoft));

		if (G.Save)
		{
			const FString Nb = FString::Printf(TEXT("Lab Notebook: %d concept card%s"), G.Save->Notebook.Num(), G.Save->Notebook.Num() == 1 ? TEXT("") : TEXT("s"));
			D.Text(Nb, Ui.W * 0.5, Ui.H - Ui.SB - Ui.U * 4.5, Ui.U * 2.6, FLinearColor::White, 0.5, 0.5, true);
		}
	}

	// ------------------------------------------------------------ in-level HUD

	void DrawTrayIcon(FCIDraw& D, const std::string& Behavior, double X, double Y, double S, bool bEnabled)
	{
		const float A = bEnabled ? 1.f : 0.45f;
		if (Behavior == "ramp")
		{
			D.Tri(X - S * 0.5, Y + S * 0.35, X + S * 0.5, Y + S * 0.35, X - S * 0.5, Y - S * 0.3, Col(0xf0c98f, A));
			D.RoundLine(X - S * 0.5, Y - S * 0.32, X + S * 0.5, Y + S * 0.33, S * 0.1, Col(0x8a5a2b, A));
			D.Circle(X - S * 0.3, Y - S * 0.34, S * 0.13, Col(0xff5d57, A), 16);
		}
		else if (Behavior == "launcher")
		{
			D.RoundLine(X - S * 0.25, Y + S * 0.05, X + S * 0.35, Y - S * 0.35, S * 0.26, Col(0x3d4260, A));
			D.Circle(X + S * 0.35, Y - S * 0.35, S * 0.1, Col(0xf2c14e, A), 12);
			D.Circle(X - S * 0.2, Y + S * 0.28, S * 0.17, Col(0x6b4a2e, A), 16);
			D.Circle(X + S * 0.15, Y + S * 0.28, S * 0.17, Col(0x6b4a2e, A), 16);
		}
		else
		{
			D.RoundRect(X - S * 0.55, Y + S * 0.05, X + S * 0.55, Y + S * 0.28, S * 0.06, Col(0x33313b, A));
			for (double Sx = -0.45; Sx < 0.45; Sx += 0.2) { D.Quad(FVector2D(X + S * Sx, Y + S * 0.28), FVector2D(X + S * (Sx + 0.08), Y + S * 0.28), FVector2D(X + S * (Sx + 0.14), Y + S * 0.05), FVector2D(X + S * (Sx + 0.06), Y + S * 0.05), Col(0xffc93c, A)); }
		}
	}

	std::string BehaviorName(const CI::FPartDef* Def)
	{
		if (!Def) { return ""; }
		switch (Def->Behavior)
		{
		case CI::EPartBehavior::Ramp: return "ramp";
		case CI::EPartBehavior::Launcher: return "launcher";
		default: return "brake";
		}
	}

	// "1.20 m", "45°" (no space before a degree sign).
	FString WithUnit(double Value, const CI::FParamDef& Q)
	{
		const FString N = ToF(CI::FormatNumber(Value, Q.Decimals));
		if (Q.Unit.empty()) { return N; }
		return Q.Unit == "°" ? N + ToF(Q.Unit) : N + TEXT(" ") + ToF(Q.Unit);
	}

	// The selected part's sliders, laid out in the tray so they never cover the machine.
	void DrawSliderStrip(FUi& Ui, const FBox2D& Box, int32 Placed, int32 Fixed, const std::vector<CI::FParamDef>& Params)
	{
		FCIDraw& D = Ui.D;
		FCIGame& G = Ui.G;
		const bool bStudent = G.IsStudent();
		G.PanelBox = Box;
		double X0 = Box.Min.X;
		if (Placed >= 0)
		{
			// "Put back" as a round icon button at the start of the strip.
			const double S = Ui.U * 7.5;
			const double CY = Box.GetCenter().Y;
			Button(Ui, FBox2D(FVector2D(X0, CY - S * 0.5), FVector2D(X0 + S, CY + S * 0.5)), FString(), EIcon::Trash, 0x8d93a8, ECIAction::RemovePart);
			X0 += S + Ui.U * 3;
		}
		const int32 N = FMath::Max(1, (int32)Params.size());
		const double ColGap = Ui.U * 5;
		const double ColW = (Box.Max.X - X0 - ColGap * (N - 1)) / N;
		for (int32 K = 0; K < (int32)Params.size(); ++K)
		{
			const CI::FParamDef& Q = Params[K];
			const double CX0 = X0 + K * (ColW + ColGap), CX1 = CX0 + ColW;
			FCISliderWidget S;
			S.Placed = Placed;
			S.Fixed = Fixed;
			S.Param = K;
			const double Value = G.SliderValue(S);
			const double T = Q.Max > Q.Min ? (Value - Q.Min) / (Q.Max - Q.Min) : 0;
			const double TopY = Box.Min.Y + Ui.U * 2.2;
			D.Text(ToF(Q.Label), CX0, TopY, Ui.U * 2.6, Col(Ink), 0, 0.5, true);
			if (bStudent)
			{
				const FString Val = FString::Printf(TEXT("%s = %s"), *ToF(Q.Symbol), *WithUnit(Value, Q));
				D.Text(Val, CX1, TopY, Ui.U * 2.6, Col(Blue), 1, 0.5, true);
			}

			const double TX0 = CX0 + Ui.U * 1.5, TX1 = CX1 - Ui.U * 1.5, TY = Box.GetCenter().Y + Ui.U * 0.6;
			S.Track = FBox2D(FVector2D(TX0, TY - Ui.U * 1.5), FVector2D(TX1, TY + Ui.U * 1.5));
			G.Sliders.Add(S);
			const bool bGrab = G.SliderGrab == G.Sliders.Num() - 1;
			D.RoundRect(TX0 - Ui.U * 0.8, TY - Ui.U * 0.8, TX1 + Ui.U * 0.8, TY + Ui.U * 0.8, Ui.U * 0.8, Col(0xe3e6ef));
			const double KX = FMath::Lerp(TX0, TX1, T);
			D.RoundRectV(TX0 - Ui.U * 0.8, TY - Ui.U * 0.8, KX, TY + Ui.U * 0.8, Ui.U * 0.8, Col(0xffd76a), Col(Yellow));
			if (bStudent)
			{
				for (int32 Nt = 0; Nt <= 10; ++Nt)
				{
					const double NX = FMath::Lerp(TX0, TX1, Nt / 10.0);
					D.Line(NX, TY + Ui.U * 1.4, NX, TY + Ui.U * (Nt % 5 == 0 ? 2.4 : 1.9), 1.5, Col(Muted));
				}
			}
			const double KR = Ui.U * (bGrab ? 2.9 : 2.5);
			D.Circle(KX, TY + Ui.U * 0.35, KR, Col(0x1f2440, 0.18f), 24);
			D.Circle(KX, TY, KR, FLinearColor::White, 24);
			D.Ring(KX, TY, KR * 0.62, KR, Col(Coral), 24);
			const FString Lo = bStudent ? WithUnit(Q.Min, Q) : ToF(Q.Less);
			const FString Hi = bStudent ? WithUnit(Q.Max, Q) : ToF(Q.More);
			const double LY = Box.Max.Y - Ui.U * 2.0;
			D.Text(Lo, TX0 - Ui.U * 0.8, LY, Ui.U * 2.0, Col(InkSoft), 0, 0.5, false);
			D.Text(Hi, TX1 + Ui.U * 0.8, LY, Ui.U * 2.0, Col(InkSoft), 1, 0.5, false);
		}
	}


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
		double CH = Ui.U * 25 + WhyLines.Num() * Ui.U * 3.6 + Ui.U * 12;
		if (bStudent) { CH += Ui.U * 8 + ExLines.Num() * Ui.U * 3.3 + Ui.U * 6; }
		const double X0 = (G.WorldBox.Min.X + G.WorldBox.Max.X) * 0.5 - CW * 0.5;
		const double Y0 = FMath::Max(G.WorldBox.Min.Y + Ui.U * 1, (G.WorldBox.Min.Y + G.WorldBox.Max.Y) * 0.5 - CH * 0.5) + (1 - Ease) * Ui.U * 8;
		const FBox2D B(FVector2D(X0, Y0), FVector2D(X0 + CW, Y0 + CH));
		// Dim the world only: the tray buttons (RETRY / NEXT) stay bright.
		D.Rect(0, 0, Ui.W, G.TrayBox.bIsValid ? G.TrayBox.Min.Y - Ui.U * 0.5 : Ui.H, FLinearColor(0.05f, 0.05f, 0.15f, 0.25f * (float)Ease));
		Card(Ui, B);

		// Ribbon.
		D.RoundRectV(X0 + Pad, Y0 - Ui.U * 3, X0 + CW - Pad, Y0 + Ui.U * 5, Ui.U * 2.5, Col(0x4fd382), Col(Green));
		D.Text(TEXT("MACHINE FIXED!"), X0 + CW * 0.5, Y0 + Ui.U * 1, Ui.U * 4, FLinearColor::White, 0.5, 0.5, true);
		Stars(D, X0 + CW * 0.5, Y0 + Ui.U * 11, Ui.U * 3.2, G.Stars);
		D.Text(TEXT("solved   ·   within par   ·   predict (soon)"), X0 + CW * 0.5, Y0 + Ui.U * 16.5, Ui.U * 1.9, Col(InkSoft), 0.5, 0.5, false);

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
		D.Text(TEXT("New concept card added to your Lab Notebook"), X0 + CW * 0.5, B.Max.Y - Ui.U * 3.2, Ui.U * 1.9, Col(InkSoft), 0.5, 0.5, false);
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

	void DrawPlaying(FUi& Ui)
	{
		FCIDraw& D = Ui.D;
		FCIGame& G = Ui.G;
		const CI::FLevelDef* LP = G.CurrentLevel();
		const CI::FWorldDef* WP = G.CurrentWorld();
		if (!LP || !WP) { return; }
		const CI::FLevelDef& L = *LP;

		DrawTopBar(Ui, FString::Printf(TEXT("%d-%d  %s"), WP->Number, LevelNumber(L.Id), *ToF(L.Name)), ToF(L.Goal));
		{
			const int32* Best = G.Save ? G.Save->Stars.Find(ToF(L.Id)) : nullptr;
			const double SX = Ui.W - Ui.SR - Ui.U * 2.5 - Ui.U * 30 - Ui.U * 10;
			Stars(D, SX, Ui.ST + Ui.U * 6.5, Ui.U * 1.7, Best ? *Best : 0, true);
		}

		if (G.IsStudent() && G.Phase != ECIPhase::Build) { DrawReadout(Ui); }

		// Tray.
		const double TY = Ui.H - G.TrayH;
		const FBox2D Tray(FVector2D(Ui.SL + Ui.U * 1.5, TY + Ui.U * 1.5), FVector2D(Ui.W - Ui.SR - Ui.U * 1.5, Ui.H - FMath::Max(Ui.SB, Ui.U * 1.5)));
		G.TrayBox = Tray;
		Card(Ui, Tray, Cream, Ui.U * 3);
		const double BH = Tray.GetSize().Y - Ui.U * 4;
		const double CY = Tray.GetCenter().Y;
		const double PlayW = Ui.U * 30;
		const FBox2D Right(FVector2D(Tray.Max.X - Ui.U * 2 - PlayW, CY - BH * 0.5), FVector2D(Tray.Max.X - Ui.U * 2, CY + BH * 0.5));

		if (G.Phase == ECIPhase::Build)
		{
			double X = Tray.Min.X + Ui.U * 2;
			for (int32 I = 0; I < (int32)L.Tray.size(); ++I)
			{
				const CI::FPartDef* Def = G.Island.Catalog.Find(L.Tray[I].Part);
				const int32 Left = G.TrayRemaining(I);
				const FBox2D CB(FVector2D(X, CY - BH * 0.5), FVector2D(X + BH * 1.5, CY + BH * 0.5));
				FCITrayCard Tc;
				Tc.Box = CB;
				Tc.Tray = I;
				G.TrayCards.Add(Tc);
				const bool bHot = Left > 0 && Hot(Ui, CB);
				D.RoundRect(CB.Min.X, CB.Min.Y, CB.Max.X, CB.Max.Y, Ui.U * 2, bHot ? Col(0xfff0c9) : Col(0xf1ece0));
				DrawTrayIcon(D, BehaviorName(Def), CB.GetCenter().X, CB.Min.Y + BH * 0.38, BH * 0.5, Left > 0);
				D.Text(Def ? ToF(Def->Name) : FString(), CB.GetCenter().X, CB.Max.Y - BH * 0.18, Ui.U * 2.3, Left > 0 ? Col(Ink) : Col(Muted), 0.5, 0.5, true);
				D.Circle(CB.Max.X - Ui.U * 1.4, CB.Min.Y + Ui.U * 1.4, Ui.U * 2.1, Left > 0 ? Col(Coral) : Col(Muted), 20);
				D.Text(FString::Printf(TEXT("%d"), Left), CB.Max.X - Ui.U * 1.4, CB.Min.Y + Ui.U * 1.4, Ui.U * 2.2, FLinearColor::White, 0.5, 0.5, true);
				X += BH * 1.5 + Ui.U * 2;
			}
			const FBox2D Strip(FVector2D(X + Ui.U * 2, CY - BH * 0.5), FVector2D(Right.Min.X - Ui.U * 4, CY + BH * 0.5));
			if (G.Selected >= 0 && G.Selected < (int32)G.Setup.Placed.size())
			{
				DrawSliderStrip(Ui, Strip, G.Selected, -1, L.Tray[G.Setup.Placed[G.Selected].Tray].Params);
			}
			else if (G.SelectedFixed >= 0 && G.SelectedFixed < (int32)L.Fixed.size() && L.Fixed[G.SelectedFixed].bTunable)
			{
				DrawSliderStrip(Ui, Strip, -1, G.SelectedFixed, L.Fixed[G.SelectedFixed].Params);
			}
			else if (Strip.GetSize().X > Ui.U * 10)
			{
				bool bLauncher = false;
				for (const CI::FPlacedPart& Pp : G.Setup.Placed) { bLauncher = bLauncher || L.Tray[Pp.Tray].Part == "launcher"; }
				const FString Hint = G.Setup.Placed.empty() ? TEXT("Drag a part onto a glowing spot")
					: (bLauncher ? TEXT("Pull the ball back and let go to fire!") : TEXT("Drag the glowing grip on the machine, then press PLAY"));
				D.TextWrapped(Hint, Strip.Min.X + Ui.U * 1, CY - Ui.U * 1.6, Strip.GetSize().X, Ui.U * 2.6, 1.3, Col(InkSoft), 0, false);
			}
			Button(Ui, Right, TEXT("PLAY"), EIcon::Play, Green, ECIAction::Run, 0, true, Ui.U * 4.2);
		}
		else if (G.Phase == ECIPhase::Running)
		{
			const double BW = Ui.U * 20;
			double X = Tray.GetCenter().X - (BW * 3 + Ui.U * 4) * 0.5;
			Button(Ui, FBox2D(FVector2D(X, CY - BH * 0.5), FVector2D(X + BW, CY + BH * 0.5)), G.bPaused ? TEXT("GO") : TEXT("PAUSE"), G.bPaused ? EIcon::Play : EIcon::Pause, Blue, ECIAction::Pause, 0, true, Ui.U * 3);
			X += BW + Ui.U * 2;
			Button(Ui, FBox2D(FVector2D(X, CY - BH * 0.5), FVector2D(X + BW, CY + BH * 0.5)), G.bSlow ? TEXT("0.25x") : TEXT("SLOW"), EIcon::Slow, G.bSlow ? Yellow : 0x8d93a8, ECIAction::SlowMo, 0, true, Ui.U * 3);
			X += BW + Ui.U * 2;
			Button(Ui, FBox2D(FVector2D(X, CY - BH * 0.5), FVector2D(X + BW, CY + BH * 0.5)), TEXT("RETRY"), EIcon::Retry, Coral, ECIAction::Retry, 0, true, Ui.U * 3);
		}
		else
		{
			if (G.bSuccess)
			{
				int32 NW, NL;
				const bool bNext = G.FindNextLevel(NW, NL);
				Button(Ui, Right, bNext ? TEXT("NEXT") : TEXT("LEVELS"), EIcon::Next, Green, ECIAction::NextLevel, 0, true, Ui.U * 4.2);
				Button(Ui, FBox2D(FVector2D(Right.Min.X - Ui.U * 24, Right.Min.Y), FVector2D(Right.Min.X - Ui.U * 3, Right.Max.Y)), TEXT("RETRY"), EIcon::Retry, Blue, ECIAction::Retry, 0, true, Ui.U * 3.2);
				D.Text(FString::Printf(TEXT("Solved in %.2f s"), G.Sim.Time), Tray.Min.X + Ui.U * 4, CY, Ui.U * 2.8, Col(Ink), 0, 0.5, true);
			}
			else
			{
				const double Pulse = 1.0 + 0.04 * FMath::Sin(G.RealTime * 6.0);
				const FVector2D C = Right.GetCenter();
				const FVector2D Half = Right.GetExtent() * Pulse;
				Button(Ui, FBox2D(C - Half, C + Half), TEXT("RETRY"), EIcon::Retry, Coral, ECIAction::Retry, 0, true, Ui.U * 4.2);
				D.Text(TEXT("Your parts stay where you put them."), Tray.Min.X + Ui.U * 4, CY, Ui.U * 2.6, Col(InkSoft), 0, 0.5, false);
			}
		}

		if (G.Phase == ECIPhase::Result && G.bSuccess) { DrawResultCard(Ui); }
		if (G.Phase == ECIPhase::Result && !G.bSuccess) { DrawFailBubble(Ui); }
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
	G.TrayH = G.Screen == ECIScreen::Playing ? G.Safe.W + U * 19 : 0;
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
