// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TSCGameMode.generated.h"

/** Default game mode: RTS camera pawn + mouse-driven player controller. Derive a Blueprint (BP_TSCGameMode) to tweak. */
UCLASS()
class THESAGACUTTER_API ATSCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATSCGameMode();
};
