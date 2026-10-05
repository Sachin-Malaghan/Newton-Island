// CURIO ISLES: the content model - islands, worlds, parts and levels, all read from JSON. (CLAUDE.md: Data-driven content)
#include "CIContent.h"

#include <cstdlib>

namespace CI
{
	// ---------------------------------------------------------------- params

	double FParamDef::Snap(double V) const
	{
		const int N = NumValues();
		const double F = Step > 0 ? (V - Min) / Step : 0;
		int I = (int)std::floor(F + 0.5);
		if (I < 0) { I = 0; }
		if (I > N - 1) { I = N - 1; }
		return ValueAt(I);
	}

	int FParamDef::NumValues() const
	{
		if (Step <= 0 || Max <= Min) { return 1; }
		return (int)std::floor((Max - Min) / Step + 1e-9) + 1;
	}

	double FParamDef::ValueAt(int Index) const
	{
		// Index * Step from Min, so every platform gets the same double for the same slider notch.
		const double V = Min + (double)Index * Step;
		return V > Max ? Max : V;
	}

	int FPartDef::ParamIndex(const std::string& ParamId) const
	{
		for (int I = 0; I < (int)Params.size(); ++I) { if (Params[I].Id == ParamId) { return I; } }
		return -1;
	}

	const FPartDef* FPartCatalog::Find(const std::string& Id) const
	{
		for (const FPartDef& P : Parts) { if (P.Id == Id) { return &P; } }
		return nullptr;
	}

	bool FSlotDef::AcceptsPart(const std::string& PartId) const
	{
		if (Accepts.empty()) { return true; }
		for (const std::string& A : Accepts) { if (A == PartId) { return true; } }
		return false;
	}

	int FLevelDef::FindSlot(const std::string& SlotId) const
	{
		for (int I = 0; I < (int)Slots.size(); ++I) { if (Slots[I].Id == SlotId) { return I; } }
		return -1;
	}

	int FLevelDef::FindZone(const std::string& ZoneId) const
	{
		for (int I = 0; I < (int)Zones.size(); ++I) { if (Zones[I].Id == ZoneId) { return I; } }
		return -1;
	}

	int FLevelDef::FindBody(const std::string& BodyId) const
	{
		for (int I = 0; I < (int)Bodies.size(); ++I) { if (Bodies[I].Id == BodyId) { return I; } }
		return -1;
	}

	// ---------------------------------------------------------------- parsing helpers

	namespace
	{
		bool ReadVec(const FJson& J, FVec2& Out)
		{
			if (J.IsArray() && J.Array.size() >= 2 && J.Array[0].IsNumber() && J.Array[1].IsNumber())
			{
				Out = FVec2(J.Array[0].Number, J.Array[1].Number);
				return true;
			}
			if (J.IsObject() && J.Has("x") && J.Has("y"))
			{
				Out = FVec2(J.Num("x"), J.Num("y"));
				return true;
			}
			return false;
		}

		// [x0, y0, x1, y1]
		bool ReadRect(const FJson& J, FVec2& Min, FVec2& Max)
		{
			if (!J.IsArray() || J.Array.size() < 4) { return false; }
			Min = FVec2(CI::Min(J.Array[0].Number, J.Array[2].Number), CI::Min(J.Array[1].Number, J.Array[3].Number));
			Max = FVec2(CI::Max(J.Array[0].Number, J.Array[2].Number), CI::Max(J.Array[1].Number, J.Array[3].Number));
			return true;
		}

		unsigned ReadColor(const FJson& J, const char* Key, unsigned Default)
		{
			const std::string S = J.Str(Key);
			if (S.size() == 7 && S[0] == '#') { return (unsigned)std::strtoul(S.c_str() + 1, nullptr, 16); }
			return Default;
		}

		void ReadParam(const FJson& J, FParamDef& P)
		{
			if (J.Has("id")) { P.Id = J.Str("id"); }
			P.Label = J.Str("label", P.Label.empty() ? P.Id : P.Label);
			P.Symbol = J.Str("symbol", P.Symbol);
			P.Unit = J.Str("unit", P.Unit);
			P.Less = J.Str("less", P.Less.empty() ? "less" : P.Less);
			P.More = J.Str("more", P.More.empty() ? "more" : P.More);
			P.Min = J.Num("min", P.Min);
			P.Max = J.Num("max", P.Max);
			P.Step = J.Num("step", P.Step);
			P.Default = J.Num("default", P.Default);
			P.Decimals = J.Int("decimals", P.Decimals);
		}

		// Catalog params with a level's overrides: { "height": { "min": 0.2, "max": 2 } }.
		bool ResolveParams(const FPartDef& Part, const FJson& Overrides, std::vector<FParamDef>& Out, std::string& Err)
		{
			Out = Part.Params;
			if (Overrides.IsObject())
			{
				for (const auto& KV : Overrides.Object)
				{
					const int I = Part.ParamIndex(KV.first);
					if (I < 0) { Err = "part '" + Part.Id + "' has no slider '" + KV.first + "'"; return false; }
					ReadParam(KV.second, Out[I]);
				}
			}
			for (FParamDef& P : Out) { P.Default = P.Snap(P.Default); }
			return true;
		}

		bool ParseBehavior(const std::string& S, EPartBehavior& Out)
		{
			if (S == "ramp") { Out = EPartBehavior::Ramp; return true; }
			if (S == "launcher") { Out = EPartBehavior::Launcher; return true; }
			if (S == "brake") { Out = EPartBehavior::Brake; return true; }
			return false;
		}
	}

	bool ParseCatalog(const FJson& J, FPartCatalog& Out, std::string& Err)
	{
		for (const FJson& PJ : J["parts"].Array)
		{
			FPartDef P;
			P.Id = PJ.Str("id");
			P.Name = PJ.Str("name", P.Id);
			P.Blurb = PJ.Str("blurb");
			P.Look = PJ.Str("look", P.Id);
			if (!ParseBehavior(PJ.Str("behavior", P.Id), P.Behavior)) { Err = "part '" + P.Id + "': unknown behavior '" + PJ.Str("behavior") + "'"; return false; }
			for (const FJson& Q : PJ["params"].Array)
			{
				FParamDef D;
				ReadParam(Q, D);
				if (D.Id.empty() || D.Max < D.Min || D.Step <= 0) { Err = "part '" + P.Id + "': bad slider '" + D.Id + "'"; return false; }
				P.Params.push_back(D);
			}
			P.Props = PJ["props"];
			Out.Parts.push_back(std::move(P));
		}
		if (Out.Parts.empty()) { Err = "no parts"; return false; }
		return true;
	}

	bool ParseLevel(const FJson& J, const FPartCatalog& Catalog, FLevelDef& L, std::string& Err)
	{
		L.Id = J.Str("id");
		L.Name = J.Str("name", L.Id);
		L.Goal = J.Str("goal");
		L.Concept = J.Str("concept");
		L.Syllabus = J.Strings("syllabus");
		L.Why = J.Str("why");
		L.WhyStudent = J.Str("whyStudent", L.Why);
		L.Formula = J.Str("formula");
		L.Example = J.Str("example");
		L.StopText = J.Str("stopText", L.StopText);
		L.Gravity = J.Num("gravity", L.Gravity);
		L.TimeLimit = J.Num("timeLimit", L.TimeLimit);
		L.PreviewSeconds = J.Num("previewSeconds", L.PreviewSeconds);
		L.Par = J.Int("par", L.Par);
		if (!ReadRect(J["view"], L.ViewMin, L.ViewMax)) { Err = "missing \"view\": [x0, y0, x1, y1]"; return false; }
		if (!ReadRect(J["bounds"], L.BoundsMin, L.BoundsMax))
		{
			const FVec2 Pad(4, 4);
			L.BoundsMin = L.ViewMin - Pad;
			L.BoundsMax = L.ViewMax + FVec2(4, 30);
		}

		for (const FJson& S : J["surfaces"].Array)
		{
			FSurfaceDef D;
			for (const FJson& P : S["points"].Array)
			{
				FVec2 V;
				if (!ReadVec(P, V)) { Err = "surface point must be [x, y]"; return false; }
				D.Points.push_back(V);
			}
			if (D.Points.size() < 2) { Err = "a surface needs at least two points"; return false; }
			D.Friction = S.Num("friction", D.Friction);
			D.Restitution = S.Num("restitution", D.Restitution);
			D.Look = S.Str("look", D.Look);
			L.Surfaces.push_back(std::move(D));
		}

		for (const FJson& Z : J["zones"].Array)
		{
			FZoneDef D;
			D.Id = Z.Str("id");
			D.Look = Z.Str("look", D.Id);
			if (!ReadRect(Z["rect"], D.Min, D.Max)) { Err = "zone '" + D.Id + "' needs \"rect\""; return false; }
			D.Level = Z.Num("level", D.Max.Y);
			L.Zones.push_back(D);
		}

		for (const FJson& B : J["bodies"].Array)
		{
			FBodyDef D;
			D.Id = B.Str("id");
			const std::string Kind = B.Str("kind", "ball");
			D.Kind = Kind == "cart" ? EBodyKind::Cart : EBodyKind::Ball;
			D.Look = B.Str("look", Kind);
			D.Radius = B.Num("radius", D.Kind == EBodyKind::Cart ? 0.3 : 0.15);
			D.Mass = B.Num("mass", 1);
			ReadVec(B["pos"], D.Pos);
			ReadVec(B["vel"], D.Vel);
			const FJson& On = B["on"];
			D.OnPart = On.Str("part");
			D.Anchor = On.Str("anchor");
			L.Bodies.push_back(D);
		}
		if (L.Bodies.empty()) { Err = "a level needs at least one body"; return false; }

		for (const FJson& S : J["slots"].Array)
		{
			FSlotDef D;
			D.Id = S.Str("id");
			if (!ReadVec(S["pos"], D.Pos)) { Err = "slot '" + D.Id + "' needs \"pos\""; return false; }
			D.Accepts = S.Strings("accepts");
			D.Facing = S.Num("facing", 1) < 0 ? -1 : 1;
			L.Slots.push_back(D);
		}

		for (const FJson& T : J["tray"].Array)
		{
			FTrayItem D;
			D.Part = T.Str("part");
			D.Count = T.Int("count", 1);
			const FPartDef* P = Catalog.Find(D.Part);
			if (!P) { Err = "tray: unknown part '" + D.Part + "'"; return false; }
			if (!ResolveParams(*P, T["params"], D.Params, Err)) { return false; }
			L.Tray.push_back(D);
		}

		for (const FJson& F : J["fixed"].Array)
		{
			FFixedPart D;
			D.Part = F.Str("part");
			D.Slot = F.Str("slot");
			D.bTunable = F.Flag("tunable");
			const FPartDef* P = Catalog.Find(D.Part);
			if (!P) { Err = "fixed: unknown part '" + D.Part + "'"; return false; }
			if (L.FindSlot(D.Slot) < 0) { Err = "fixed part '" + D.Part + "': unknown slot '" + D.Slot + "'"; return false; }
			if (!ResolveParams(*P, F["params"], D.Params, Err)) { return false; }
			for (const FParamDef& Q : D.Params) { D.Values.push_back(Q.Default); }
			L.Fixed.push_back(D);
		}

		for (const FJson& G : J["goals"].Array)
		{
			FGoalDef D;
			const std::string Type = G.Str("type", "enter");
			D.Type = Type == "rest" ? EGoalType::Rest : EGoalType::Enter;
			D.Body = G.Str("body");
			D.Zone = G.Str("zone");
			if (L.FindBody(D.Body) < 0 || L.FindZone(D.Zone) < 0) { Err = "goal refers to unknown body '" + D.Body + "' or zone '" + D.Zone + "'"; return false; }
			L.Goals.push_back(D);
		}
		if (L.Goals.empty()) { Err = "a level needs a goal"; return false; }

		for (const FJson& F : J["fails"].Array)
		{
			FFailDef D;
			D.Body = F.Str("body");
			D.Zone = F.Str("zone");
			D.Say = F.Str("say", "Oops!");
			if (L.FindBody(D.Body) < 0 || L.FindZone(D.Zone) < 0) { Err = "fail refers to unknown body '" + D.Body + "' or zone '" + D.Zone + "'"; return false; }
			L.Fails.push_back(D);
		}

		const FJson& Pr = J["predict"];
		L.Predict.Question = Pr.Str("question");
		L.Predict.Measure = Pr.Str("measure");
		L.Predict.Choices = Pr.Strings("choices");
		return true;
	}

	bool LoadIsland(const std::string& Folder, const FContentReader& Read, FIslandDef& Out, std::string& Err)
	{
		auto ReadJson = [&](const std::string& Rel, FJson& J) -> bool
		{
			std::string Text, ParseErr;
			if (!Read(Folder + "/" + Rel, Text)) { Err = Folder + "/" + Rel + ": file not found"; return false; }
			if (!FJson::Parse(Text, J, ParseErr)) { Err = Folder + "/" + Rel + ": " + ParseErr; return false; }
			return true;
		};

		FJson Manifest;
		if (!ReadJson("island.json", Manifest)) { return false; }
		Out.Folder = Folder;
		Out.Id = Manifest.Str("id", Folder);
		Out.Name = Manifest.Str("name", Out.Id);
		Out.Subject = Manifest.Str("subject");
		Out.Mentor = Manifest.Str("mentor");

		FJson CatalogJson;
		if (!ReadJson(Manifest.Str("parts", "parts.json"), CatalogJson)) { return false; }
		std::string CatErr;
		if (!ParseCatalog(CatalogJson, Out.Catalog, CatErr)) { Err = Folder + "/parts.json: " + CatErr; return false; }

		int Number = 0;
		for (const FJson& W : Manifest["worlds"].Array)
		{
			FWorldDef World;
			World.Id = W.Str("id");
			World.Name = W.Str("name", World.Id);
			World.Teaches = W.Str("teaches");
			World.Number = W.Int("number", ++Number);
			Number = World.Number;
			const FJson& T = W["theme"];
			World.Theme.SkyTop = ReadColor(T, "skyTop", World.Theme.SkyTop);
			World.Theme.SkyBottom = ReadColor(T, "skyBottom", World.Theme.SkyBottom);
			World.Theme.Hills[0] = ReadColor(T, "hillFar", World.Theme.Hills[0]);
			World.Theme.Hills[1] = ReadColor(T, "hillMid", World.Theme.Hills[1]);
			World.Theme.Hills[2] = ReadColor(T, "hillNear", World.Theme.Hills[2]);
			World.Theme.Ground = ReadColor(T, "ground", World.Theme.Ground);
			World.Theme.GroundDark = ReadColor(T, "groundDark", World.Theme.GroundDark);
			World.Theme.Accent = ReadColor(T, "accent", World.Theme.Accent);
			World.Theme.Sun = ReadColor(T, "sun", World.Theme.Sun);

			for (const std::string& File : W.Strings("levels"))
			{
				FJson LJ;
				if (!ReadJson(File, LJ)) { return false; }
				FLevelDef Level;
				std::string LevelErr;
				if (!ParseLevel(LJ, Out.Catalog, Level, LevelErr)) { Err = Folder + "/" + File + ": " + LevelErr; return false; }
				Level.File = File;
				World.Levels.push_back(std::move(Level));
			}
			Out.Worlds.push_back(std::move(World));
		}
		return true;
	}
}
