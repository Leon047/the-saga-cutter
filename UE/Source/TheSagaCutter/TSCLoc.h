// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Dev-only runtime translation table (English -> Russian). Inert when UE/DevLoc/ru.tsv is absent. */
namespace TSCLoc
{
	bool IsAvailable();                 // table file was found and has >0 entries (lazy-loads on first call)
	bool IsRussian();                   // current language
	void SetRussian(bool bOn);          // no-op if !IsAvailable(); persists the choice
	void Toggle();
	FString Tr(const FString& En);      // returns En unchanged when !IsRussian() or on a miss
	FString Fmt(const TCHAR* EnPattern, const FStringFormatOrderedArguments& Args); // Tr(pattern) then FString::Format
	void DumpMisses();                  // writes <ProjectSavedDir>/DevLoc_missing.txt, logs the path to LogSaga
}
