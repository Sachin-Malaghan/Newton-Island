// CURIO ISLES: deterministic Sin/Cos (fdlibm kernels, Cody-Waite reduction). (CLAUDE.md: Deterministic core)
#include "CIMath.h"

namespace CI
{
	namespace
	{
		// pi/2 split so k * PiO2Hi is exact for |k| < 2^20.
		constexpr double PiO2Hi = 1.57079632673412561417e+00;
		constexpr double PiO2Lo = 6.07710050650619224932e-11;
		constexpr double TwoOverPi = 6.36619772367581382433e-01;

		double KernelSin(double X)
		{
			constexpr double S1 = -1.66666666666666324348e-01, S2 = 8.33333333332248946124e-03,
				S3 = -1.98412698298579493134e-04, S4 = 2.75573137070700676789e-06,
				S5 = -2.50507602534068634195e-08, S6 = 1.58969099521155010221e-10;
			const double Z = X * X;
			const double R = S2 + Z * (S3 + Z * (S4 + Z * (S5 + Z * S6)));
			return X + X * Z * (S1 + Z * R);
		}

		double KernelCos(double X)
		{
			constexpr double C1 = 4.16666666666666019037e-02, C2 = -1.38888888887411279913e-03,
				C3 = 2.48015872894767294178e-05, C4 = -2.75573143513906633035e-07,
				C5 = 2.08757232129817482790e-09, C6 = -1.13596475577881948265e-11;
			const double Z = X * X;
			const double R = Z * (C1 + Z * (C2 + Z * (C3 + Z * (C4 + Z * (C5 + Z * C6)))));
			return 1.0 - (0.5 * Z - Z * R);
		}

		// Reduces X to R in [-pi/4, pi/4] and returns the quadrant (0..3).
		int Reduce(double X, double& R)
		{
			const double K = std::floor(X * TwoOverPi + 0.5);
			R = (X - K * PiO2Hi) - K * PiO2Lo;
			const long long Q = (long long)K;
			return (int)(((Q % 4) + 4) % 4);
		}
	}

	double Sin(double X)
	{
		double R;
		switch (Reduce(X, R))
		{
		case 0: return KernelSin(R);
		case 1: return KernelCos(R);
		case 2: return -KernelSin(R);
		default: return -KernelCos(R);
		}
	}

	double Cos(double X)
	{
		double R;
		switch (Reduce(X, R))
		{
		case 0: return KernelCos(R);
		case 1: return -KernelSin(R);
		case 2: return -KernelCos(R);
		default: return KernelSin(R);
		}
	}
}
