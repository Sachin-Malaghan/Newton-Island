// CURIO ISLES: finds and loads island content packs from Content/Islands. (CLAUDE.md: Data-driven content)
#pragma once

#include "CoreMinimal.h"
#include "Core/CIContent.h"

namespace CIContent
{
	// Folders under Content/Islands that contain an island.json.
	TArray<FString> FindIslands();
	// Loads one island by folder name; OutError names the file and line on failure.
	bool LoadIsland(const FString& Folder, CI::FIslandDef& Out, FString& OutError);
	FString IslandsRoot();
}
