// Copyright Epic Games, Inc. All Rights Reserved.

#include "TSCPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "TSCColonyCameraPawn.h"
#include "TheSagaCutter.h"

ATSCPlayerController::ATSCPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ATSCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Keep the cursor free and visible: a colony sim is mouse-driven.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}

ATSCColonyCameraPawn* ATSCPlayerController::GetCameraPawn() const
{
	return Cast<ATSCColonyCameraPawn>(GetPawn());
}

void ATSCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// --- Actions ---
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	ZoomAction = MakeAction(TEXT("IA_Zoom"), EInputActionValueType::Axis1D);
	RotateAction = MakeAction(TEXT("IA_Rotate"), EInputActionValueType::Axis1D);
	DragAction = MakeAction(TEXT("IA_Drag"), EInputActionValueType::Boolean);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);

	// --- Mapping context ---
	ColonyContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Colony"));

	auto Swizzle = [this]()
	{
		UInputModifierSwizzleAxis* M = NewObject<UInputModifierSwizzleAxis>(this);
		M->Order = EInputAxisSwizzle::YXZ; // key's X value -> Y axis
		return M;
	};
	auto Negate = [this]() { return NewObject<UInputModifierNegate>(this); };

	// Move: X = right, Y = forward
	ColonyContext->MapKey(MoveAction, EKeys::D);
	ColonyContext->MapKey(MoveAction, EKeys::A).Modifiers.Add(Negate());
	ColonyContext->MapKey(MoveAction, EKeys::W).Modifiers.Add(Swizzle());
	{
		FEnhancedActionKeyMapping& S = ColonyContext->MapKey(MoveAction, EKeys::S);
		S.Modifiers.Add(Swizzle());
		S.Modifiers.Add(Negate());
	}
	// Arrow keys do the same
	ColonyContext->MapKey(MoveAction, EKeys::Right);
	ColonyContext->MapKey(MoveAction, EKeys::Left).Modifiers.Add(Negate());
	ColonyContext->MapKey(MoveAction, EKeys::Up).Modifiers.Add(Swizzle());
	{
		FEnhancedActionKeyMapping& Down = ColonyContext->MapKey(MoveAction, EKeys::Down);
		Down.Modifiers.Add(Swizzle());
		Down.Modifiers.Add(Negate());
	}

	ColonyContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);
	ColonyContext->MapKey(RotateAction, EKeys::E);
	ColonyContext->MapKey(RotateAction, EKeys::Q).Modifiers.Add(Negate());
	ColonyContext->MapKey(DragAction, EKeys::MiddleMouseButton);
	ColonyContext->MapKey(LookAction, EKeys::Mouse2D);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->AddMappingContext(ColonyContext, 0);
		}
	}

	// --- Bindings ---
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATSCPlayerController::OnMove);
		EIC->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ATSCPlayerController::OnZoom);
		EIC->BindAction(RotateAction, ETriggerEvent::Triggered, this, &ATSCPlayerController::OnRotate);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATSCPlayerController::OnLook);
		EIC->BindAction(DragAction, ETriggerEvent::Started, this, &ATSCPlayerController::OnDragStarted);
		EIC->BindAction(DragAction, ETriggerEvent::Completed, this, &ATSCPlayerController::OnDragCompleted);
		EIC->BindAction(DragAction, ETriggerEvent::Canceled, this, &ATSCPlayerController::OnDragCompleted);
	}
	else
	{
		UE_LOG(LogSaga, Error, TEXT("InputComponent is not an EnhancedInputComponent - check DefaultInput.ini"));
	}
}

void ATSCPlayerController::OnMove(const FInputActionValue& Value)
{
	if (ATSCColonyCameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->AddPanInput(Value.Get<FVector2D>());
	}
}

void ATSCPlayerController::OnZoom(const FInputActionValue& Value)
{
	if (ATSCColonyCameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->AddZoomInput(Value.Get<float>());
	}
}

void ATSCPlayerController::OnRotate(const FInputActionValue& Value)
{
	if (ATSCColonyCameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->AddRotateInput(Value.Get<float>());
	}
}

void ATSCPlayerController::OnLook(const FInputActionValue& Value)
{
	if (!bDragging)
	{
		return;
	}
	if (ATSCColonyCameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->AddDragInput(Value.Get<FVector2D>());
	}
}
