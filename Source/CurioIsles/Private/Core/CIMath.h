// CURIO ISLES: deterministic maths for the simulation core. (CLAUDE.md: Deterministic core)
// The same inputs must give bit-identical results on x64 (Windows) and ARM64 (Android, iOS). Only
// +, -, *, / and sqrt are used - IEEE-754 rounds those identically everywhere - plus our own Sin/Cos
// (library versions differ in the last bit between platforms). Fused multiply-add is forbidden here.
#pragma once

#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(_MSC_VER)
#pragma fp_contract(off)
#endif

#include <cmath>

namespace CI
{
	constexpr double kPi = 3.14159265358979323846;
	constexpr double kDegToRad = kPi / 180.0;

	inline double Abs(double X) { return X < 0 ? -X : X; }
	inline double Min(double A, double B) { return A < B ? A : B; }
	inline double Max(double A, double B) { return A > B ? A : B; }
	inline double Clamp(double X, double Lo, double Hi) { return X < Lo ? Lo : (X > Hi ? Hi : X); }
	inline double Sign(double X) { return X > 0 ? 1.0 : (X < 0 ? -1.0 : 0.0); }
	// IEEE-754 requires sqrt to be correctly rounded, so std::sqrt is identical on every platform.
	inline double Sqrt(double X) { return std::sqrt(X); }

	double Sin(double X);
	double Cos(double X);

	struct FVec2
	{
		double X = 0, Y = 0;

		FVec2() = default;
		FVec2(double InX, double InY) : X(InX), Y(InY) {}

		FVec2 operator+(const FVec2& O) const { return FVec2(X + O.X, Y + O.Y); }
		FVec2 operator-(const FVec2& O) const { return FVec2(X - O.X, Y - O.Y); }
		FVec2 operator-() const { return FVec2(-X, -Y); }
		FVec2 operator*(double S) const { return FVec2(X * S, Y * S); }
		FVec2 operator/(double S) const { return FVec2(X / S, Y / S); }
		FVec2& operator+=(const FVec2& O) { X += O.X; Y += O.Y; return *this; }
		FVec2& operator-=(const FVec2& O) { X -= O.X; Y -= O.Y; return *this; }
		FVec2& operator*=(double S) { X *= S; Y *= S; return *this; }
		bool operator==(const FVec2& O) const { return X == O.X && Y == O.Y; }
		bool operator!=(const FVec2& O) const { return !(*this == O); }

		double Dot(const FVec2& O) const { return X * O.X + Y * O.Y; }
		double Cross(const FVec2& O) const { return X * O.Y - Y * O.X; }
		double LengthSq() const { return X * X + Y * Y; }
		double Length() const { return Sqrt(LengthSq()); }
		FVec2 Normalized() const { const double L = Length(); return L > 0 ? FVec2(X / L, Y / L) : FVec2(); }
		FVec2 Perp() const { return FVec2(-Y, X); }   // 90 degrees counter-clockwise
	};

	inline FVec2 operator*(double S, const FVec2& V) { return V * S; }
	inline FVec2 FromAngle(double Radians) { return FVec2(Cos(Radians), Sin(Radians)); }

	// Closest point on segment AB to P, and its parameter along AB in [0, 1].
	inline FVec2 ClosestOnSegment(const FVec2& A, const FVec2& B, const FVec2& P, double& OutT)
	{
		const FVec2 AB = B - A;
		const double L2 = AB.LengthSq();
		OutT = L2 > 0 ? Clamp((P - A).Dot(AB) / L2, 0.0, 1.0) : 0.0;
		return A + AB * OutT;
	}
}
