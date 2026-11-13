// Copyright Epic Games, Inc. All Rights Reserved.

#include "TSCLoc.h"
#include "TheSagaCutter.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"
#include "Misc/StringBuilder.h"

namespace
{
	struct FCSMapKeyFuncs : BaseKeyFuncs<TPair<FString, FString>, FString, false>
	{
		static const FString& GetSetKey(const TPair<FString, FString>& Element) { return Element.Key; }
		static bool Matches(const FString& A, const FString& B) { return A.Equals(B, ESearchCase::CaseSensitive); }
		static uint32 GetKeyHash(const FString& Key) { return FCrc::StrCrc32(*Key); }
	};

	struct FCSSetKeyFuncs : BaseKeyFuncs<FString, FString, false>
	{
		static const FString& GetSetKey(const FString& Element) { return Element; }
		static bool Matches(const FString& A, const FString& B) { return A.Equals(B, ESearchCase::CaseSensitive); }
		static uint32 GetKeyHash(const FString& Key) { return FCrc::StrCrc32(*Key); }
	};

	TMap<FString, FString, FDefaultSetAllocator, FCSMapKeyFuncs> GTable;
	TSet<FString, FCSSetKeyFuncs> GMisses;
	bool GLoaded = false;
	bool GRussian = false;

	void EnsureLoaded()
	{
		if (GLoaded)
		{
			return;
		}
		GLoaded = true;
		const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("DevLoc/ru.tsv"));
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
		{
			return;
		}
		for (FString& Line : Lines)
		{
			if (Line.Len() > 0 && Line[0] == 0xFEFF)
			{
				Line.RightChopInline(1);
			}
			// Keep spaces: keys and values can start or end with them. A line without a tab is a comment.
			// "#" cannot mark comments because number keys start with it ("# CUTS LEFT").
			while (Line.Len() > 0 && (Line[Line.Len() - 1] == TEXT('\r') || Line[Line.Len() - 1] == TEXT('\n')))
			{
				Line.LeftChopInline(1);
			}
			int32 Tab = INDEX_NONE;
			if (!Line.FindChar(TEXT('\t'), Tab) || Tab <= 0)
			{
				continue;
			}
			GTable.Add(Line.Left(Tab), Line.Mid(Tab + 1));
		}
		if (GTable.Num() > 0)
		{
			bool bStored = false;
			if (GConfig)
			{
				GConfig->GetBool(TEXT("TSC"), TEXT("bRussian"), bStored, GGameUserSettingsIni);
			}
			GRussian = bStored;
		}
		UE_LOG(LogSaga, Log, TEXT("TSCLoc: loaded %d entries from %s (Russian=%d)"), GTable.Num(), *Path, GRussian ? 1 : 0);
	}

	// True for English text worth reporting as untranslated. Strings that already hold Cyrillic were
	// translated when they were composed and reach Tr() a second time at draw; they are not misses.
	bool HasLetter(const FString& S)
	{
		bool bAlpha = false;
		for (TCHAR C : S)
		{
			if (C >= 0x0400 && C <= 0x04FF)
			{
				return false;
			}
			bAlpha |= FChar::IsAlpha(C);
		}
		return bAlpha;
	}
}

namespace TSCLoc
{
	bool IsAvailable()
	{
		EnsureLoaded();
		return GTable.Num() > 0;
	}

	bool IsRussian()
	{
		return IsAvailable() && GRussian;
	}

	void SetRussian(bool bOn)
	{
		if (!IsAvailable())
		{
			return;
		}
		GRussian = bOn;
		if (GConfig)
		{
			GConfig->SetBool(TEXT("TSC"), TEXT("bRussian"), bOn, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	void Toggle()
	{
		SetRussian(!IsRussian());
	}

	FString Tr(const FString& En)
	{
		if (En.IsEmpty() || !IsRussian())
		{
			return En;
		}
		if (const FString* Hit = GTable.Find(En))
		{
			return *Hit;
		}

		// Normalize digit runs to '#'.
		FString Norm;
		Norm.Reserve(En.Len());
		FStringFormatOrderedArguments Args;
		const int32 Len = En.Len();
		for (int32 i = 0; i < Len;)
		{
			if (En[i] >= TEXT('0') && En[i] <= TEXT('9'))
			{
				int32 j = i;
				while (j < Len && En[j] >= TEXT('0') && En[j] <= TEXT('9'))
				{
					++j;
				}
				Args.Add(FStringFormatArg(En.Mid(i, j - i)));
				Norm.AppendChar(TEXT('#'));
				i = j;
			}
			else
			{
				Norm.AppendChar(En[i]);
				++i;
			}
		}

		if (Args.Num() > 0 && Norm != En)
		{
			if (const FString* Hit = GTable.Find(Norm))
			{
				return FString::Format(**Hit, Args);
			}
		}

		const FString& MissKey = (Args.Num() > 0) ? Norm : En;
		if (HasLetter(MissKey))
		{
			bool bKnown = false;
			GMisses.Add(MissKey, &bKnown);
			if (!bKnown)
			{
				UE_LOG(LogSaga, Display, TEXT("[TSCLoc] missing: %s"), *MissKey);
			}
		}
		return En;
	}

	FString Fmt(const TCHAR* EnPattern, const FStringFormatOrderedArguments& Args)
	{
		return FString::Format(*Tr(FString(EnPattern)), Args);
	}

	void DumpMisses()
	{
		TArray<FString> Sorted = GMisses.Array();
		Sorted.Sort();
		const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DevLoc_missing.txt"));
		FFileHelper::SaveStringArrayToFile(Sorted, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogSaga, Log, TEXT("TSCLoc: %d missing strings written to %s"), Sorted.Num(), *Path);
	}
}
