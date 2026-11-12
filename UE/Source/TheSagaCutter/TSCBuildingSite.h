// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TSCBuildingSite.generated.h"

class UStaticMeshComponent;

/** Placeholder construction site: needs some amount of one resource, then becomes a building. */
UCLASS()
class THESAGACUTTER_API ATSCBuildingSite : public AActor
{
	GENERATED_BODY()

public:
	ATSCBuildingSite();

	/** Resource tag under Colony.Resource.* that haulers must deliver. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colony")
	FGameplayTag RequiredResource;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colony", meta = (ClampMin = "0"))
	int32 RequiredAmount = 10;

	UPROPERTY(BlueprintReadOnly, Category = "Colony")
	int32 DeliveredAmount = 0;

	/** Returns true when the site is fully supplied after this delivery. */
	UFUNCTION(BlueprintCallable, Category = "Colony")
	bool DeliverResource(int32 Amount);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Colony")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
