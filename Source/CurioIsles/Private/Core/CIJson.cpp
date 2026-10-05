// CURIO ISLES: a small JSON reader for level, part and island data. (CLAUDE.md: Data-driven content)
#include "CIJson.h"

#include <cstdlib>
#include <cstring>

namespace CI
{
	namespace
	{
		struct FParser
		{
			const char* P;
			const char* Begin;
			const char* End;
			std::string Error;

			int Line() const
			{
				int L = 1;
				for (const char* C = Begin; C < P && C < End; ++C) { if (*C == '\n') { ++L; } }
				return L;
			}

			bool Fail(const char* Msg)
			{
				if (Error.empty()) { Error = std::string(Msg) + " at line " + std::to_string(Line()); }
				return false;
			}

			void SkipSpace()
			{
				while (P < End)
				{
					if (*P == ' ' || *P == '\t' || *P == '\n' || *P == '\r') { ++P; }
					// Comments are allowed in our data files (level designers annotate them).
					else if (*P == '/' && P + 1 < End && P[1] == '/') { while (P < End && *P != '\n') { ++P; } }
					else { break; }
				}
			}

			bool Literal(const char* Word)
			{
				const size_t N = std::strlen(Word);
				if ((size_t)(End - P) >= N && std::strncmp(P, Word, N) == 0) { P += N; return true; }
				return false;
			}

			static void AppendUtf8(std::string& S, unsigned Code)
			{
				if (Code < 0x80) { S += (char)Code; }
				else if (Code < 0x800) { S += (char)(0xC0 | (Code >> 6)); S += (char)(0x80 | (Code & 0x3F)); }
				else if (Code < 0x10000) { S += (char)(0xE0 | (Code >> 12)); S += (char)(0x80 | ((Code >> 6) & 0x3F)); S += (char)(0x80 | (Code & 0x3F)); }
				else { S += (char)(0xF0 | (Code >> 18)); S += (char)(0x80 | ((Code >> 12) & 0x3F)); S += (char)(0x80 | ((Code >> 6) & 0x3F)); S += (char)(0x80 | (Code & 0x3F)); }
			}

			bool ParseHex4(unsigned& Out)
			{
				if (End - P < 4) { return Fail("bad \\u escape"); }
				Out = 0;
				for (int I = 0; I < 4; ++I)
				{
					const char C = *P++;
					Out <<= 4;
					if (C >= '0' && C <= '9') { Out |= (unsigned)(C - '0'); }
					else if (C >= 'a' && C <= 'f') { Out |= (unsigned)(C - 'a' + 10); }
					else if (C >= 'A' && C <= 'F') { Out |= (unsigned)(C - 'A' + 10); }
					else { return Fail("bad \\u escape"); }
				}
				return true;
			}

			bool ParseString(std::string& Out)
			{
				++P;   // opening quote
				while (P < End && *P != '"')
				{
					if (*P == '\\')
					{
						++P;
						if (P >= End) { break; }
						const char E = *P++;
						switch (E)
						{
						case '"': Out += '"'; break;
						case '\\': Out += '\\'; break;
						case '/': Out += '/'; break;
						case 'b': Out += '\b'; break;
						case 'f': Out += '\f'; break;
						case 'n': Out += '\n'; break;
						case 'r': Out += '\r'; break;
						case 't': Out += '\t'; break;
						case 'u':
						{
							unsigned Code;
							if (!ParseHex4(Code)) { return false; }
							if (Code >= 0xD800 && Code < 0xDC00 && End - P >= 6 && P[0] == '\\' && P[1] == 'u')
							{
								P += 2;
								unsigned Low;
								if (!ParseHex4(Low)) { return false; }
								Code = 0x10000 + ((Code - 0xD800) << 10) + (Low - 0xDC00);
							}
							AppendUtf8(Out, Code);
							break;
						}
						default: return Fail("bad escape");
						}
					}
					else { Out += *P++; }
				}
				if (P >= End) { return Fail("unterminated string"); }
				++P;
				return true;
			}

			bool ParseValue(FJson& Out, int Depth)
			{
				if (Depth > 64) { return Fail("nesting too deep"); }
				SkipSpace();
				if (P >= End) { return Fail("unexpected end"); }
				const char C = *P;
				if (C == '{')
				{
					Out.Type = FJson::EType::Object;
					++P;
					SkipSpace();
					if (P < End && *P == '}') { ++P; return true; }
					for (;;)
					{
						SkipSpace();
						if (P >= End || *P != '"') { return Fail("expected a \"key\""); }
						std::string Key;
						if (!ParseString(Key)) { return false; }
						SkipSpace();
						if (P >= End || *P != ':') { return Fail("expected ':'"); }
						++P;
						Out.Object.emplace_back(std::move(Key), FJson());
						if (!ParseValue(Out.Object.back().second, Depth + 1)) { return false; }
						SkipSpace();
						if (P < End && *P == ',')
						{
							++P;
							SkipSpace();
							if (P < End && *P == '}') { ++P; return true; }   // trailing comma
							continue;
						}
						if (P < End && *P == '}') { ++P; return true; }
						return Fail("expected ',' or '}'");
					}
				}
				if (C == '[')
				{
					Out.Type = FJson::EType::Array;
					++P;
					SkipSpace();
					if (P < End && *P == ']') { ++P; return true; }
					for (;;)
					{
						Out.Array.emplace_back();
						if (!ParseValue(Out.Array.back(), Depth + 1)) { return false; }
						SkipSpace();
						if (P < End && *P == ',')
						{
							++P;
							SkipSpace();
							if (P < End && *P == ']') { ++P; return true; }
							continue;
						}
						if (P < End && *P == ']') { ++P; return true; }
						return Fail("expected ',' or ']'");
					}
				}
				if (C == '"') { Out.Type = FJson::EType::String; return ParseString(Out.String); }
				if (Literal("true")) { Out.Type = FJson::EType::Bool; Out.Bool = true; return true; }
				if (Literal("false")) { Out.Type = FJson::EType::Bool; Out.Bool = false; return true; }
				if (Literal("null")) { Out.Type = FJson::EType::Null; return true; }
				if (C == '-' || (C >= '0' && C <= '9'))
				{
					const char* S = P;
					while (P < End && (*P == '-' || *P == '+' || *P == '.' || *P == 'e' || *P == 'E' || (*P >= '0' && *P <= '9'))) { ++P; }
					const std::string Token(S, P);
					char* EndPtr = nullptr;
					// strtod is correctly rounded (and the "C" locale is used by the game), so every platform reads the same double.
					Out.Number = std::strtod(Token.c_str(), &EndPtr);
					if (!EndPtr || *EndPtr != '\0') { return Fail("bad number"); }
					Out.Type = FJson::EType::Number;
					return true;
				}
				return Fail("unexpected character");
			}
		};
	}

	bool FJson::Parse(const std::string& Text, FJson& Out, std::string& OutError)
	{
		Out = FJson();
		FParser Parser{ Text.data(), Text.data(), Text.data() + Text.size(), std::string() };
		// Skip a UTF-8 byte order mark.
		if (Text.size() >= 3 && (unsigned char)Text[0] == 0xEF && (unsigned char)Text[1] == 0xBB && (unsigned char)Text[2] == 0xBF) { Parser.P += 3; }
		if (!Parser.ParseValue(Out, 0)) { OutError = Parser.Error; return false; }
		Parser.SkipSpace();
		if (Parser.P != Parser.End) { Parser.Fail("trailing characters"); OutError = Parser.Error; return false; }
		return true;
	}

	const FJson& FJson::Null()
	{
		static const FJson N;
		return N;
	}

	const FJson& FJson::operator[](const char* Key) const
	{
		if (Type == EType::Object)
		{
			for (const auto& KV : Object) { if (KV.first == Key) { return KV.second; } }
		}
		return Null();
	}

	bool FJson::Has(const char* Key) const
	{
		return !(*this)[Key].IsNull();
	}

	double FJson::Num(const char* Key, double Default) const
	{
		const FJson& V = (*this)[Key];
		return V.Type == EType::Number ? V.Number : Default;
	}

	int FJson::Int(const char* Key, int Default) const
	{
		const FJson& V = (*this)[Key];
		return V.Type == EType::Number ? (int)V.Number : Default;
	}

	bool FJson::Flag(const char* Key, bool Default) const
	{
		const FJson& V = (*this)[Key];
		return V.Type == EType::Bool ? V.Bool : Default;
	}

	std::string FJson::Str(const char* Key, const std::string& Default) const
	{
		const FJson& V = (*this)[Key];
		return V.Type == EType::String ? V.String : Default;
	}

	std::vector<std::string> FJson::Strings(const char* Key) const
	{
		std::vector<std::string> Out;
		const FJson& V = (*this)[Key];
		if (V.Type == EType::String) { Out.push_back(V.String); }
		for (const FJson& E : V.Array) { if (E.Type == EType::String) { Out.push_back(E.String); } }
		return Out;
	}
}
