// CURIO ISLES: tiny formula evaluator for worked examples and prediction answers. (CLAUDE.md: Learning features)
// Display maths only - never used inside the simulation, so it may use the C library freely.
#include "CIExpr.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace CI
{
	namespace
	{
		struct FEval
		{
			const std::string& S;
			const FVarLookup& Vars;
			size_t P = 0;
			std::string Error;

			void Space() { while (P < S.size() && (S[P] == ' ' || S[P] == '\t')) { ++P; } }
			bool Fail(const std::string& M) { if (Error.empty()) { Error = M; } return false; }

			bool Expr(double& Out)
			{
				if (!Term(Out)) { return false; }
				for (;;)
				{
					Space();
					if (P < S.size() && (S[P] == '+' || S[P] == '-'))
					{
						const char Op = S[P++];
						double R;
						if (!Term(R)) { return false; }
						Out = Op == '+' ? Out + R : Out - R;
					}
					else { return true; }
				}
			}

			bool Term(double& Out)
			{
				if (!Unary(Out)) { return false; }
				for (;;)
				{
					Space();
					if (P < S.size() && (S[P] == '*' || S[P] == '/'))
					{
						const char Op = S[P++];
						double R;
						if (!Unary(R)) { return false; }
						Out = Op == '*' ? Out * R : Out / R;
					}
					else { return true; }
				}
			}

			bool Unary(double& Out)
			{
				Space();
				if (P < S.size() && S[P] == '-') { ++P; if (!Unary(Out)) { return false; } Out = -Out; return true; }
				if (P < S.size() && S[P] == '+') { ++P; return Unary(Out); }
				if (!Atom(Out)) { return false; }
				Space();
				if (P < S.size() && S[P] == '^')
				{
					++P;
					double E;
					if (!Unary(E)) { return false; }
					Out = std::pow(Out, E);
				}
				return true;
			}

			bool Atom(double& Out)
			{
				Space();
				if (P >= S.size()) { return Fail("unexpected end"); }
				const char C = S[P];
				if (C == '(')
				{
					++P;
					if (!Expr(Out)) { return false; }
					Space();
					if (P >= S.size() || S[P] != ')') { return Fail("missing )"); }
					++P;
					return true;
				}
				if ((C >= '0' && C <= '9') || C == '.')
				{
					const char* Begin = S.c_str() + P;
					char* End = nullptr;
					Out = std::strtod(Begin, &End);
					P += (size_t)(End - Begin);
					return true;
				}
				if ((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || C == '_')
				{
					const size_t Start = P;
					while (P < S.size() && ((S[P] >= 'a' && S[P] <= 'z') || (S[P] >= 'A' && S[P] <= 'Z') || (S[P] >= '0' && S[P] <= '9') || S[P] == '_' || S[P] == '.')) { ++P; }
					const std::string Name = S.substr(Start, P - Start);
					Space();
					if (P < S.size() && S[P] == '(')
					{
						++P;
						double Args[2] = { 0, 0 };
						int N = 0;
						Space();
						if (P < S.size() && S[P] != ')')
						{
							for (;;)
							{
								double A;
								if (!Expr(A)) { return false; }
								if (N < 2) { Args[N] = A; }
								++N;
								Space();
								if (P < S.size() && S[P] == ',') { ++P; continue; }
								break;
							}
						}
						if (P >= S.size() || S[P] != ')') { return Fail("missing ) after " + Name); }
						++P;
						const double Rad = 3.14159265358979323846 / 180.0;
						if (Name == "sqrt") { Out = std::sqrt(Args[0]); }
						else if (Name == "abs") { Out = std::fabs(Args[0]); }
						else if (Name == "sin") { Out = std::sin(Args[0] * Rad); }
						else if (Name == "cos") { Out = std::cos(Args[0] * Rad); }
						else if (Name == "tan") { Out = std::tan(Args[0] * Rad); }
						else if (Name == "min" && N == 2) { Out = Args[0] < Args[1] ? Args[0] : Args[1]; }
						else if (Name == "max" && N == 2) { Out = Args[0] > Args[1] ? Args[0] : Args[1]; }
						else { return Fail("unknown function " + Name); }
						return true;
					}
					if (Name == "pi") { Out = 3.14159265358979323846; return true; }
					if (!Vars || !Vars(Name, Out)) { return Fail("unknown name " + Name); }
					return true;
				}
				return Fail(std::string("unexpected '") + C + "'");
			}
		};
	}

	bool EvalExpr(const std::string& Expr, const FVarLookup& Vars, double& Out, std::string* OutError)
	{
		FEval E{ Expr, Vars };
		const bool bOk = E.Expr(Out);
		E.Space();
		if (bOk && E.P != Expr.size()) { E.Fail("unexpected text after the formula"); }
		if (!E.Error.empty()) { if (OutError) { *OutError = E.Error; } return false; }
		return true;
	}

	std::string FormatNumber(double Value, int Decimals)
	{
		if (Decimals < 0) { Decimals = 0; }
		if (Decimals > 6) { Decimals = 6; }
		char Buf[64];
		std::snprintf(Buf, sizeof(Buf), "%.*f", Decimals, Value);
		std::string S = Buf;
		if (S == "-0" || S.rfind("-0.", 0) == 0)
		{
			// "-0.0" reads oddly on a card.
			bool bZero = true;
			for (char C : S) { if (C >= '1' && C <= '9') { bZero = false; } }
			if (bZero) { S.erase(0, 1); }
		}
		return S;
	}

	std::string FormatTemplate(const std::string& T, const FVarLookup& Vars)
	{
		std::string Out;
		size_t P = 0;
		while (P < T.size())
		{
			const size_t Open = T.find('{', P);
			if (Open == std::string::npos) { Out.append(T, P, std::string::npos); break; }
			const size_t Close = T.find('}', Open);
			if (Close == std::string::npos) { Out.append(T, P, std::string::npos); break; }
			Out.append(T, P, Open - P);
			std::string Body = T.substr(Open + 1, Close - Open - 1);
			int Decimals = 1;
			const size_t Colon = Body.rfind(':');
			if (Colon != std::string::npos)
			{
				Decimals = std::atoi(Body.c_str() + Colon + 1);
				Body.erase(Colon);
			}
			double V = 0;
			bool bOk;
			if (!Body.empty() && Body[0] == '=') { bOk = EvalExpr(Body.substr(1), Vars, V); }
			else { bOk = Vars && Vars(Body, V); }
			Out += bOk ? FormatNumber(V, Decimals) : std::string("[?]");
			P = Close + 1;
		}
		return Out;
	}
}
