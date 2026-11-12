// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "TSCColonist.generated.h"

/**
 * Placeholder settler. Walks via AAIController + navmesh. Hook a StateTree (see StateTreeAIComponent) or
 * SmartObject claims onto a Blueprint child (BP_Colonist) to implement the job loop.
 */
UCLASS()
class THESAGACUTTER_API ATSCColonist : public ACharacter
{
	GENERATED_BODY()

public:
	ATSCColonist();

	/** Display name shown in UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colony")
	FText ColonistName;

	/** Current job, a tag under Colony.Job.* (empty = idle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colony")
	FGameplayTag CurrentJob;

	/** Jobs this colonist is allowed to take, ordered by priority. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colony")
	FGameplayTagContainer AllowedJobs;
};
