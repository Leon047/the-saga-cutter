// Copyright Epic Games, Inc. All Rights Reserved.

#include "TSCGameMode.h"

#include "TSCColonyCameraPawn.h"
#include "TSCPlayerController.h"

ATSCGameMode::ATSCGameMode()
{
	DefaultPawnClass = ATSCColonyCameraPawn::StaticClass();
	PlayerControllerClass = ATSCPlayerController::StaticClass();
}
