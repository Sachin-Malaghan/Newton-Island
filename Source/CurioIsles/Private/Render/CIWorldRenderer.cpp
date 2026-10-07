// CURIO ISLES: draws the world - backdrop, level surfaces, parts, bodies, goals, overlays, effects. (CLAUDE.md: Rendering)
// Art direction: illustrated storybook - soft rounded shapes, warm gradients, layered parallax haze.
#include "Render/CIWorldRenderer.h"

#include "Core/CIParts.h"
#include "Game/CIGame.h"
#include "Render/CIDraw.h"

namespace
{
	using CI::FVec2;

	FVector2D V(const FVec2& P) { return FVector2D(P.X, P.Y); }
	FLinearColor Col(uint32 RGB, float A = 1.f) { return CIColor(RGB, A); }
	FLinearColor Lighten(const FLinearColor& C, float T) { return CIMix(C, FLinearColor(1, 1, 1, C.A), T); }
	FLinearColor Darken(const FLinearColor& C, float T) { return CIMix(C, FLinearColor(0, 0, 0, C.A), T); }
	// Display-only hash for decoration placement (never used by the simulation).
	double Hash(double X) { const double S = FMath::Sin(X * 127.1 + 311.7) * 43758.5453; return S - FMath::FloorToDouble(S); }

	const CI::FWorldTheme& ThemeOf(const FCIGame& G)
	{
		static const CI::FWorldTheme Default;
		if (const CI::FWorldDef* W = G.CurrentWorld()) { return W->Theme; }
		if (G.bLoaded && !G.Island.Worlds.empty()) { return G.Island.Worlds[0].Theme; }
		return Default;
	}

	// ------------------------------------------------------------ backdrop (screen space)

	void DrawSky(FCIDraw& D, const CI::FWorldTheme& T, double Time, double Horizon)
	{
		const double W = D.ScreenW, H = D.ScreenH;
		D.SetScreenSpace();
		D.RectV(0, 0, W, Horizon, Col(T.SkyTop), Col(T.SkyBottom));
		D.Rect(0, Horizon - 1, W, H, Col(T.SkyBottom));
		// Sun with a wide soft halo.
		D.Glow(W * 0.82, H * 0.2, H * 0.55, Col(T.Sun, 0.55f), Col(T.Sun, 0.f));
		D.Circle(W * 0.82, H * 0.2, H * 0.055, Col(T.Sun), 40);
		// Clouds drift slowly; each is a few overlapping ellipses.
		for (int I = 0; I < 6; ++I)
		{
			const double Speed = H * (0.004 + 0.003 * Hash(I + 3.0));
			const double Span = W * 1.5;
			double X = FMath::Fmod(Hash(I + 0.5) * Span + Time * Speed, Span) - W * 0.25;
			const double Y = H * (0.08 + 0.26 * Hash(I + 9.0));
			const double S = H * (0.035 + 0.03 * Hash(I + 17.0));
			const FLinearColor C = FLinearColor(1, 1, 1, 0.75f);
			D.Ellipse(X, Y, S * 2.2, S * 0.8, C, 24);
			D.Ellipse(X - S * 0.9, Y + S * 0.15, S * 1.2, S * 0.7, C, 20);
			D.Ellipse(X + S * 0.3, Y - S * 0.45, S * 1.2, S * 0.85, C, 20);
			D.Ellipse(X + S * 1.3, Y + S * 0.1, S, S * 0.6, C, 20);
		}
	}

	// Three layers of rolling hills with haze toward the horizon.
	void DrawHills(FCIDraw& D, const CI::FWorldTheme& T, double Base, double Amp)
	{
		const double W = D.ScreenW, H = D.ScreenH;
		D.SetScreenSpace();
		for (int K = 0; K < 3; ++K)
		{
			const double Y0 = Base - Amp * (1.0 - K * 0.3);
			const double A = Amp * (0.55 - K * 0.1);
			TArray<FVector2D> P;
			const int N = 72;
			for (int I = 0; I <= N; ++I)
			{
				const double X = W * I / N;
				const double U = X / H;
				const double Y = Y0 + A * (0.5 + 0.5 * FMath::Sin(U * (2.1 + K * 0.9) + K * 1.7)) * (0.7 + 0.3 * FMath::Sin(U * 5.3 + K * 2.3));
				P.Add(FVector2D(X, Y));
			}
			P.Add(FVector2D(W, H));
			P.Add(FVector2D(0, H));
			D.Polygon(P, Col(T.Hills[K]));
			// Atmospheric haze fades far layers into the sky.
			const float Haze = 0.45f - K * 0.15f;
			D.RectV(0, Y0 - A * 0.2, W, Base + Amp * 0.4, Col(T.SkyBottom, 0.f), Col(T.SkyBottom, Haze));
		}
	}

	// ------------------------------------------------------------ title and island map backdrop

	void DrawIsland(FCIDraw& D, double X, double Y, double S, bool bLocked, double Time, int Kind)
	{
		const FLinearColor Sand = bLocked ? Col(0xb9c3d6, 0.6f) : Col(0xf6dca6);
		const FLinearColor Green = bLocked ? Col(0x9aa7c2, 0.65f) : Col(0x6cc36b);
		const FLinearColor Dark = bLocked ? Col(0x8793b0, 0.65f) : Col(0x4f9e56);
		D.Ellipse(X, Y + S * 0.05, S * 1.25, S * 0.2, bLocked ? Col(0xffffff, 0.15f) : Col(0xffffff, 0.35f), 40);   // surf
		D.Ellipse(X, Y, S * 1.1, S * 0.16, Sand, 40);
		TArray<FVector2D> Hill;
		for (int I = 0; I <= 24; ++I)
		{
			const double T = (double)I / 24;
			const double HX = X + (T - 0.5) * S * 1.9;
			const double HY = Y - S * 0.05 - FMath::Sin(T * PI) * S * (0.55 + 0.18 * FMath::Sin(T * 9 + Kind));
			Hill.Add(FVector2D(HX, HY));
		}
		Hill.Add(FVector2D(X + S * 0.95, Y));
		Hill.Add(FVector2D(X - S * 0.95, Y));
		D.Polygon(Hill, Green);
		D.Ellipse(X + S * 0.25, Y - S * 0.3, S * 0.35, S * 0.18, Dark, 24);
		if (bLocked)
		{
			// Mist and a "coming soon" flag.
			D.Glow(X, Y - S * 0.3, S * 1.6, Col(0xffffff, 0.35f), Col(0xffffff, 0.f));
			D.Line(X, Y - S * 0.62, X, Y - S * 1.05, S * 0.03, Col(0x6b7390, 0.7f));
			D.Tri(X, Y - S * 1.05, X + S * 0.3, Y - S * 0.95, X, Y - S * 0.85, Col(0xfff4d6, 0.8f));
			return;
		}
		// Newton's Island: a windmill turning and a lighthouse.
		const double MX = X - S * 0.35, MY = Y - S * 0.62;
		D.Quad(FVector2D(MX - S * 0.07, MY + S * 0.35), FVector2D(MX + S * 0.07, MY + S * 0.35), FVector2D(MX + S * 0.045, MY), FVector2D(MX - S * 0.045, MY), Col(0xfff4e0));
		for (int B = 0; B < 4; ++B)
		{
			const double A = Time * 1.3 + B * PI * 0.5;
			D.Line(MX, MY, MX + FMath::Cos(A) * S * 0.28, MY + FMath::Sin(A) * S * 0.28, S * 0.05, Col(0xe8604c));
		}
		D.Circle(MX, MY, S * 0.035, Col(0x7a4b2a), 12);
		const double LX = X + S * 0.55, LY = Y - S * 0.28;
		D.Quad(FVector2D(LX - S * 0.08, LY), FVector2D(LX + S * 0.08, LY), FVector2D(LX + S * 0.05, LY - S * 0.55), FVector2D(LX - S * 0.05, LY - S * 0.55), Col(0xffffff));
		D.Rect(LX - S * 0.065, LY - S * 0.3, LX + S * 0.065, LY - S * 0.22, Col(0xe8604c));
		D.Glow(LX, LY - S * 0.6, S * 0.35, Col(0xfff3b0, 0.35f + 0.25f * (float)FMath::Abs(FMath::Sin(Time * 1.5))), Col(0xfff3b0, 0.f));
		D.Circle(LX, LY - S * 0.6, S * 0.06, Col(0xffd35a), 16);
	}

	void DrawArchipelago(FCIDraw& D, const FCIGame& G, float Dim)
	{
		CI::FWorldTheme T;
		T.SkyTop = 0x7cc8f2;
		T.SkyBottom = 0xffe7c4;
		T.Sun = 0xfff1c1;
		const double W = D.ScreenW, H = D.ScreenH;
		const double Horizon = H * 0.56;
		DrawSky(D, T, G.RealTime, Horizon);
		D.RectV(0, Horizon, W, H, Col(0x4fb8dc), Col(0x1d6f9f));
		// Sun glitter on the water.
		for (int I = 0; I < 40; ++I)
		{
			const double X = W * (0.6 + 0.35 * Hash(I * 1.3)) + FMath::Sin(G.RealTime * 0.7 + I) * H * 0.01;
			const double Y = Horizon + (H - Horizon) * Hash(I * 2.7) * 0.9;
			const double L = H * (0.01 + 0.03 * Hash(I * 5.1));
			const float A = 0.25f + 0.35f * (float)(0.5 + 0.5 * FMath::Sin(G.RealTime * 2.1 + I * 1.7));
			D.Line(X - L, Y, X + L, Y, H * 0.004, FLinearColor(1, 1, 1, A));
		}
		// Islands: 1 is here, the rest are misty silhouettes waiting for their expansions.
		const double Bob = FMath::Sin(G.RealTime * 0.8) * H * 0.004;
		DrawIsland(D, W * 0.14, Horizon + H * 0.03, H * 0.09, true, G.RealTime, 1);
		DrawIsland(D, W * 0.88, Horizon + H * 0.05, H * 0.1, true, G.RealTime, 2);
		DrawIsland(D, W * 0.66, Horizon + H * 0.02, H * 0.07, true, G.RealTime, 3);
		DrawIsland(D, W * 0.36, Horizon + H * 0.3 + Bob, H * 0.2, false, G.RealTime, 0);
		DrawIsland(D, W * 0.78, Horizon + H * 0.33, H * 0.09, true, G.RealTime, 4);
		if (Dim > 0) { D.Rect(0, 0, W, H, FLinearColor(0.08f, 0.1f, 0.2f, Dim)); }
	}

	// ------------------------------------------------------------ level (world space, metres, y up)

	struct FScene
	{
		FCIDraw& D;
		FCIGame& G;
		const CI::FLevelDef& L;
		const CI::FWorldTheme& T;
		double Time;
		double Px;       // pixels per metre
		double Bottom;   // world y below the view
	};

	void DrawSurface(FScene& S, const CI::FSurfaceDef& Surf)
	{
		FCIDraw& D = S.D;
		const auto& Pts = Surf.Points;
		if (Surf.Look == "basket") { return; }   // drawn with its zone
		const bool bTrack = Surf.Look == "track";
		if (Surf.Look == "ground" || bTrack)
		{
			TArray<FVector2D> Poly;
			for (const FVec2& P : Pts) { Poly.Add(V(P)); }
			Poly.Add(FVector2D(Pts.back().X, S.Bottom));
			Poly.Add(FVector2D(Pts.front().X, S.Bottom));
			const FLinearColor Deep = bTrack ? Col(0xb58657) : Col(S.T.GroundDark);
			const FLinearColor Top = bTrack ? Col(0xd6a86f) : Col(S.T.Ground);
			D.Polygon(Poly, Deep);
			// A band of the lighter colour along the surface, then a bright lip.
			for (size_t I = 0; I + 1 < Pts.size(); ++I)
			{
				const FVec2 A = Pts[I], B = Pts[I + 1];
				const FVec2 N = (B - A).Normalized().Perp();
				const double Band = 0.45;
				D.Quad(V(A), V(B), V(B - N * Band), V(A - N * Band), Top);
				D.Circle(B.X, B.Y, 0.001, Top, 3);
			}
			for (size_t I = 0; I + 1 < Pts.size(); ++I)
			{
				const FVec2 A = Pts[I], B = Pts[I + 1];
				D.RoundLine(A.X, A.Y, B.X, B.Y, bTrack ? 0.06 : 0.1, Lighten(Top, 0.28f));
			}
			if (bTrack)
			{
				// Sleepers and a rail.
				for (size_t I = 0; I + 1 < Pts.size(); ++I)
				{
					const FVec2 A = Pts[I], B = Pts[I + 1];
					const double Len = (B - A).Length();
					const FVec2 Dir = (B - A) / Len, N = Dir.Perp();
					for (double T = 0.2; T < Len; T += 0.55)
					{
						const FVec2 C = A + Dir * T;
						D.Quad(V(C - Dir * 0.09), V(C + Dir * 0.09), V(C + Dir * 0.09 + N * 0.06), V(C - Dir * 0.09 + N * 0.06), Col(0x7a5236));
					}
					D.Line(A.X + N.X * 0.075, A.Y + N.Y * 0.075, B.X + N.X * 0.075, B.Y + N.Y * 0.075, 0.035, Col(0x5b6070));
				}
				return;
			}
			// Grass tufts on the flatter stretches.
			for (size_t I = 0; I + 1 < Pts.size(); ++I)
			{
				const FVec2 A = Pts[I], B = Pts[I + 1];
				const double Len = (B - A).Length();
				const FVec2 Dir = (B - A) / Len, N = Dir.Perp();
				if (N.Y < 0.75) { continue; }
				for (double T = 0.15; T < Len; T += 0.32)
				{
					const double H = Hash(A.X * 3.1 + T * 7.7);
					if (H < 0.35) { continue; }
					const FVec2 C = A + Dir * T;
					const double Tall = 0.08 + 0.1 * H;
					const FLinearColor Blade = Lighten(Top, 0.12f + 0.2f * (float)H);
					D.Tri(C.X - 0.04, C.Y, C.X + 0.02, C.Y, C.X - 0.03 + 0.04 * FMath::Sin(S.Time * 1.6 + C.X), C.Y + Tall, Blade);
					D.Tri(C.X, C.Y, C.X + 0.06, C.Y, C.X + 0.05 + 0.03 * FMath::Sin(S.Time * 1.4 + C.X * 1.3), C.Y + Tall * 0.8, Blade);
					if (H > 0.93) { D.Circle(C.X + 0.02, C.Y + Tall + 0.02, 0.035, Col(S.T.Accent), 10); }
				}
			}
			return;
		}
		if (Surf.Look == "wall")
		{
			// Stone: a filled shape with courses of blocks.
			TArray<FVector2D> Poly;
			double MinY = 1e9, MaxY = -1e9, MinX = 1e9, MaxX = -1e9;
			for (size_t I = 0; I < Pts.size(); ++I)
			{
				if (I + 1 == Pts.size() && Pts[I] == Pts[0]) { break; }
				Poly.Add(V(Pts[I]));
			}
			for (const FVec2& P : Pts) { MinY = FMath::Min(MinY, P.Y); MaxY = FMath::Max(MaxY, P.Y); MinX = FMath::Min(MinX, P.X); MaxX = FMath::Max(MaxX, P.X); }
			D.Polygon(Poly, Col(0x9a93a6));
			int Row = 0;
			for (double Y = MinY + 0.4; Y < MaxY - 0.05; Y += 0.4, ++Row)
			{
				D.Line(MinX, Y, MaxX, Y, 0.03, Col(0x7d7689));
				for (double X = MinX + (Row % 2 ? 0.3 : 0.6); X < MaxX - 0.05; X += 0.6) { D.Line(X, Y - 0.4, X, Y, 0.03, Col(0x7d7689)); }
			}
			D.Rect(MinX, MaxY - 0.08, MaxX, MaxY, Col(0xb9b3c4));
			D.Polyline(Poly, 0.05, Col(0x6d6679), true);
			return;
		}
		if (Surf.Look == "block")
		{
			TArray<FVector2D> Poly;
			for (const FVec2& P : Pts) { Poly.Add(V(P)); }
			double MinY = 1e9, MaxY = -1e9, MinX = 1e9, MaxX = -1e9;
			for (const FVec2& P : Pts) { MinY = FMath::Min(MinY, P.Y); MaxY = FMath::Max(MaxY, P.Y); MinX = FMath::Min(MinX, P.X); MaxX = FMath::Max(MaxX, P.X); }
			// A sturdy wooden workbench: legs, apron, thick top.
			const FLinearColor Wood = Col(0xd9a066), WoodDark = Col(0xa8703f), WoodLight = Col(0xefc48c);
			D.Rect(MinX + 0.15, MinY, MinX + 0.4, MaxY - 0.2, WoodDark);
			D.Rect(MaxX - 0.4, MinY, MaxX - 0.15, MaxY - 0.2, WoodDark);
			D.Rect((MinX + MaxX) * 0.5 - 0.12, MinY, (MinX + MaxX) * 0.5 + 0.12, MaxY - 0.2, Darken(WoodDark, 0.1f));
			D.Rect(MinX + 0.1, MinY + 0.12, MaxX - 0.1, MinY + 0.22, Darken(WoodDark, 0.15f));
			D.RoundRectV(MinX, MaxY - 0.32, MaxX, MaxY, 0.06, WoodDark, Wood);
			D.RoundRect(MinX + 0.02, MaxY - 0.07, MaxX - 0.02, MaxY, 0.03, WoodLight);
			for (double X = MinX + 0.7; X < MaxX - 0.3; X += 1.3) { D.Line(X, MaxY - 0.3, X + 0.4, MaxY - 0.3, 0.02, Darken(Wood, 0.15f)); }
			return;
		}
		for (size_t I = 0; I + 1 < Pts.size(); ++I) { D.RoundLine(Pts[I].X, Pts[I].Y, Pts[I + 1].X, Pts[I + 1].Y, 0.06, Col(0x5b4a3a)); }
	}

	void DrawWater(FScene& S, const CI::FZoneDef& Z)
	{
		FCIDraw& D = S.D;
		const double Top = Z.Level;
		D.RectV(Z.Min.X, Top, Z.Max.X, Z.Min.Y - 1.0, Col(0x6fd0f2, 0.8f), Col(0x2a78b8, 0.92f));
		TArray<FVector2D> Wave;
		for (double X = Z.Min.X; X <= Z.Max.X + 1e-6; X += 0.1)
		{
			Wave.Add(FVector2D(X, Top + 0.035 * FMath::Sin(X * 5.0 + S.Time * 2.4)));
		}
		D.Polyline(Wave, 0.05, Col(0xd9f6ff, 0.9f));
		for (int I = 0; I < 10; ++I)
		{
			const double X = Z.Min.X + (Z.Max.X - Z.Min.X) * FMath::Fmod(Hash(I * 3.3) + S.Time * 0.03 * (1 + Hash(I)), 1.0);
			const double Y = Top - 0.12 - 0.5 * Hash(I * 7.1);
			if (Y < Z.Min.Y) { continue; }
			D.Line(X - 0.12, Y, X + 0.12, Y, 0.025, FLinearColor(1, 1, 1, 0.35f));
		}
	}

	void DrawSlot(FScene& S, int32 Index, bool bHighlight, bool bHover)
	{
		FCIDraw& D = S.D;
		const FVec2 P = S.L.Slots[Index].Pos + FVec2(0, 0.35);
		const double Pulse = 0.5 + 0.5 * FMath::Sin(S.Time * 3.2 + Index);
		const double R = 0.32 + 0.04 * Pulse + (bHover ? 0.1 : 0);
		const FLinearColor C = bHighlight ? Col(0x5cf08a, 0.95f) : FLinearColor(1, 1, 1, 0.75f);
		if (bHighlight) { D.Glow(P.X, P.Y, R * 2.4, Col(0x5cf08a, bHover ? 0.6f : 0.35f), Col(0x5cf08a, 0.f)); }
		const int Dashes = 12;
		for (int I = 0; I < Dashes; ++I)
		{
			const double A0 = 2 * PI * I / Dashes + S.Time * 0.6;
			D.Arc(P.X, P.Y, R - 0.03, R + 0.03, A0, A0 + PI / Dashes, C, 4);
		}
		D.Line(P.X - 0.12, P.Y, P.X + 0.12, P.Y, 0.05, C);
		D.Line(P.X, P.Y - 0.12, P.X, P.Y + 0.12, 0.05, C);
	}

	FLinearColor Tinted(const FLinearColor& C, const FLinearColor& Tint, float Amount, float Alpha)
	{
		FLinearColor R = CIMix(C, Tint, Amount);
		R.A = C.A * Alpha;
		return R;
	}

	void DrawPart(FScene& S, const CI::FPartDef& Part, const CI::FPartGeometry& G, const TArray<double>& Values, bool bSelected, const FLinearColor& Tint, float TintAmount, float Alpha)
	{
		FCIDraw& D = S.D;
		auto C = [&](uint32 RGB) { return Tinted(Col(RGB), Tint, TintAmount, Alpha); };
		switch (Part.Behavior)
		{
		case CI::EPartBehavior::Ramp:
		{
			if (G.Chains.empty() || G.Chains[0].Points.size() < 3) { return; }
			const FVec2 Lip = G.Chains[0].Points[0], Top = G.Chains[0].Points[1], Foot = G.Chains[0].Points[2];
			if (bSelected) { D.Glow((Top.X + Foot.X) * 0.5, (Top.Y + Foot.Y) * 0.5, 2.0, Col(0xffe066, 0.55f * Alpha), Col(0xffe066, 0.f)); }
			TArray<FVector2D> Side = { V(Lip), V(Top), V(Foot), FVector2D(Lip.X, Foot.Y) };
			D.Polygon(Side, C(0xf0c98f));
			// Cross bracing and posts.
			const double Dx = Foot.X - Lip.X;
			for (int I = 1; I <= 3; ++I)
			{
				const double X = Lip.X + Dx * I / 4.0;
				const double TopY = FMath::Lerp(Top.Y, Foot.Y, FMath::Clamp((X - Top.X) / (Foot.X - Top.X), 0.0, 1.0));
				D.Line(X, Foot.Y, X, TopY, 0.05, C(0xc48a52));
			}
			D.Line(Lip.X, Foot.Y + 0.05, Lip.X + Dx * 0.75, Foot.Y + (Top.Y - Foot.Y) * 0.8, 0.04, C(0xc48a52));
			D.Line(Lip.X, Top.Y, Lip.X, Foot.Y, 0.08, C(0xa8703f));
			D.Line(Lip.X, Top.Y + 0.02, Top.X, Top.Y + 0.02, 0.1, C(0x8a5a2b));
			D.RoundLine(Top.X, Top.Y + 0.02, Foot.X, Foot.Y + 0.02, 0.1, C(0x8a5a2b));
			D.RoundLine(Lip.X, Top.Y + 0.05, Top.X, Top.Y + 0.05, 0.03, C(0xd9a066));
			D.Line(Top.X, Top.Y + 0.05, Foot.X, Foot.Y + 0.05, 0.03, C(0xd9a066));
			// Little start flag.
			D.Line(Lip.X + 0.05, Top.Y, Lip.X + 0.05, Top.Y + 0.55, 0.03, C(0x5b4a3a));
			D.Tri(Lip.X + 0.05, Top.Y + 0.55, Lip.X + 0.05 + 0.3 * G.Facing, Top.Y + 0.47, Lip.X + 0.05, Top.Y + 0.39, C(0xff6b5a));
			break;
		}
		case CI::EPartBehavior::Launcher:
		{
			const FVec2 Pv = G.Pivot;
			if (bSelected) { D.Glow(Pv.X, Pv.Y + 0.1, 1.6, Col(0xffe066, 0.55f * Alpha), Col(0xffe066, 0.f)); }
			const FVec2 Dir(FMath::Cos(G.Angle), FMath::Sin(G.Angle));
			const FVec2 Back = Pv - Dir * 0.28, Front = Pv + Dir * 0.78;
			// Barrel with brass bands.
			D.RoundLine(Back.X, Back.Y, Front.X, Front.Y, 0.3, C(0x3d4260));
			D.RoundLine(Back.X, Back.Y, Front.X, Front.Y, 0.18, C(0x575d85));
			const FVec2 N = Dir.Perp();
			for (double T : { 0.1, 0.62 })
			{
				const FVec2 B = Pv + Dir * T;
				D.Line(B.X - N.X * 0.17, B.Y - N.Y * 0.17, B.X + N.X * 0.17, B.Y + N.Y * 0.17, 0.07, C(0xf2c14e));
			}
			D.Circle(Front.X, Front.Y, 0.13, C(0xf2c14e), 16);
			D.Circle(Front.X, Front.Y, 0.08, C(0x1f2233), 16);
			// Carriage and wheels.
			const FVec2 Base = G.Base;
			D.Quad(FVector2D(Base.X - 0.36, Base.Y + 0.14), FVector2D(Base.X + 0.36, Base.Y + 0.14), FVector2D(Pv.X + 0.12, Pv.Y), FVector2D(Pv.X - 0.12, Pv.Y), C(0xa8703f));
			for (double Wx : { -0.24, 0.24 })
			{
				D.Circle(Base.X + Wx, Base.Y + 0.17, 0.17, C(0x6b4a2e), 20);
				D.Circle(Base.X + Wx, Base.Y + 0.17, 0.11, C(0xc48a52), 16);
				D.Circle(Base.X + Wx, Base.Y + 0.17, 0.04, C(0x6b4a2e), 8);
			}
			D.Circle(Pv.X, Pv.Y, 0.08, C(0xf2c14e), 12);
			break;
		}
		case CI::EPartBehavior::Bouncer:
		{
			if (G.Chains.empty()) { return; }
			const FVec2 A = G.Chains[0].Points[0], B2 = G.Chains[0].Points[1];
			const FVec2 Dir = (B2 - A).Normalized();
			FVec2 N = Dir.Perp();
			if (N.Y < 0) { N = -N; }
			if (bSelected) { D.Glow(G.Pivot.X, G.Pivot.Y, 1.6, Col(0xffe066, 0.55f * Alpha), Col(0xffe066, 0.f)); }
			// Post and springs, then the pad.
			D.RoundRect(G.Base.X - 0.35, G.Base.Y - 0.02, G.Base.X + 0.35, G.Base.Y + 0.1, 0.04, C(0x5b6070));
			D.Line(G.Base.X, G.Base.Y + 0.05, G.Pivot.X - N.X * 0.12, G.Pivot.Y - N.Y * 0.12, 0.09, C(0x8d93a8));
			for (double T : { 0.2, 0.8 })
			{
				const FVec2 P0 = A + (B2 - A) * T - N * 0.06;
				D.Line(P0.X, P0.Y, G.Base.X + (T - 0.5) * 0.4, G.Base.Y + 0.08, 0.035, C(0xb8bfd1));
			}
			D.RoundLine(A.X - N.X * 0.07, A.Y - N.Y * 0.07, B2.X - N.X * 0.07, B2.Y - N.Y * 0.07, 0.16, C(0x3d4260));
			D.RoundLine(A.X, A.Y, B2.X, B2.Y, 0.09, C(0x4fd382));
			D.RoundLine(A.X + Dir.X * 0.15, A.Y + Dir.Y * 0.15, B2.X - Dir.X * 0.15, B2.Y - Dir.Y * 0.15, 0.03, C(0xb6f5cf));
			break;
		}
		case CI::EPartBehavior::Brake:
		{
			for (const CI::FBrakeRegion& R : G.Brakes)
			{
				const double Y = G.Base.Y;
				if (bSelected) { D.Glow((R.Min.X + R.Max.X) * 0.5, Y, (R.Max.X - R.Min.X) * 0.7, Col(0xffe066, 0.5f * Alpha), Col(0xffe066, 0.f)); }
				D.RoundRect(R.Min.X, Y - 0.02, R.Max.X, Y + 0.09, 0.04, C(0x33313b));
				const double Strength = Values.Num() > 0 ? Values[0] : 2.0;
				const double Gap = FMath::Lerp(0.42, 0.16, FMath::Clamp((Strength - 0.5) / 5.5, 0.0, 1.0));
				for (double X = R.Min.X + 0.08; X < R.Max.X - 0.12; X += Gap)
				{
					D.Quad(FVector2D(X, Y), FVector2D(X + 0.08, Y), FVector2D(X + 0.14, Y + 0.08), FVector2D(X + 0.06, Y + 0.08), C(0xffc93c));
				}
				D.RoundRect(R.Min.X - 0.05, Y - 0.03, R.Min.X + 0.06, Y + 0.14, 0.03, C(0xff6b5a));
				D.RoundRect(R.Max.X - 0.06, Y - 0.03, R.Max.X + 0.05, Y + 0.14, 0.03, C(0xff6b5a));
			}
			break;
		}
		}
	}

	void DrawBall(FScene& S, const FVector2D& P, double R, double Roll)
	{
		FCIDraw& D = S.D;
		D.Circle(P.X, P.Y, R, Col(0xd8413c), 28);
		D.Circle(P.X, P.Y, R * 0.86, Col(0xff5d57), 28);
		const FVector2D Dir(FMath::Cos(Roll), FMath::Sin(Roll));
		D.Line(P.X - Dir.X * R * 0.84, P.Y - Dir.Y * R * 0.84, P.X + Dir.X * R * 0.84, P.Y + Dir.Y * R * 0.84, R * 0.28, Col(0xfff1e6));
		D.Circle(P.X - R * 0.35, P.Y + R * 0.38, R * 0.22, FLinearColor(1, 1, 1, 0.55f), 12);
	}

	void DrawCart(FScene& S, const FVector2D& P, double R, double Roll)
	{
		FCIDraw& D = S.D;
		const double WheelY = P.Y - R + 0.16;
		const double BodyY0 = P.Y - R + 0.2, BodyY1 = BodyY0 + 0.55;
		D.RoundRectV(P.X - 0.6, BodyY0, P.X + 0.6, BodyY1, 0.12, Darken(Col(S.T.Accent), 0.15f), Col(S.T.Accent));
		D.RoundRect(P.X - 0.48, BodyY0 + 0.24, P.X + 0.2, BodyY1 - 0.08, 0.06, Col(0xfff4dc));
		D.RoundRect(P.X - 0.62, BodyY1 - 0.06, P.X + 0.62, BodyY1 + 0.04, 0.04, Darken(Col(S.T.Accent), 0.3f));
		// Bolt the robot rides along: a round screen-face.
		D.RoundRect(P.X - 0.02, BodyY1, P.X + 0.42, BodyY1 + 0.36, 0.1, Col(0xdde3f0));
		D.RoundRect(P.X + 0.04, BodyY1 + 0.06, P.X + 0.36, BodyY1 + 0.3, 0.07, Col(0x243056));
		D.Circle(P.X + 0.13, BodyY1 + 0.19, 0.035, Col(0x7ef0ff), 10);
		D.Circle(P.X + 0.27, BodyY1 + 0.19, 0.035, Col(0x7ef0ff), 10);
		D.Line(P.X + 0.2, BodyY1 + 0.36, P.X + 0.2, BodyY1 + 0.46, 0.025, Col(0x8a93ab));
		D.Circle(P.X + 0.2, BodyY1 + 0.48, 0.04, Col(0xff6b5a), 10);
		for (double Wx : { -0.36, 0.36 })
		{
			D.Circle(P.X + Wx, WheelY, 0.17, Col(0x2e2f3a), 20);
			D.Circle(P.X + Wx, WheelY, 0.1, Col(0xb8bfd1), 16);
			for (int K = 0; K < 3; ++K)
			{
				const double A = Roll + K * PI / 3;
				D.Line(P.X + Wx - FMath::Cos(A) * 0.1, WheelY - FMath::Sin(A) * 0.1, P.X + Wx + FMath::Cos(A) * 0.1, WheelY + FMath::Sin(A) * 0.1, 0.025, Col(0x5b6070));
			}
		}
	}

	void DrawBell(FScene& S, const CI::FZoneDef& Z, double Swing)
	{
		FCIDraw& D = S.D;
		const double CX = (Z.Min.X + Z.Max.X) * 0.5;
		const double Base = Z.Min.Y - 0.7, TopY = Z.Max.Y + 0.35, PostX = Z.Max.X + 0.45;
		D.Rect(PostX - 0.07, Base, PostX + 0.07, TopY + 0.05, Col(0x8a5a2b));
		D.Rect(CX - 0.1, TopY - 0.04, PostX + 0.07, TopY + 0.06, Col(0x8a5a2b));
		D.Line(PostX - 0.05, TopY - 0.45, PostX - 0.5, TopY - 0.02, 0.05, Col(0xa8703f));
		const FVector2D Hang(CX, TopY - 0.04);
		const double Len = TopY - (Z.Max.Y - 0.05);
		const FVector2D Down(FMath::Sin(Swing), -FMath::Cos(Swing));
		const FVector2D Across(-Down.Y, Down.X);
		const FVector2D Crown = Hang + Down * (Len * 0.25);
		D.Line(Hang.X, Hang.Y, Crown.X, Crown.Y, 0.03, Col(0x5b4a3a));
		// Bell body: a flared shape in the bell's own frame.
		TArray<FVector2D> Bell;
		const double H = (Z.Max.Y - Z.Min.Y) * 0.85, Wd = (Z.Max.X - Z.Min.X) * 0.5;
		const double Prof[][2] = { { 0.0, 0.22 }, { 0.12, 0.4 }, { 0.35, 0.52 }, { 0.62, 0.6 }, { 0.82, 0.78 }, { 1.0, 1.0 } };
		for (const auto& Pr : Prof) { Bell.Add(Crown + Down * (Pr[0] * H) + Across * (Pr[1] * Wd)); }
		for (int I = UE_ARRAY_COUNT(Prof) - 1; I >= 0; --I) { Bell.Add(Crown + Down * (Prof[I][0] * H) - Across * (Prof[I][1] * Wd)); }
		D.Polygon(Bell, Col(0xf2c14e));
		const FVector2D Lip0 = Crown + Down * H + Across * Wd, Lip1 = Crown + Down * H - Across * Wd;
		D.RoundLine(Lip0.X, Lip0.Y, Lip1.X, Lip1.Y, 0.07, Col(0xc8912e));
		const FVector2D Shine = Crown + Down * (H * 0.35) + Across * (Wd * 0.25);
		D.Ellipse(Shine.X, Shine.Y, Wd * 0.12, H * 0.22, FLinearColor(1, 1, 1, 0.45f), 12, Swing);
		const FVector2D Clap = Crown + Down * (H * 1.05) + Across * (FMath::Sin(Swing * 3) * Wd * 0.2);
		D.Circle(Clap.X, Clap.Y, 0.07, Col(0x8a5a2b), 12);
	}

	void DrawButton(FScene& S, const CI::FZoneDef& Z, bool bPressed)
	{
		FCIDraw& D = S.D;
		const double CX = (Z.Min.X + Z.Max.X) * 0.5, Y = Z.Min.Y;
		D.RoundRect(Z.Min.X - 0.08, Y - 0.02, Z.Max.X + 0.08, Y + 0.1, 0.04, Col(0x8d93a8));
		if (bPressed) { D.Glow(CX, Y + 0.1, 1.2, Col(0x5cf08a, 0.6f), Col(0x5cf08a, 0.f)); }
		D.Ellipse(CX, Y + 0.1, (Z.Max.X - Z.Min.X) * 0.36, bPressed ? 0.06 : 0.17, bPressed ? Col(0x3ccf6e) : Col(0xff4d4d), 28);
		D.Ellipse(CX - 0.08, Y + (bPressed ? 0.12 : 0.18), 0.1, 0.035, FLinearColor(1, 1, 1, 0.45f), 12);
		// A sign so the goal reads without words.
		D.Rect(Z.Max.X + 0.25, Y, Z.Max.X + 0.31, Y + 1.0, Col(0x8a5a2b));
		D.RoundRect(Z.Max.X + 0.02, Y + 0.72, Z.Max.X + 0.54, Y + 1.2, 0.08, Col(0xfff4dc));
		D.Circle(Z.Max.X + 0.28, Y + 0.96, 0.14, bPressed ? Col(0x3ccf6e) : Col(0xff4d4d), 16);
	}

	void DrawBasket(FScene& S, const CI::FZoneDef& Z, double Glow)
	{
		FCIDraw& D = S.D;
		const double X0 = Z.Min.X - 0.08, X1 = Z.Max.X + 0.08, Y0 = Z.Min.Y, Y1 = Z.Max.Y + 0.02;
		if (Glow > 0) { D.Glow((X0 + X1) * 0.5, Y1, 1.6, Col(0xffe066, (float)(0.6 * Glow)), Col(0xffe066, 0.f)); }
		TArray<FVector2D> Body = { FVector2D(X0, Y1), FVector2D(X1, Y1), FVector2D(X1 - 0.08, Y0), FVector2D(X0 + 0.08, Y0) };
		D.Polygon(Body, Col(0xc98646));
		for (double Y = Y0 + 0.06; Y < Y1 - 0.02; Y += 0.09)
		{
			const double T = (Y - Y0) / (Y1 - Y0);
			D.Line(X0 + 0.08 * (1 - T), Y, X1 - 0.08 * (1 - T), Y, 0.025, Col(0xa0663a));
		}
		for (double X = X0 + 0.12; X < X1 - 0.08; X += 0.14) { D.Line(X, Y0 + 0.02, X + 0.02, Y1 - 0.02, 0.02, Col(0xe0a468, 0.7f)); }
		D.RoundLine(X0 - 0.02, Y1, X1 + 0.02, Y1, 0.08, Col(0x8a5230));
	}

	void DrawGrid(FScene& S)
	{
		FCIDraw& D = S.D;
		const CI::FLevelDef& L = S.L;
		const CI::FVec2 A = S.G.ToWorld(S.G.WorldBox.Min), B = S.G.ToWorld(S.G.WorldBox.Max);
		const double X0 = FMath::FloorToDouble(FMath::Min(A.X, B.X)), X1 = FMath::CeilToDouble(FMath::Max(A.X, B.X));
		const double Y0 = FMath::FloorToDouble(FMath::Min(A.Y, B.Y)), Y1 = FMath::CeilToDouble(FMath::Max(A.Y, B.Y));
		const double Px1 = 1.0 / S.Px;
		const int LabelEvery = S.Px < 45 ? 2 : 1;
		for (double X = X0; X <= X1; X += 1.0)
		{
			const bool bMajor = FMath::Fmod(FMath::Abs(X), 5.0) < 0.5;
			D.Line(X, Y0, X, Y1, (bMajor ? 2.0 : 1.0) * Px1, FLinearColor(1, 1, 1, bMajor ? 0.32f : 0.16f));
		}
		for (double Y = Y0; Y <= Y1; Y += 1.0)
		{
			const bool bMajor = FMath::Fmod(FMath::Abs(Y), 5.0) < 0.5;
			D.Line(X0, Y, X1, Y, (bMajor ? 2.0 : 1.0) * Px1, FLinearColor(1, 1, 1, bMajor ? 0.32f : 0.16f));
		}
		// Axis labels along the bottom and left of the world view.
		const double Fs = FMath::Clamp(S.D.ScreenH * 0.018, 10.0, 22.0);
		const FLinearColor Lbl(1, 1, 1, 0.85f);
		for (double X = X0; X <= X1; X += 1.0)
		{
			if ((int)FMath::Abs(X) % LabelEvery != 0) { continue; }
			const FVector2D P = S.G.ToScreen(X, 0);
			if (P.X < S.G.WorldBox.Min.X + Fs * 2 || P.X > S.G.WorldBox.Max.X - Fs) { continue; }
			D.Text(FString::Printf(TEXT("%d"), (int)X), P.X, S.G.WorldBox.Max.Y - Fs * 0.7, Fs, Lbl, 0.5, 0.5);
		}
		for (double Y = Y0; Y <= Y1; Y += 1.0)
		{
			const FVector2D P = S.G.ToScreen(0, Y);
			if (P.Y < S.G.WorldBox.Min.Y + Fs || P.Y > S.G.WorldBox.Max.Y - Fs * 1.5) { continue; }
			D.Text(FString::Printf(TEXT("%d m"), (int)Y), S.G.WorldBox.Min.X + Fs * 0.5, P.Y, Fs, Lbl, 0, 0.5);
		}
		(void)L;
	}

	// Measurements drawn on the parts in Student Mode: ramp height, launcher angle.
	void DrawPartLabels(FScene& S, const CI::FPartInstance& Inst)
	{
		FCIDraw& D = S.D;
		const double Fs = FMath::Clamp(S.D.ScreenH * 0.022, 11.0, 26.0);
		const FLinearColor Ink = Col(0x1f2440);
		const FLinearColor Mark = Col(0xffffff, 0.95f);
		const CI::FPartGeometry& G = Inst.Geo;
		if (Inst.Part->Behavior == CI::EPartBehavior::Ramp && !G.Chains.empty())
		{
			const FVec2 Lip = G.Chains[0].Points[0], Top = G.Chains[0].Points[1], Foot = G.Chains[0].Points[2];
			const double X = Lip.X - 0.3 * G.Facing;
			D.Line(X, Foot.Y, X, Top.Y, 0.03, Mark);
			D.Line(X - 0.1, Foot.Y, X + 0.1, Foot.Y, 0.03, Mark);
			D.Line(X - 0.1, Top.Y, X + 0.1, Top.Y, 0.03, Mark);
			const FVector2D P = S.G.ToScreen(X - 0.15 * G.Facing, (Top.Y + Foot.Y) * 0.5);
			const FString Label = FString::Printf(TEXT("h = %.2f m"), Inst.Values.empty() ? 0.0 : Inst.Values[0]);
			const FVector2D Sz = D.MeasureText(Label, Fs, true);
			const double X0 = G.Facing > 0 ? P.X - Sz.X - Fs * 0.6 : P.X;
			D.SetScreenSpace();
			D.RoundRect(X0 - Fs * 0.3, P.Y - Fs * 0.7, X0 + Sz.X + Fs * 0.3, P.Y + Fs * 0.7, Fs * 0.4, FLinearColor(1, 1, 1, 0.9f));
			D.Text(Label, X0, P.Y, Fs, Ink, 0, 0.5, true);
			D.SetTransform(S.G.CamScale, S.G.CamOffX, S.G.CamOffY, true);
		}
		if (Inst.Part->Behavior == CI::EPartBehavior::Launcher && Inst.Values.size() >= 2)
		{
			const FVec2 Pv = G.Pivot;
			const double R = 1.1;
			const double A0 = G.Facing > 0 ? 0 : PI, A1 = G.Angle;
			D.Arc(Pv.X, Pv.Y, R - 0.02, R + 0.02, FMath::Min(A0, A1), FMath::Max(A0, A1), Mark, 24);
			D.Line(Pv.X, Pv.Y, Pv.X + (G.Facing > 0 ? 1.3 : -1.3), Pv.Y, 0.025, Mark);
			const double Mid = (A0 + A1) * 0.5;
			const FVector2D P = S.G.ToScreen(Pv.X + FMath::Cos(Mid) * (R + 0.45), Pv.Y + FMath::Sin(Mid) * (R + 0.45));
			const FString Label = FString::Printf(TEXT("θ = %.0f°   v = %.1f m/s"), Inst.Values[0], Inst.Values[1]);
			const FVector2D Sz = D.MeasureText(Label, Fs, true);
			D.SetScreenSpace();
			D.RoundRect(P.X - Fs * 0.3, P.Y - Fs * 0.7, P.X + Sz.X + Fs * 0.3, P.Y + Fs * 0.7, Fs * 0.4, FLinearColor(1, 1, 1, 0.9f));
			D.Text(Label, P.X, P.Y, Fs, Ink, 0, 0.5, true);
			D.SetTransform(S.G.CamScale, S.G.CamOffX, S.G.CamOffY, true);
		}
		if (Inst.Part->Behavior == CI::EPartBehavior::Brake && !Inst.Values.empty() && !G.Brakes.empty())
		{
			const CI::FBrakeRegion& R = G.Brakes[0];
			const FVector2D P = S.G.ToScreen((R.Min.X + R.Max.X) * 0.5, G.Base.Y - 0.35);
			const FString Label = FString::Printf(TEXT("a = %.1f m/s²"), Inst.Values[0]);
			const FVector2D Sz = D.MeasureText(Label, Fs, true);
			D.SetScreenSpace();
			D.RoundRect(P.X - Sz.X * 0.5 - Fs * 0.3, P.Y - Fs * 0.7, P.X + Sz.X * 0.5 + Fs * 0.3, P.Y + Fs * 0.7, Fs * 0.4, FLinearColor(1, 1, 1, 0.9f));
			D.Text(Label, P.X, P.Y, Fs, Ink, 0.5, 0.5, true);
			D.SetTransform(S.G.CamScale, S.G.CamOffX, S.G.CamOffY, true);
		}
	}

	// Soft light shafts fanning out from the sun (screen space, additive-looking translucent wedges).
	void DrawSunRays(FCIDraw& D, const CI::FWorldTheme& T, double Time)
	{
		const double W = D.ScreenW, H = D.ScreenH;
		const FVector2D Sun(W * 0.82, H * 0.2);
		for (int I = 0; I < 9; ++I)
		{
			const double A = PI * (0.55 + 0.1 * I) + 0.04 * FMath::Sin(Time * 0.3 + I);
			const double Wd = 0.035 + 0.02 * Hash(I * 4.2);
			const double Len = H * 1.6;
			const FVector2D P0 = Sun + FVector2D(FMath::Cos(A - Wd), FMath::Sin(A - Wd)) * Len;
			const FVector2D P1 = Sun + FVector2D(FMath::Cos(A + Wd), FMath::Sin(A + Wd)) * Len;
			const float Al = 0.10f + 0.05f * (float)FMath::Sin(Time * 0.5 + I * 1.3);
			D.TriColors(Sun, P0, P1, Col(T.Sun, Al), Col(T.Sun, 0.f), Col(T.Sun, 0.f));
		}
	}

	void DrawTree(FCIDraw& D, double X, double Y, double S, const FLinearColor& Leaf, double Sway)
	{
		D.Quad(FVector2D(X - S * 0.07, Y), FVector2D(X + S * 0.07, Y), FVector2D(X + S * 0.04 + Sway, Y - S * 0.6), FVector2D(X - S * 0.04 + Sway, Y - S * 0.6), Col(0x8a5a3a, Leaf.A));
		D.Circle(X + Sway, Y - S * 0.85, S * 0.38, Darken(Leaf, 0.12f), 20);
		D.Circle(X - S * 0.22 + Sway, Y - S * 0.68, S * 0.28, Leaf, 18);
		D.Circle(X + S * 0.24 + Sway, Y - S * 0.7, S * 0.3, Leaf, 18);
		D.Circle(X + S * 0.05 + Sway, Y - S * 1.0, S * 0.26, Lighten(Leaf, 0.15f), 18);
	}

	// Storybook props on the hills behind the machine: each world has its own.
	void DrawScenery(FCIDraw& D, const FCIGame& G, const CI::FWorldTheme& T, double Base)
	{
		const double W = D.ScreenW, H = D.ScreenH, Time = G.RealTime;
		const CI::FWorldDef* World = G.CurrentWorld();
		const bool bCliffs = World && World->Id == "gravity-cliffs";
		D.SetScreenSpace();
		if (bCliffs)
		{
			// Mesas on the horizon and hot-air balloons drifting.
			for (int I = 0; I < 4; ++I)
			{
				const double X = W * (0.08 + 0.27 * I + 0.05 * Hash(I * 3.0)), Wd = H * (0.1 + 0.06 * Hash(I + 5.0)), Ht = H * (0.08 + 0.07 * Hash(I + 9.0));
				const FLinearColor C = CIMix(Col(T.Hills[0]), Col(T.SkyBottom), 0.35f);
				D.Quad(FVector2D(X - Wd, Base), FVector2D(X + Wd, Base), FVector2D(X + Wd * 0.75, Base - Ht), FVector2D(X - Wd * 0.8, Base - Ht), C);
				D.Rect(X - Wd * 0.8, Base - Ht, X + Wd * 0.75, Base - Ht + H * 0.012, Lighten(C, 0.2f));
			}
			const uint32 Stripes[3] = { 0xff6f59, 0xffc93c, 0x5b9cf0 };
			for (int I = 0; I < 3; ++I)
			{
				const double X = FMath::Fmod(W * (0.2 + 0.33 * I) + Time * H * 0.006 * (1 + I * 0.3), W * 1.2) - W * 0.1;
				const double Y = H * (0.2 + 0.09 * I) + FMath::Sin(Time * 0.5 + I * 2.0) * H * 0.01;
				const double S = H * (0.045 - 0.008 * I);
				D.Ellipse(X, Y, S, S * 1.15, Col(Stripes[I]), 24);
				D.Ellipse(X, Y, S * 0.45, S * 1.15, Lighten(Col(Stripes[I]), 0.45f), 20);
				D.Line(X - S * 0.5, Y + S * 0.95, X - S * 0.18, Y + S * 1.6, 1.5, Col(0x6b4a2e));
				D.Line(X + S * 0.5, Y + S * 0.95, X + S * 0.18, Y + S * 1.6, 1.5, Col(0x6b4a2e));
				D.RoundRect(X - S * 0.22, Y + S * 1.55, X + S * 0.22, Y + S * 1.9, S * 0.06, Col(0x8a5a2b));
			}
			return;
		}
		// Meadow: a windmill turning on the far hill and round trees swaying.
		const double MX = W * 0.68, MY = Base - H * 0.085, S = H * 0.11;
		D.Quad(FVector2D(MX - S * 0.2, MY + S * 0.7), FVector2D(MX + S * 0.2, MY + S * 0.7), FVector2D(MX + S * 0.11, MY - S * 0.1), FVector2D(MX - S * 0.11, MY - S * 0.1), Col(0xfff6e5));
		D.Tri(MX - S * 0.16, MY - S * 0.1, MX + S * 0.16, MY - S * 0.1, MX, MY - S * 0.32, Col(0xe8604c));
		D.RoundRect(MX - S * 0.05, MY + S * 0.42, MX + S * 0.05, MY + S * 0.7, S * 0.04, Col(0x8a5a2b));
		for (int B = 0; B < 4; ++B)
		{
			const double A = Time * 0.9 + B * PI * 0.5;
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A)), N(-Dir.Y, Dir.X);
			const FVector2D C(MX, MY - S * 0.08);
			D.Line(C.X, C.Y, C.X + Dir.X * S * 0.62, C.Y + Dir.Y * S * 0.62, S * 0.035, Col(0x8a5a2b));
			D.Quad(C + Dir * (S * 0.18), C + Dir * (S * 0.62), C + Dir * (S * 0.62) + N * (S * 0.16), C + Dir * (S * 0.18) + N * (S * 0.1), Col(0xfdebc8, 0.95f));
		}
		D.Circle(MX, MY - S * 0.08, S * 0.05, Col(0x6b4a2e), 12);
		const double TreeX[5] = { 0.07, 0.19, 0.44, 0.86, 0.95 };
		for (int I = 0; I < 5; ++I)
		{
			const double X = W * TreeX[I], Sz = H * (0.07 + 0.035 * Hash(I * 2.3));
			const double Y = Base - H * (0.02 + 0.05 * Hash(I * 7.7));
			DrawTree(D, X, Y, Sz, CIMix(Col(T.Hills[2]), Col(0x3f9a5a), 0.5f), FMath::Sin(Time * 0.9 + I) * Sz * 0.03);
		}
	}

	// Grips: the handles you drag right on the machine. A soft pulsing ring so the eye finds them.
	void DrawGrips(FScene& S)
	{
		FCIDraw& D = S.D;
		FCIGame& G = S.G;
		const double Pulse = 0.5 + 0.5 * FMath::Sin(S.Time * 4.0);
		const double R = 26.0 / S.Px * (S.D.ScreenH / 900.0);
		for (const FCIGrip& Grip : G.Grips())
		{
			const bool bHeld = G.GripGrab == Grip.Placed;
			const FVector2D P = Grip.World;
			if (Grip.Kind == ECIGrip::LauncherAim)
			{
				if (!bHeld)
				{
					// "Pull me back": a ring around the ball and a little arrow pointing away from the shot.
					D.Ring(P.X, P.Y, R * (1.25 + 0.2 * Pulse), R * (1.4 + 0.2 * Pulse), FLinearColor(1, 1, 1, 0.85f - 0.4f * (float)Pulse), 32);
				}
				continue;
			}
			D.Glow(P.X, P.Y, R * 2.6, Col(0xffe066, bHeld ? 0.7f : 0.35f + 0.25f * (float)Pulse), Col(0xffe066, 0.f));
			D.Circle(P.X, P.Y - R * 0.12, R, Col(0x1f2440, 0.25f), 24);
			D.Circle(P.X, P.Y, R * (bHeld ? 1.12 : 1.0), FLinearColor::White, 24);
			D.Circle(P.X, P.Y, R * 0.62, Col(0xff6b5a), 20);
			// Arrows show which way it moves.
			const double A = R * 1.5, Hd = R * 0.5;
			if (Grip.Kind == ECIGrip::RampHeight || Grip.Kind == ECIGrip::Tilt)
			{
				D.Tri(P.X - Hd, P.Y + A, P.X + Hd, P.Y + A, P.X, P.Y + A + Hd * 1.3, FLinearColor(1, 1, 1, 0.9f));
				D.Tri(P.X - Hd, P.Y - A, P.X + Hd, P.Y - A, P.X, P.Y - A - Hd * 1.3, FLinearColor(1, 1, 1, 0.9f));
			}
			else
			{
				D.Tri(P.X + A, P.Y - Hd, P.X + A, P.Y + Hd, P.X + A + Hd * 1.3, P.Y, FLinearColor(1, 1, 1, 0.9f));
				D.Tri(P.X - A, P.Y - Hd, P.X - A, P.Y + Hd, P.X - A - Hd * 1.3, P.Y, FLinearColor(1, 1, 1, 0.9f));
			}
		}

		// Until the player has grabbed something, a ghost finger shows the move.
		if (!G.bHintDone && G.GripGrab < 0)
		{
			const double T = FMath::Fmod(S.Time, 2.2) / 2.2;
			const double E = FMath::SmoothStep(0.15, 0.75, T);
			const float Fade = (float)(FMath::SmoothStep(0.0, 0.12, T) * (1.0 - FMath::SmoothStep(0.85, 1.0, T)));
			for (const FCIGrip& Grip : G.Grips())
			{
				FVector2D To = Grip.World;
				if (Grip.Kind == ECIGrip::LauncherAim) { To += FVector2D(-1.5, -1.2); }
				else if (Grip.Kind == ECIGrip::RampHeight) { To += FVector2D(0, 0.7); }
				else if (Grip.Kind == ECIGrip::BrakeStrength) { To += FVector2D(1.0, 0); }
				else { To += FVector2D(0, 0.5); }
				const FVector2D F = FMath::Lerp(Grip.World, To, E);
				D.Line(Grip.World.X, Grip.World.Y, F.X, F.Y, R * 0.25, FLinearColor(1, 1, 1, 0.35f * Fade));
				D.Circle(F.X, F.Y - R * 0.15, R * 1.15, Col(0x1f2440, 0.2f * Fade), 24);
				D.Circle(F.X, F.Y, R * 1.1, FLinearColor(1, 1, 1, 0.9f * Fade), 24);
				D.Ring(F.X, F.Y, R * 1.3, R * 1.5, FLinearColor(1, 1, 1, 0.5f * Fade), 28);
				break;
			}
		}

		// Aiming a launcher: the slingshot bands from the finger to the barrel and a power arc.
		if (G.GripGrab >= 0 && G.GripKind == ECIGrip::LauncherAim)
		{
			const int32 Inst = (int32)S.L.Fixed.size() + G.GripGrab;
			if (Inst < (int32)G.Sim.Parts.size())
			{
				const CI::FPartGeometry& Geo = G.Sim.Parts[Inst].Geo;
				const FVec2 F = G.ToWorld(G.GripPointer);
				const FVec2 Dir(FMath::Cos(Geo.Angle), FMath::Sin(Geo.Angle));
				const FVec2 N = Dir.Perp();
				const FVec2 Mouth = Geo.Pivot + Dir * 0.55;
				const FLinearColor Band = G.bAimArmed ? CIMix(Col(0xffe066), Col(0xff5d57), (float)G.AimPower) : FLinearColor(1, 1, 1, 0.5f);
				D.TaperLine(Mouth.X + N.X * 0.14, Mouth.Y + N.Y * 0.14, F.X, F.Y, 0.09, 0.04, Band);
				D.TaperLine(Mouth.X - N.X * 0.14, Mouth.Y - N.Y * 0.14, F.X, F.Y, 0.09, 0.04, Band);
				D.Circle(F.X, F.Y, R * 0.9, FLinearColor(1, 1, 1, 0.9f), 20);
				D.Circle(F.X, F.Y, R * 0.55, Band, 16);
				// Power pips along the barrel direction.
				for (int K = 0; K < 8; ++K)
				{
					const bool bOn = G.bAimArmed && K < FMath::CeilToInt(G.AimPower * 8);
					const FVec2 Pp = Geo.Pivot - Dir * (0.5 + K * 0.16) + N * 0.42;
					D.Circle(Pp.X, Pp.Y, 0.05, bOn ? Band : FLinearColor(1, 1, 1, 0.3f), 10);
				}
			}
		}
	}

	// Darkened corners and a warm wash: pulls the eye to the machine.
	void DrawVignette(FCIDraw& D, const CI::FWorldTheme& T)
	{
		const double W = D.ScreenW, H = D.ScreenH;
		const FLinearColor Dark(0.06f, 0.05f, 0.16f, 0.18f), Clear(0.06f, 0.05f, 0.16f, 0.f);
		D.QuadColors(FVector2D(0, 0), FVector2D(W * 0.28, 0), FVector2D(W * 0.28, H), FVector2D(0, H), Dark, Clear, Clear, Dark);
		D.QuadColors(FVector2D(W * 0.72, 0), FVector2D(W, 0), FVector2D(W, H), FVector2D(W * 0.72, H), Clear, Dark, Dark, Clear);
		D.RectV(0, 0, W, H * 0.2, CIAlpha(Dark, 0.6f), Clear);
		D.Glow(W * 0.82, H * 0.2, H * 1.1, Col(T.Sun, 0.16f), Col(T.Sun, 0.f), 40);
	}

	void DrawLevel(FCIDraw& D, FCIGame& G)
	{
		const CI::FLevelDef& L = *G.CurrentLevel();
		const CI::FWorldTheme& T = ThemeOf(G);
		const double Horizon = G.ToScreen(0, L.ViewMin.Y + (L.ViewMax.Y - L.ViewMin.Y) * 0.45).Y;
		DrawSky(D, T, G.RealTime, Horizon);
		DrawSunRays(D, T, G.RealTime);
		// Mesas and balloons sit behind the dunes; the meadow's windmill and trees stand on its hills.
		const bool bFarScenery = G.CurrentWorld() && G.CurrentWorld()->Id == "gravity-cliffs";
		if (bFarScenery) { DrawScenery(D, G, T, Horizon - D.ScreenH * 0.02); }
		DrawHills(D, T, Horizon + D.ScreenH * 0.06, D.ScreenH * 0.22);
		if (!bFarScenery) { DrawScenery(D, G, T, Horizon + D.ScreenH * 0.02); }

		const double ShakeX = FMath::Sin(G.RealTime * 61.0) * G.Shake * 8.0;
		const double ShakeY = FMath::Cos(G.RealTime * 47.0) * G.Shake * 6.0;
		D.SetTransform(G.CamScale, G.CamOffX + ShakeX, G.CamOffY + ShakeY, true);
		FScene S{ D, G, L, T, G.RealTime, G.CamScale, L.ViewMin.Y - 60 };
		const bool bBuild = G.Phase == ECIPhase::Build;
		const bool bStudent = G.IsStudent();

		for (const CI::FSurfaceDef& Surf : L.Surfaces) { DrawSurface(S, Surf); }
		if (bStudent) { DrawGrid(S); }

		// Goal objects behind the bodies.
		const double AliveT = G.AliveTime;
		for (const CI::FZoneDef& Z : L.Zones)
		{
			if (Z.Look == "bell") { DrawBell(S, Z, AliveT >= 0 ? 0.55 * FMath::Sin(AliveT * 7.0) * FMath::Exp(-AliveT * 0.9) : 0.02 * FMath::Sin(G.RealTime * 1.3)); }
			if (Z.Look == "button") { DrawButton(S, Z, AliveT >= 0); }
			if (Z.Look == "patch")
			{
				// A bed of flowers: the target to stop on. They open when the machine is fixed.
				D.RoundRect(Z.Min.X, Z.Min.Y - 0.06, Z.Max.X, Z.Min.Y + 0.06, 0.05, Col(0x7a5236));
				const uint32 Petals[3] = { 0xff6b9a, 0xffc93c, 0xffffff };
				int K = 0;
				for (double X = Z.Min.X + 0.1; X < Z.Max.X - 0.05; X += 0.16, ++K)
				{
					const double H = 0.16 + 0.1 * Hash(K * 1.7) + (AliveT >= 0 ? 0.08 : 0.0);
					const double Sw = 0.02 * FMath::Sin(G.RealTime * 1.8 + K);
					D.Line(X, Z.Min.Y + 0.04, X + Sw, Z.Min.Y + H, 0.025, Col(0x3e8a4a));
					D.Circle(X + Sw, Z.Min.Y + H, AliveT >= 0 ? 0.075 : 0.055, Col(Petals[K % 3]), 10);
					D.Circle(X + Sw, Z.Min.Y + H, 0.022, Col(0xf2a900), 8);
				}
			}
		}

		// Slots: where parts can go.
		if (bBuild)
		{
			for (int32 I = 0; I < (int32)L.Slots.size(); ++I)
			{
				if (G.Setup.PartInSlot(I) >= 0 && !(G.Drag.bActive && G.Setup.PartInSlot(I) == G.Drag.FromPlaced)) { continue; }
				bool bFixed = false;
				for (const CI::FFixedPart& F : L.Fixed) { if (L.FindSlot(F.Slot) == I) { bFixed = true; } }
				if (bFixed) { continue; }
				const bool bAccepts = G.Drag.bActive && G.Drag.Tray >= 0 && L.Slots[I].AcceptsPart(L.Tray[G.Drag.Tray].Part);
				DrawSlot(S, I, bAccepts, G.Drag.bActive && G.Drag.HoverSlot == I);
			}
		}

		// Parts.
		for (int32 I = 0; I < (int32)G.Sim.Parts.size(); ++I)
		{
			const CI::FPartInstance& Inst = G.Sim.Parts[I];
			const int32 Placed = I - (int32)L.Fixed.size();
			if (G.Drag.bActive && Placed >= 0 && Placed == G.Drag.FromPlaced) { continue; }
			TArray<double> Values;
			for (double Val : Inst.Values) { Values.Add(Val); }
			const bool bSel = bBuild && Placed >= 0 && Placed == G.GripGrab;
			DrawPart(S, *Inst.Part, Inst.Geo, Values, bSel, FLinearColor::White, 0.f, 1.f);
		}

		// The part being dragged: green ghost on a slot, red where it can't go.
		if (G.Drag.bActive && G.Drag.Tray >= 0)
		{
			const CI::FTrayItem& Item = L.Tray[G.Drag.Tray];
			if (const CI::FPartDef* Def = G.Island.Catalog.Find(Item.Part))
			{
				std::vector<double> Vals;
				if (G.Drag.FromPlaced >= 0) { Vals = G.Setup.Placed[G.Drag.FromPlaced].Values; }
				else { for (const CI::FParamDef& Q : Item.Params) { Vals.push_back(Q.Default); } }
				CI::FSlotDef Where;
				if (G.Drag.bValid) { Where = L.Slots[G.Drag.HoverSlot]; }
				else { Where.Pos = G.ToWorld(G.Drag.Screen) - FVec2(0, 0.3); }
				CI::FPartGeometry Geo;
				CI::BuildPartGeometry(*Def, Vals, Where, Geo);
				TArray<double> Values;
				for (double Val : Vals) { Values.Add(Val); }
				DrawPart(S, *Def, Geo, Values, false, G.Drag.bValid ? Col(0x5cf08a) : Col(0xff4d4d), 0.45f, 0.72f);
			}
		}

		// Dotted path preview (the first moments of the real simulation).
		if (bBuild && G.Preview.Num() > 1)
		{
			for (int32 I = 1; I < G.Preview.Num(); ++I)
			{
				const float Fade = 1.f - (float)I / G.Preview.Num();
				const double Rd = 0.05 + 0.05 * Fade;
				D.Circle(G.Preview[I].X, G.Preview[I].Y, Rd + 0.025, Col(0x1f2440, 0.35f * Fade), 12);
				D.Circle(G.Preview[I].X, G.Preview[I].Y, Rd, FLinearColor(1, 1, 1, 0.35f + 0.6f * Fade), 12);
			}
		}

		// Motion streak behind the first body, and soft contact shadows.
		if (!bBuild && G.Trail.Num() > 1 && !G.Sim.Bodies.empty())
		{
			const double R0 = G.Sim.Bodies[0].R;
			for (int32 I = 0; I < G.Trail.Num(); ++I)
			{
				const float F = (float)(I + 1) / G.Trail.Num();
				D.Circle(G.Trail[I].X, G.Trail[I].Y, R0 * (0.25 + 0.6 * F), FLinearColor(1, 1, 1, 0.34f * F * F), 12);
			}
		}
		for (const CI::FBody& B : G.Sim.Bodies)
		{
			if (B.bGone || B.Contact < 0 || G.bSinking) { continue; }
			const FVec2 Foot = B.P - B.N * B.R;
			D.Ellipse(Foot.X, Foot.Y, B.R * 1.5, B.R * 0.3, Col(0x1f2440, 0.22f), 20, FMath::Atan2(-B.N.X, B.N.Y));
		}

		// Bodies (interpolated between physics steps).
		for (int32 I = 0; I < (int32)G.Sim.Bodies.size(); ++I)
		{
			const CI::FBody& B = G.Sim.Bodies[I];
			if (B.bGone) { continue; }
			const FVector2D Prev = I < G.PrevPos.Num() ? G.PrevPos[I] : FVector2D(B.P.X, B.P.Y);
			FVector2D P = FMath::Lerp(Prev, FVector2D(B.P.X, B.P.Y), G.Alpha);
			if (G.bSinking && G.Phase == ECIPhase::Result)
			{
				// Failed into water: it slowly sinks, trailing bubbles.
				P.Y -= FMath::Min(0.9, G.PhaseTime * 0.45);
				for (int K = 0; K < 5; ++K)
				{
					const double Age = FMath::Fmod(G.PhaseTime * 0.8 + K * 0.2, 1.0);
					D.Circle(P.X + 0.25 * FMath::Sin(K * 2.1 + G.PhaseTime * 3), P.Y + 0.2 + Age * 0.8, 0.03 + 0.03 * Age, FLinearColor(1, 1, 1, 0.7f * (float)(1 - Age)), 10);
				}
			}
			const double Roll = -(P.X - B.StartP.X) / B.R;
			if (L.Bodies[I].Kind == CI::EBodyKind::Cart) { DrawCart(S, P, B.R, Roll); }
			else { DrawBall(S, P, B.R, Roll); }
		}

		// Water in front of the bodies, so whatever falls in is really in it.
		for (const CI::FZoneDef& Z : L.Zones) { if (Z.Look == "pond" || Z.Look == "river") { DrawWater(S, Z); } }

		// Goal objects in front.
		for (const CI::FZoneDef& Z : L.Zones)
		{
			if (Z.Look == "basket") { DrawBasket(S, Z, AliveT >= 0 ? FMath::Max(0.0, 1.0 - AliveT * 0.4) : 0.0); }
		}

		if (bBuild && !G.Drag.bActive) { DrawGrips(S); }

		// Student Mode: measurements on the parts and velocity arrows on moving bodies.
		if (bStudent)
		{
			for (const CI::FPartInstance& Inst : G.Sim.Parts) { DrawPartLabels(S, Inst); }
			const double Fs = FMath::Clamp(D.ScreenH * 0.022, 11.0, 26.0);
			for (int32 I = 0; I < (int32)G.Sim.Bodies.size(); ++I)
			{
				const CI::FBody& B = G.Sim.Bodies[I];
				const double Speed = B.V.Length();
				if (B.bGone || Speed < 0.05 || bBuild) { continue; }
				const FVec2 Tip = B.P + B.V * 0.22;
				D.Arrow(B.P.X, B.P.Y, Tip.X, Tip.Y, 0.06, Col(0xffd23f));
				const FVector2D P = G.ToScreen(Tip.X, Tip.Y + 0.25);
				D.SetScreenSpace();
				const FString Label = FString::Printf(TEXT("%.1f m/s"), Speed);
				const FVector2D Sz = D.MeasureText(Label, Fs, true);
				D.RoundRect(P.X - Sz.X * 0.5 - Fs * 0.3, P.Y - Fs * 0.7, P.X + Sz.X * 0.5 + Fs * 0.3, P.Y + Fs * 0.7, Fs * 0.4, Col(0x1f2440, 0.8f));
				D.Text(Label, P.X, P.Y, Fs, Col(0xffd23f), 0.5, 0.5, true);
				D.SetTransform(G.CamScale, G.CamOffX, G.CamOffY, true);
			}
		}

		// Confetti, splashes and dust.
		for (const FCIParticle& P : G.Particles)
		{
			const float A = (float)FMath::Clamp((P.Life - P.Age) / 0.5, 0.0, 1.0);
			FLinearColor C = P.Color;
			C.A *= A;
			if (P.bRect)
			{
				const FVector2D Ax(FMath::Cos(P.Angle) * P.Size, FMath::Sin(P.Angle) * P.Size);
				const FVector2D Ay(-Ax.Y * 0.5 * FMath::Abs(FMath::Cos(P.Angle * 1.7)), Ax.X * 0.5 * FMath::Abs(FMath::Cos(P.Angle * 1.7)));
				D.Quad(P.P - Ax - Ay, P.P + Ax - Ay, P.P + Ax + Ay, P.P - Ax + Ay, C);
			}
			else { D.Circle(P.P.X, P.P.Y, P.Size * (1.0 + P.Age), C, 10); }
		}

		// The broken machine waits under a grey veil; solving it washes colour back out from the goal.
		D.SetScreenSpace();
		DrawVignette(D, T);
		const FLinearColor Veil = Col(0x7d7a99, 0.12f);
		if (AliveT < 0) { D.Rect(0, 0, D.ScreenW, D.ScreenH, Veil); }
		else
		{
			const FVector2D O = G.ToScreen(G.AliveOrigin.X, G.AliveOrigin.Y);
			const double R = AliveT * D.ScreenH * 2.2;
			const double Far = D.ScreenW + D.ScreenH;
			if (R < Far)
			{
				D.Ring(O.X, O.Y, R, Far * 1.5, Veil, 64);
				const float Wave = (float)FMath::Clamp(1.0 - AliveT * 0.9, 0.0, 1.0);
				D.Ring(O.X, O.Y, FMath::Max(0.0, R - D.ScreenH * 0.03), R, FLinearColor(1, 1, 0.85f, 0.55f * Wave), 64);
			}
		}
	}
}

void FCIWorldRenderer::Draw(FCIDraw& D, FCIGame& G)
{
	if (G.Screen == ECIScreen::Playing && G.CurrentLevel()) { DrawLevel(D, G); }
	else { DrawArchipelago(D, G, G.Screen == ECIScreen::Levels ? 0.28f : 0.f); }
	D.SetScreenSpace();
}
