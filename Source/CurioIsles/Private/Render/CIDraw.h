// CURIO ISLES: batched 2D triangle drawing and text on a UCanvas. (CLAUDE.md: Rendering)
// Everything in the game is drawn from these primitives; the look lives in code, not imported assets.
#pragma once

#include "CoreMinimal.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"

class UFont;

class FCIDraw
{
public:
	FCIDraw(UCanvas* InCanvas, UFont* InFont);
	~FCIDraw();

	// Primitives take coordinates in the current transform:
	//   screen = Offset + (X, FlipY ? -Y : Y) * Scale
	// The world uses metres with y up (FlipY); the UI uses pixels (SetScreenSpace).
	void SetTransform(double InScale, double InOffsetX, double InOffsetY, bool bInFlipY);
	void SetScreenSpace() { SetTransform(1, 0, 0, false); }
	FVector2D ToScreen(double X, double Y) const { return FVector2D(OffsetX + X * Scale, OffsetY + (bFlipY ? -Y : Y) * Scale); }
	double GetScale() const { return Scale; }

	void SetBlend(ESimpleElementBlendMode Mode);
	void Flush();

	void Tri(double AX, double AY, double BX, double BY, double CX, double CY, const FLinearColor& Color);
	void TriColors(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC);
	void Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& Color);
	void QuadColors(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC, const FLinearColor& CD);
	void Rect(double X0, double Y0, double X1, double Y1, const FLinearColor& Color);
	// Gradient between the Y0 edge (C0) and the Y1 edge (C1).
	void RectV(double X0, double Y0, double X1, double Y1, const FLinearColor& C0, const FLinearColor& C1);
	void RoundRect(double X0, double Y0, double X1, double Y1, double Radius, const FLinearColor& Color);
	void RoundRectV(double X0, double Y0, double X1, double Y1, double Radius, const FLinearColor& C0, const FLinearColor& C1);
	// A soft drop shadow under a rounded rectangle.
	void Shadow(double X0, double Y0, double X1, double Y1, double Radius, double Blur, const FLinearColor& Color);
	void Circle(double CX, double CY, double R, const FLinearColor& Color, int Segments = 24);
	void Ellipse(double CX, double CY, double RX, double RY, const FLinearColor& Color, int Segments = 24, double Rotation = 0);
	// Radial gradient, (1 - r)^2 falloff so the edge has no visible rim.
	void Glow(double CX, double CY, double R, const FLinearColor& Center, const FLinearColor& Edge, int Segments = 32);
	void Ring(double CX, double CY, double R0, double R1, const FLinearColor& Color, int Segments = 40);
	void Arc(double CX, double CY, double R0, double R1, double A0, double A1, const FLinearColor& Color, int Segments = 24);
	void Line(double AX, double AY, double BX, double BY, double Width, const FLinearColor& Color);
	void TaperLine(double AX, double AY, double BX, double BY, double WA, double WB, const FLinearColor& Color);
	void RoundLine(double AX, double AY, double BX, double BY, double Width, const FLinearColor& Color);
	void DashedLine(double AX, double AY, double BX, double BY, double Width, double Dash, double Gap, const FLinearColor& Color);
	void Arrow(double AX, double AY, double BX, double BY, double Width, const FLinearColor& Color);
	// Any simple polygon (convex or not, either winding), triangulated by ear clipping.
	void Polygon(const TArray<FVector2D>& Points, const FLinearColor& Color);
	void Polyline(const TArray<FVector2D>& Points, double Width, const FLinearColor& Color, bool bClosed = false);

	// Text in screen pixels. Align 0 = left/top, 0.5 = centre, 1 = right/bottom. Returns the drawn size.
	FVector2D Text(const FString& S, double X, double Y, double Px, const FLinearColor& Color, double AlignX = 0, double AlignY = 0.5, bool bBold = false);
	FVector2D MeasureText(const FString& S, double Px, bool bBold = false) const;
	// Greedy word wrap to MaxWidth; draws lines from Y downward. Returns the total height.
	double TextWrapped(const FString& S, double X, double Y, double MaxWidth, double Px, double LineGap, const FLinearColor& Color, double AlignX = 0, bool bBold = false);
	TArray<FString> Wrap(const FString& S, double MaxWidth, double Px, bool bBold = false) const;

	UCanvas* Canvas;
	UFont* Font;
	double ScreenW = 0, ScreenH = 0;

private:
	void Push(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC);
	void RoundRectPoints(double X0, double Y0, double X1, double Y1, double Radius, TArray<FVector2D>& Out) const;

	TArray<FCanvasUVTri> Batch;
	ESimpleElementBlendMode Blend = SE_BLEND_Translucent;
	double Scale = 1, OffsetX = 0, OffsetY = 0;
	bool bFlipY = false;
};

// sRGB hex -> linear colour with alpha.
FLinearColor CIColor(uint32 RGB, float Alpha = 1.f);
FLinearColor CIMix(const FLinearColor& A, const FLinearColor& B, float T);
FLinearColor CIAlpha(const FLinearColor& C, float Alpha);
