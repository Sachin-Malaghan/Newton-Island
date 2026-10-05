// CURIO ISLES: tiny formula evaluator for worked examples and prediction answers. (CLAUDE.md: Learning features)
//
// Templates (level JSON "example", "why"...):  "v = √(2 × {g} × {height:2}) = {=sqrt(2*g*height):1} m/s"
//   {name}      a variable (slider id "height", "ramp.height", "g", sim measures "ball.speed", "ball.landX"...)
//   {name:2}    ... with 2 decimals (default 1)
//   {=expr:1}   an expression: + - * / ^, parentheses, sqrt abs min max, sin cos tan (degrees)
// Unknown names render as "[?]", so a typo shows up on screen instead of crashing.
#pragma once

#include <functional>
#include <string>

namespace CI
{
	using FVarLookup = std::function<bool(const std::string& Name, double& Out)>;

	bool EvalExpr(const std::string& Expr, const FVarLookup& Vars, double& Out, std::string* OutError = nullptr);
	std::string FormatTemplate(const std::string& Template, const FVarLookup& Vars);
	std::string FormatNumber(double Value, int Decimals);
}
