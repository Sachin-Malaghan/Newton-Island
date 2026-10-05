// CURIO ISLES: a small JSON reader for level, part and island data. (CLAUDE.md: Data-driven content)
// Engine-agnostic so the harness and the game read exactly the same files the same way.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace CI
{
	class FJson
	{
	public:
		enum class EType { Null, Bool, Number, String, Array, Object };

		EType Type = EType::Null;
		bool Bool = false;
		double Number = 0;
		std::string String;
		std::vector<FJson> Array;
		std::vector<std::pair<std::string, FJson>> Object;

		// Parses Text; on failure returns false and describes the problem (with line number) in OutError.
		static bool Parse(const std::string& Text, FJson& Out, std::string& OutError);

		bool IsNull() const { return Type == EType::Null; }
		bool IsArray() const { return Type == EType::Array; }
		bool IsObject() const { return Type == EType::Object; }
		bool IsNumber() const { return Type == EType::Number; }
		bool IsString() const { return Type == EType::String; }

		// Object member or a shared null value.
		const FJson& operator[](const char* Key) const;
		bool Has(const char* Key) const;

		double Num(const char* Key, double Default = 0) const;
		int Int(const char* Key, int Default = 0) const;
		bool Flag(const char* Key, bool Default = false) const;
		std::string Str(const char* Key, const std::string& Default = std::string()) const;
		std::vector<std::string> Strings(const char* Key) const;

		static const FJson& Null();
	};
}
