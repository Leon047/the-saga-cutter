// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TSCColonyCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * RTS-style colony camera: pan (WASD / edge / middle-mouse drag), zoom (mouse wheel), rotate (Q/E).
 * Input is fed in by ATSCPlayerController; this pawn only accumulates it and applies it in Tick.
 */
UCLASS()
class THESAGACUTTER_API ATSCColonyCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ATSCColonyCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** X = right, Y = forward, each -1..1. Called every frame while the keys are held. */
	void AddPanInput(const FVector2D& Input) { PendingPan += Input; }
	/** Mouse wheel delta; positive zooms in. */
	void AddZoomInput(float Axis) { PendingZoom += Axis; }
	/** -1..1, positive rotates clockwise seen from above. */
	void AddRotateInput(float Axis) { PendingRotate += Axis; }
	/** Raw mouse delta while middle-mouse dragging ("grab the ground"). */
	void AddDragInput(const FVector2D& MouseDelta) { PendingDrag += MouseDelta; }
	/** Scripted camera (title prologue): place the focus point and set the zoom at once. */
	void SetView(const FVector& Loc, float ArmLength);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Pan speed in uu/s at the reference arm length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pan")
	float PanSpeed = 2600.f;

	/** World units per raw mouse unit while dragging, at the reference arm length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pan")
	float DragPanSpeed = 8.f;

	/** Pan with the cursor near the screen edge. Off by default so PIE does not scroll away unexpectedly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pan")
	bool bEdgeScrollEnabled = false;

	/** Edge scroll zone width in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pan", meta = (ClampMin = "1"))
	float EdgeScrollMargin = 24.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ReferenceArmLength = 16500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float MinArmLength = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float MaxArmLength = 20000.f;

	/** Arm length change per wheel notch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomStep = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomInterpSpeed = 8.f;

	/** Degrees per second at full Q/E input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Rotate")
	float RotateSpeed = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraPitch = -36.f;

private:
	FVector2D PendingPan = FVector2D::ZeroVector;
	FVector2D PendingDrag = FVector2D::ZeroVector;
	float PendingZoom = 0.f;
	float PendingRotate = 0.f;
	float DesiredArmLength = 0.f;
};
