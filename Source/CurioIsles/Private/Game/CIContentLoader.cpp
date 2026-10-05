// CURIO ISLES: finds and loads island content packs from Content/Islands. (CLAUDE.md: Data-driven content)
#include "Game/CIContentLoader.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace CIContent
{
	FString IslandsRoot()
	{
		return FPaths::ProjectContentDir() / TEXT("Islands");
	}

	TArray<FString> FindIslands()
	{
		TArray<FString> Dirs;
		IFileManager::Get().FindFiles(Dirs, *(IslandsRoot() / TEXT("*")), false, true);
		TArray<FString> Out;
		for (const FString& D : Dirs)
		{
			if (IFileManager::Get().FileExists(*(IslandsRoot() / D / TEXT("island.json")))) { Out.Add(D); }
		}
		Out.Sort();
		return Out;
	}

	bool LoadIsland(const FString& Folder, CI::FIslandDef& Out, FString& OutError)
	{
		const FString Root = IslandsRoot();
		// Files are UTF-8; read raw bytes so the engine's text decoding never touches them.
		const CI::FContentReader Read = [&Root](const std::string& Rel, std::string& Text) -> bool
		{
			TArray<uint8> Bytes;
			if (!FFileHelper::LoadFileToArray(Bytes, *(Root / UTF8_TO_TCHAR(Rel.c_str())), FILEREAD_Silent)) { return false; }
			Text.assign((const char*)Bytes.GetData(), (size_t)Bytes.Num());
			return true;
		};
		std::string Err;
		if (!CI::LoadIsland(TCHAR_TO_UTF8(*Folder), Read, Out, Err))
		{
			OutError = UTF8_TO_TCHAR(Err.c_str());
			return false;
		}
		return true;
	}
}
