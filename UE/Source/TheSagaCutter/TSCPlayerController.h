// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TSCPlayerController.generated.h"

class ATSCColonyCameraPawn;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Player controller with a free mouse cursor. Builds its Enhanced Input actions and mapping context in code,
 * so the project needs no input assets to start. Replace with data assets later if designers need to rebind keys.
 */
UCLASS()
class THESAGACUTTER_API ATSCPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATSCPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void OnMove(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);
	void OnRotate(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnDragStarted() { bDragging = true; }
	void OnDragCompleted() { bDragging = false; }

	ATSCColonyCameraPawn* GetCameraPawn() const;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> ColonyContext;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ZoomAction;

	UPROPERTY()
	TObjectPtr<UInputAction> RotateAction;

	UPROPERTY()
	TObjectPtr<UInputAction> DragAction;

	UPROPERTY()
	TObjectPtr<UInputAction> LookAction;

	bool bDragging = false;
};
