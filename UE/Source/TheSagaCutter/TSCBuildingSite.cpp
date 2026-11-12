// Copyright Epic Games, Inc. All Rights Reserved.

#include "TSCBuildingSite.h"

#include "Components/StaticMeshComponent.h"

ATSCBuildingSite::ATSCBuildingSite()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

bool ATSCBuildingSite::DeliverResource(int32 Amount)
{
	DeliveredAmount = FMath::Clamp(DeliveredAmount + FMath::Max(Amount, 0), 0, RequiredAmount);
	return DeliveredAmount >= RequiredAmount;
}
