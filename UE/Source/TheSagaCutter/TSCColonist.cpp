// Copyright Epic Games, Inc. All Rights Reserved.

#include "TSCColonist.h"

#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"

ATSCColonist::ATSCColonist()
{
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
}
