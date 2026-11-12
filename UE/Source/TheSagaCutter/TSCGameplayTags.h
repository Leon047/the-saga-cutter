// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/** Native gameplay tags for the colony sim. Add new jobs/resources here and use them from C++ and Blueprints. */
namespace TSCTags
{
	// Jobs a colonist can be assigned
	THESAGACUTTER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Colony_Job_Chop);
	THESAGACUTTER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Colony_Job_Mine);
	THESAGACUTTER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Colony_Job_Haul);
	THESAGACUTTER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Colony_Job_Build);
	THESAGACUTTER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Colony_Job_Farm);
	THESAGACUTTER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Colony_Job_Fish);

	// Stockpile resources
	THESAGACUTTER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Colony_Resource_Wood);
	THESAGACUTTER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Colony_Resource_Stone);
	THESAGACUTTER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Colony_Resource_Iron);
	THESAGACUTTER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Colony_Resource_Food);
	THESAGACUTTER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Colony_Resource_Mead);
}
