// CURIO ISLES: batched 2D triangle drawing and text on a UCanvas. (CLAUDE.md: Rendering)
#include "Render/CIDraw.h"

#include "CanvasItem.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "RenderUtils.h"

FLinearColor CIColor(uint32 RGB, float Alpha)
{
	FLinearColor C = FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xff, (RGB >> 8) & 0xff, RGB & 0xff, 255));
	C.A = Alpha;
	return C;
}

FLinearColor CIMix(const FLinearColor& A, const FLinearColor& B, float T)
{
	return A + (B - A) * FMath::Clamp(T, 0.f, 1.f);
}

FLinearColor CIAlpha(const FLinearColor& C, float Alpha)
{
	FLinearColor R = C;
	R.A = C.A * Alpha;
	return R;
}

namespace
{
	FSlateFontInfo FontInfo(UFont* Font, double Px, bool bBold)
	{
		// Slate sizes are points at 96 dpi: 1 px = 0.75 pt.
		return FSlateFontInfo(Font, FMath::Max(1, FMath::RoundToInt(Px * 0.75)), bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
	}
}

FCIDraw::FCIDraw(UCanvas* InCanvas, UFont* InFont)
	: Canvas(InCanvas)
	, Font(InFont)
{
	ScreenW = Canvas->ClipX;
	ScreenH = Canvas->ClipY;
	Batch.Reserve(8192);
}

FCIDraw::~FCIDraw()
{
	Flush();
}

void FCIDraw::SetTransform(double InScale, double InOffsetX, double InOffsetY, bool bInFlipY)
{
	Scale = InScale;
	OffsetX = InOffsetX;
	OffsetY = InOffsetY;
	bFlipY = bInFlipY;
}

void FCIDraw::SetBlend(ESimpleElementBlendMode Mode)
{
	if (Mode != Blend)
	{
		Flush();
		Blend = Mode;
	}
}

void FCIDraw::Flush()
{
	if (Batch.Num() == 0 || !Canvas || !Canvas->Canvas) { Batch.Reset(); return; }
	FCanvasTriangleItem Item(Batch, GWhiteTexture);
	Item.BlendMode = Blend;
	Canvas->DrawItem(Item);
	Batch.Reset();
}

void FCIDraw::Push(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC)
{
	FCanvasUVTri& T = Batch.AddDefaulted_GetRef();
	T.V0_Pos = A; T.V1_Pos = B; T.V2_Pos = C;
	T.V0_UV = T.V1_UV = T.V2_UV = FVector2D(0.5, 0.5);
	T.V0_Color = CA; T.V1_Color = CB; T.V2_Color = CC;
	if (Batch.Num() >= 16000) { Flush(); }
}

void FCIDraw::Tri(double AX, double AY, double BX, double BY, double CX, double CY, const FLinearColor& Color)
{
	Push(ToScreen(AX, AY), ToScreen(BX, BY), ToScreen(CX, CY), Color, Color, Color);
}

void FCIDraw::TriColors(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC)
{
	Push(ToScreen(A.X, A.Y), ToScreen(B.X, B.Y), ToScreen(C.X, C.Y), CA, CB, CC);
}

void FCIDraw::Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& Color)
{
	QuadColors(A, B, C, D, Color, Color, Color, Color);
}

void FCIDraw::QuadColors(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC, const FLinearColor& CD)
{
	const FVector2D SA = ToScreen(A.X, A.Y), SB = ToScreen(B.X, B.Y), SC = ToScreen(C.X, C.Y), SD = ToScreen(D.X, D.Y);
	Push(SA, SB, SC, CA, CB, CC);
	Push(SA, SC, SD, CA, CC, CD);
}

void FCIDraw::Rect(double X0, double Y0, double X1, double Y1, const FLinearColor& Color)
{
	RectV(X0, Y0, X1, Y1, Color, Color);
}

void FCIDraw::RectV(double X0, double Y0, double X1, double Y1, const FLinearColor& C0, const FLinearColor& C1)
{
	QuadColors(FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1), C0, C0, C1, C1);
}

void FCIDraw::RoundRectPoints(double X0, double Y0, double X1, double Y1, double Radius, TArray<FVector2D>& Out) const
{
	const double MinX = FMath::Min(X0, X1), MaxX = FMath::Max(X0, X1), MinY = FMath::Min(Y0, Y1), MaxY = FMath::Max(Y0, Y1);
	const double R = FMath::Clamp(Radius, 0.0, FMath::Min(MaxX - MinX, MaxY - MinY) * 0.5);
	constexpr int Seg = 6;
	const FVector2D Centers[4] = { { MaxX - R, MaxY - R }, { MinX + R, MaxY - R }, { MinX + R, MinY + R }, { MaxX - R, MinY + R } };
	Out.Reset();
	for (int C = 0; C < 4; ++C)
	{
		for (int I = 0; I <= Seg; ++I)
		{
			const double A = (C * 0.5 + 0.5 * I / Seg) * PI;
			Out.Add(Centers[C] + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
		}
	}
}

void FCIDraw::RoundRect(double X0, double Y0, double X1, double Y1, double Radius, const FLinearColor& Color)
{
	RoundRectV(X0, Y0, X1, Y1, Radius, Color, Color);
}

void FCIDraw::RoundRectV(double X0, double Y0, double X1, double Y1, double Radius, const FLinearColor& C0, const FLinearColor& C1)
{
	TArray<FVector2D> P;
	RoundRectPoints(X0, Y0, X1, Y1, Radius, P);
	const double Span = Y1 - Y0;
	auto ColorAt = [&](const FVector2D& V) { return Span != 0 ? CIMix(C0, C1, (float)((V.Y - Y0) / Span)) : C0; };
	const FVector2D Mid((X0 + X1) * 0.5, (Y0 + Y1) * 0.5);
	const FLinearColor CM = ColorAt(Mid);
	for (int I = 0; I < P.Num(); ++I)
	{
		const FVector2D& A = P[I];
		const FVector2D& B = P[(I + 1) % P.Num()];
		TriColors(Mid, A, B, CM, ColorAt(A), ColorAt(B));
	}
}

void FCIDraw::Shadow(double X0, double Y0, double X1, double Y1, double Radius, double Blur, const FLinearColor& Color)
{
	constexpr int Layers = 5;
	for (int I = Layers; I >= 1; --I)
	{
		const double G = Blur * I / Layers;
		RoundRect(X0 - G, Y0 - G, X1 + G, Y1 + G, Radius + G, CIAlpha(Color, 1.f / Layers));
	}
}

void FCIDraw::Circle(double CX, double CY, double R, const FLinearColor& Color, int Segments)
{
	Ellipse(CX, CY, R, R, Color, Segments, 0);
}

void FCIDraw::Ellipse(double CX, double CY, double RX, double RY, const FLinearColor& Color, int Segments, double Rotation)
{
	const FVector2D C = ToScreen(CX, CY);
	const double CR = FMath::Cos(Rotation), SR = FMath::Sin(Rotation);
	auto P = [&](int I)
	{
		const double A = 2.0 * PI * I / Segments;
		const double LX = FMath::Cos(A) * RX, LY = FMath::Sin(A) * RY;
		return ToScreen(CX + LX * CR - LY * SR, CY + LX * SR + LY * CR);
	};
	FVector2D Prev = P(0);
	for (int I = 1; I <= Segments; ++I)
	{
		const FVector2D Next = P(I);
		Push(C, Prev, Next, Color, Color, Color);
		Prev = Next;
	}
}

void FCIDraw::Glow(double CX, double CY, double R, const FLinearColor& Center, const FLinearColor& Edge, int Segments)
{
	static const double Radii[] = { 0.0, 0.1, 0.24, 0.42, 0.62, 0.82, 1.0 };
	constexpr int NumRings = UE_ARRAY_COUNT(Radii);
	FLinearColor Cols[NumRings];
	for (int K = 0; K < NumRings; ++K)
	{
		const float F = (float)FMath::Square(1.0 - Radii[K]);
		Cols[K] = Center * F + Edge * (1.f - F);
	}
	for (int I = 0; I < Segments; ++I)
	{
		const double A0 = 2.0 * PI * I / Segments, A1 = 2.0 * PI * (I + 1) / Segments;
		const double C0 = FMath::Cos(A0), S0 = FMath::Sin(A0), C1 = FMath::Cos(A1), S1 = FMath::Sin(A1);
		for (int K = 0; K + 1 < NumRings; ++K)
		{
			const double RA = R * Radii[K], RB = R * Radii[K + 1];
			const FVector2D In0 = ToScreen(CX + C0 * RA, CY + S0 * RA), In1 = ToScreen(CX + C1 * RA, CY + S1 * RA);
			const FVector2D Out0 = ToScreen(CX + C0 * RB, CY + S0 * RB), Out1 = ToScreen(CX + C1 * RB, CY + S1 * RB);
			if (K == 0) { Push(In0, Out0, Out1, Cols[0], Cols[1], Cols[1]); continue; }
			Push(In0, Out0, Out1, Cols[K], Cols[K + 1], Cols[K + 1]);
			Push(In0, Out1, In1, Cols[K], Cols[K + 1], Cols[K]);
		}
	}
}

void FCIDraw::Ring(double CX, double CY, double R0, double R1, const FLinearColor& Color, int Segments)
{
	Arc(CX, CY, R0, R1, 0, 2 * PI, Color, Segments);
}

void FCIDraw::Arc(double CX, double CY, double R0, double R1, double A0, double A1, const FLinearColor& Color, int Segments)
{
	for (int I = 0; I < Segments; ++I)
	{
		const double T0 = A0 + (A1 - A0) * I / Segments, T1 = A0 + (A1 - A0) * (I + 1) / Segments;
		const FVector2D In0 = ToScreen(CX + FMath::Cos(T0) * R0, CY + FMath::Sin(T0) * R0);
		const FVector2D In1 = ToScreen(CX + FMath::Cos(T1) * R0, CY + FMath::Sin(T1) * R0);
		const FVector2D Out0 = ToScreen(CX + FMath::Cos(T0) * R1, CY + FMath::Sin(T0) * R1);
		const FVector2D Out1 = ToScreen(CX + FMath::Cos(T1) * R1, CY + FMath::Sin(T1) * R1);
		Push(In0, Out0, Out1, Color, Color, Color);
		Push(In0, Out1, In1, Color, Color, Color);
	}
}

void FCIDraw::Line(double AX, double AY, double BX, double BY, double Width, const FLinearColor& Color)
{
	TaperLine(AX, AY, BX, BY, Width, Width, Color);
}

void FCIDraw::TaperLine(double AX, double AY, double BX, double BY, double WA, double WB, const FLinearColor& Color)
{
	const FVector2D D(BX - AX, BY - AY);
	const double Len = D.Size();
	if (Len < 1e-9) { return; }
	const FVector2D N(-D.Y / Len, D.X / Len);
	Quad(FVector2D(AX, AY) + N * (WA * 0.5), FVector2D(BX, BY) + N * (WB * 0.5),
	     FVector2D(BX, BY) - N * (WB * 0.5), FVector2D(AX, AY) - N * (WA * 0.5), Color);
}

void FCIDraw::RoundLine(double AX, double AY, double BX, double BY, double Width, const FLinearColor& Color)
{
	Line(AX, AY, BX, BY, Width, Color);
	Circle(AX, AY, Width * 0.5, Color, 12);
	Circle(BX, BY, Width * 0.5, Color, 12);
}

void FCIDraw::DashedLine(double AX, double AY, double BX, double BY, double Width, double Dash, double Gap, const FLinearColor& Color)
{
	const FVector2D A(AX, AY), B(BX, BY);
	const double Len = FVector2D::Distance(A, B);
	if (Len < 1e-9 || Dash <= 0) { return; }
	const FVector2D Dir = (B - A) / Len;
	for (double T = 0; T < Len; T += Dash + Gap)
	{
		const FVector2D P0 = A + Dir * T, P1 = A + Dir * FMath::Min(Len, T + Dash);
		Line(P0.X, P0.Y, P1.X, P1.Y, Width, Color);
	}
}

void FCIDraw::Arrow(double AX, double AY, double BX, double BY, double Width, const FLinearColor& Color)
{
	const FVector2D A(AX, AY), B(BX, BY);
	const double Len = FVector2D::Distance(A, B);
	if (Len < 1e-9) { return; }
	const FVector2D Dir = (B - A) / Len, N(-Dir.Y, Dir.X);
	const double Head = FMath::Min(Len * 0.45, Width * 3.2);
	const FVector2D Base = B - Dir * Head;
	Line(A.X, A.Y, Base.X, Base.Y, Width, Color);
	const FVector2D L = Base + N * (Head * 0.6), R = Base - N * (Head * 0.6);
	Tri(B.X, B.Y, L.X, L.Y, R.X, R.Y, Color);
}

void FCIDraw::Polygon(const TArray<FVector2D>& Points, const FLinearColor& Color)
{
	const int N = Points.Num();
	if (N < 3) { return; }
	// Signed area decides the winding; ears are convex vertices with no other vertex inside.
	double Area = 0;
	for (int I = 0; I < N; ++I) { const FVector2D& A = Points[I]; const FVector2D& B = Points[(I + 1) % N]; Area += A.X * B.Y - B.X * A.Y; }
	const double Wind = Area >= 0 ? 1.0 : -1.0;
	TArray<int32> Idx;
	for (int I = 0; I < N; ++I) { Idx.Add(I); }
	auto Cross = [](const FVector2D& O, const FVector2D& A, const FVector2D& B) { return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X); };
	int Guard = 0;
	while (Idx.Num() > 3 && Guard++ < N * N)
	{
		bool bFound = false;
		for (int I = 0; I < Idx.Num(); ++I)
		{
			const FVector2D& A = Points[Idx[(I + Idx.Num() - 1) % Idx.Num()]];
			const FVector2D& B = Points[Idx[I]];
			const FVector2D& C = Points[Idx[(I + 1) % Idx.Num()]];
			if (Cross(A, B, C) * Wind <= 0) { continue; }
			bool bInside = false;
			for (int K = 0; K < Idx.Num() && !bInside; ++K)
			{
				const FVector2D& P = Points[Idx[K]];
				if (&P == &A || &P == &B || &P == &C) { continue; }
				bInside = Cross(A, B, P) * Wind > 0 && Cross(B, C, P) * Wind > 0 && Cross(C, A, P) * Wind > 0;
			}
			if (bInside) { continue; }
			Tri(A.X, A.Y, B.X, B.Y, C.X, C.Y, Color);
			Idx.RemoveAt(I);
			bFound = true;
			break;
		}
		if (!bFound) { break; }
	}
	if (Idx.Num() >= 3)
	{
		for (int I = 1; I + 1 < Idx.Num(); ++I)
		{
			const FVector2D& A = Points[Idx[0]]; const FVector2D& B = Points[Idx[I]]; const FVector2D& C = Points[Idx[I + 1]];
			Tri(A.X, A.Y, B.X, B.Y, C.X, C.Y, Color);
		}
	}
}

void FCIDraw::Polyline(const TArray<FVector2D>& Points, double Width, const FLinearColor& Color, bool bClosed)
{
	const int N = Points.Num();
	for (int I = 0; I + 1 < N + (bClosed ? 1 : 0); ++I)
	{
		const FVector2D& A = Points[I];
		const FVector2D& B = Points[(I + 1) % N];
		Line(A.X, A.Y, B.X, B.Y, Width, Color);
		Circle(B.X, B.Y, Width * 0.5, Color, 10);
	}
}

FVector2D FCIDraw::MeasureText(const FString& S, double Px, bool bBold) const
{
	if (!Font || S.IsEmpty() || !FSlateApplication::IsInitialized()) { return FVector2D(S.Len() * Px * 0.5, Px); }
	const TSharedRef<FSlateFontMeasure> M = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return M->Measure(S, FontInfo(Font, Px, bBold));
}

FVector2D FCIDraw::Text(const FString& S, double X, double Y, double Px, const FLinearColor& Color, double AlignX, double AlignY, bool bBold)
{
	if (!Font || S.IsEmpty() || Color.A <= 0.002f) { return FVector2D::ZeroVector; }
	Flush();   // keep painter's order with the triangle batches
	const FSlateFontInfo Info = FontInfo(Font, Px, bBold);
	const FVector2D Size = MeasureText(S, Px, bBold);
	FCanvasTextItem Item(FVector2D(X - Size.X * AlignX, Y - Size.Y * AlignY), FText::FromString(S), Info, Color);
	Canvas->DrawItem(Item);
	return Size;
}

TArray<FString> FCIDraw::Wrap(const FString& S, double MaxWidth, double Px, bool bBold) const
{
	TArray<FString> Lines;
	TArray<FString> Paragraphs;
	S.ParseIntoArray(Paragraphs, TEXT("\n"), false);
	for (const FString& Para : Paragraphs)
	{
		TArray<FString> Words;
		Para.ParseIntoArray(Words, TEXT(" "), true);
		FString Line;
		for (const FString& W : Words)
		{
			const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
			if (!Line.IsEmpty() && MeasureText(Try, Px, bBold).X > MaxWidth)
			{
				Lines.Add(Line);
				Line = W;
			}
			else { Line = Try; }
		}
		Lines.Add(Line);
	}
	return Lines;
}

double FCIDraw::TextWrapped(const FString& S, double X, double Y, double MaxWidth, double Px, double LineGap, const FLinearColor& Color, double AlignX, bool bBold)
{
	const TArray<FString> Lines = Wrap(S, MaxWidth, Px, bBold);
	const double Step = Px * LineGap;
	for (int I = 0; I < Lines.Num(); ++I) { Text(Lines[I], X, Y + I * Step, Px, Color, AlignX, 0, bBold); }
	return Lines.Num() * Step;
}
