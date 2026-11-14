// Copyright Epic Games, Inc. All Rights Reserved.

#include "TSCColonyCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "TSCGameMode.h"

ATSCColonyCameraPawn::ATSCColonyCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true;
	SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f));
	SpringArm->TargetArmLength = ReferenceArmLength;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(28.f); // narrow lens far away: close to an isometric projection

	DesiredArmLength = ReferenceArmLength;
}

void ATSCColonyCameraPawn::SetView(const FVector& Loc, float ArmLength)
{
	SetActorLocation(Loc);
	DesiredArmLength = FMath::Clamp(ArmLength, MinArmLength, MaxArmLength);
	SpringArm->TargetArmLength = DesiredArmLength;
}

void ATSCColonyCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const ATSCGameMode* GM = GetWorld()->GetAuthGameMode<ATSCGameMode>();
	if (GM && GM->IsFirstCutFramed())
	{
		PendingPan = FVector2D::ZeroVector;
		PendingDrag = FVector2D::ZeroVector;
		PendingZoom = 0.f;
		PendingRotate = 0.f;
		return;
	}

	// Pan speed grows with zoom so the camera feels the same at every height.
	const float ZoomScale = SpringArm->TargetArmLength / FMath::Max(ReferenceArmLength, 1.f);
	const FRotator YawOnly(0.f, GetActorRotation().Yaw, 0.f);

	// Keyboard + optional edge scroll
	FVector2D Pan = PendingPan;
	if (bEdgeScrollEnabled)
	{
		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			float MouseX = 0.f, MouseY = 0.f;
			int32 SizeX = 0, SizeY = 0;
			PC->GetViewportSize(SizeX, SizeY);
			if (SizeX > 0 && SizeY > 0 && PC->GetMousePosition(MouseX, MouseY))
			{
				if (MouseX <= EdgeScrollMargin) { Pan.X -= 1.f; }
				if (MouseX >= SizeX - EdgeScrollMargin) { Pan.X += 1.f; }
				if (MouseY <= EdgeScrollMargin) { Pan.Y += 1.f; }
				if (MouseY >= SizeY - EdgeScrollMargin) { Pan.Y -= 1.f; }
			}
		}
	}
	Pan = Pan.GetSafeNormal() * FMath::Min(Pan.Size(), 1.f);
	if (!Pan.IsNearlyZero())
	{
		const FVector Local(Pan.Y, Pan.X, 0.f); // forward = +X, right = +Y
		AddActorWorldOffset(YawOnly.RotateVector(Local) * PanSpeed * ZoomScale * DeltaSeconds);
	}

	// Middle-mouse drag: move the camera opposite to the mouse so the ground follows the cursor.
	if (!PendingDrag.IsNearlyZero())
	{
		const FVector Local(-PendingDrag.Y, -PendingDrag.X, 0.f);
		AddActorWorldOffset(YawOnly.RotateVector(Local) * DragPanSpeed * ZoomScale);
	}

	// The view is a fixed isometric one (the tile constructions are drawn for it), so there is no rotation.
	// Keep the camera over the clearing.
	FVector Loc = GetActorLocation();
	Loc.X = FMath::Clamp(Loc.X, -3600.f, 3600.f);
	Loc.Y = FMath::Clamp(Loc.Y, -3200.f, 3400.f);
	SetActorLocation(Loc);

	// Zoom (smoothed)
	if (!FMath::IsNearlyZero(PendingZoom))
	{
		DesiredArmLength = FMath::Clamp(DesiredArmLength - PendingZoom * ZoomStep, MinArmLength, MaxArmLength);
	}
	SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, DesiredArmLength, DeltaSeconds, ZoomInterpSpeed);

	PendingPan = FVector2D::ZeroVector;
	PendingDrag = FVector2D::ZeroVector;
	PendingZoom = 0.f;
	PendingRotate = 0.f;
}
