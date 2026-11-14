// The Saga Cutter - game jam project.

#include "TSCPrimActor.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Particles/ParticleSystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	UStaticMesh* ShapeMesh(ETSCShape Shape)
	{
		const TCHAR* Path = TEXT("/Engine/BasicShapes/Cube.Cube");
		switch (Shape)
		{
		case ETSCShape::Sphere: Path = TEXT("/Engine/BasicShapes/Sphere.Sphere"); break;
		case ETSCShape::Cylinder: Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder"); break;
		case ETSCShape::Cone: Path = TEXT("/Engine/BasicShapes/Cone.Cone"); break;
		case ETSCShape::Plane: Path = TEXT("/Engine/BasicShapes/Plane.Plane"); break;
		default: break;
		}
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	UMaterialInterface* FlatMaterial()
	{
		// Flat colour + emissive material made for the game; falls back to the engine one (colour only).
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/M_SagaFlat.M_SagaFlat"));
		if (!Mat)
		{
			Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		return Mat;
	}
}

ATSCPrimActor::ATSCPrimActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

FLinearColor ATSCPrimActor::Ashify(const FLinearColor& C)
{
	// Colour drain: mostly grey, darker, a touch warm.
	const float L = C.R * 0.3f + C.G * 0.59f + C.B * 0.11f;
	return FLinearColor(L * 0.55f + C.R * 0.08f + 0.012f, L * 0.5f + C.G * 0.05f + 0.01f, L * 0.47f + C.B * 0.03f + 0.008f, 1.f);
}

int32 ATSCPrimActor::AddPart(ETSCShape Shape, const FVector& Loc, const FRotator& Rot, const FVector& Scale, const FLinearColor& Col,
	USceneComponent* Parent)
{
	return AddPartQ(Shape, Loc, Rot.Quaternion(), Scale, Col, Parent);
}

int32 ATSCPrimActor::AddPartQ(ETSCShape Shape, const FVector& Loc, const FQuat& Rot, const FVector& Scale, const FLinearColor& Col,
	USceneComponent* Parent)
{
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(ShapeMesh(Shape));
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetupAttachment(Parent ? Parent : Root.Get());
	Comp->SetRelativeLocationAndRotation(Loc, Rot);
	Comp->SetRelativeScale3D(Scale);
	Comp->RegisterComponent();
	AddInstanceComponent(Comp);

	FTSCPart Part;
	Part.Comp = Comp;
	Part.MID = UMaterialInstanceDynamic::Create(FlatMaterial(), Comp);
	Comp->SetMaterial(0, Part.MID);
	Part.LCol = Col;
	Part.ACol = Ashify(Col);
	Part.Scale = Scale;
	if (bStepParts)
	{
		Part.BuildStep = NumBuildSteps++;
	}
	if (Part.MID)
	{
		Part.MID->SetVectorParameterValue(TEXT("Color"), Col);
	}
	return Parts.Add(Part);
}

int32 ATSCPrimActor::AddMeshPart(UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, const FVector& Scale, USceneComponent* Parent)
{
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetupAttachment(Parent ? Parent : Root.Get());
	Comp->SetRelativeLocationAndRotation(Loc, Rot);
	Comp->SetRelativeScale3D(Scale);
	Comp->RegisterComponent();
	AddInstanceComponent(Comp);

	FTSCPart Part;
	Part.Comp = Comp;
	Part.Scale = Scale;
	if (bStepParts)
	{
		Part.BuildStep = NumBuildSteps++;
	}
	return Parts.Add(Part);
}

USceneComponent* ATSCPrimActor::AddPivot(const FVector& Loc, USceneComponent* Parent)
{
	USceneComponent* Pivot = NewObject<USceneComponent>(this);
	Pivot->SetMobility(EComponentMobility::Movable);
	Pivot->SetupAttachment(Parent ? Parent : Root.Get());
	Pivot->SetRelativeLocation(Loc);
	Pivot->RegisterComponent();
	AddInstanceComponent(Pivot);
	return Pivot;
}

void ATSCPrimActor::AddLight(const FVector& Loc, const FLinearColor& Col, float LivingIntensity, float AshenIntensity, float Radius)
{
	// Decorative lights are disabled; fire visuals are provided by the VFX assets.
}

void ATSCPrimActor::StartSagaTransition(float Target, float Delay)
{
	SagaTarget = Target;
	SagaDelay = Delay;
}

void ATSCPrimActor::SetSagaImmediate(float Alpha)
{
	SagaAlpha = SagaTarget = Alpha;
	SagaDelay = 0.f;
	Refresh();
}

void ATSCPrimActor::SetBuildProgress(float Progress)
{
	BuildProgress = FMath::Clamp(Progress, 0.f, 1.f);
	Refresh();
}

void ATSCPrimActor::SimVisual(float Dt, float Time)
{
	if (!FMath::IsNearlyEqual(SagaAlpha, SagaTarget))
	{
		if (SagaDelay > 0.f)
		{
			SagaDelay -= Dt;
		}
		else
		{
			SagaAlpha = FMath::FInterpConstantTo(SagaAlpha, SagaTarget, Dt, 0.9f);
			Refresh();
		}
	}
	for (FTSCLight& L : Lights)
	{
		const float Base = FMath::Lerp(L.LInt, L.AInt, SagaAlpha) * BuildProgress;
		const float Flicker = 0.85f + 0.15f * FMath::Sin(Time * 9.f + L.Phase) * FMath::Sin(Time * 5.3f + L.Phase * 2.f);
		L.Comp->SetIntensity(Base * Flicker);
	}
}

void ATSCPrimActor::Refresh()
{
	const float A = FMath::SmoothStep(0.f, 1.f, SagaAlpha);
	for (FTSCPart& P : Parts)
	{
		float V = 1.f;
		if (P.Vis == ETSCVis::LivingOnly) { V = 1.f - A; }
		else if (P.Vis == ETSCVis::AshenOnly) { V = A; }
		if (P.BuildStep >= 0)
		{
			V *= FMath::Clamp(BuildProgress * NumBuildSteps - P.BuildStep, 0.f, 1.f);
		}
		if (!P.bShown) { V = 0.f; }

		const bool bVisible = V > 0.02f;
		// Smoke belongs to its chimney's construction step and stops in the Ashen saga.
		for (USceneComponent* Child : P.Comp->GetAttachChildren())
		{
			if (UParticleSystemComponent* Smoke = Cast<UParticleSystemComponent>(Child))
			{
				if (!Smoke->ComponentHasTag(TEXT("TSC_ChimneySmoke"))) { continue; }
				const bool bSmoking = bVisible && BuildProgress >= 0.999f && SagaAlpha < 0.95f;
				// A stopped system still reports IsActive() while its last particles fade, so check bWasDeactivated too:
				// otherwise smoke switched off and on again within a few seconds would never come back.
				const bool bRunning = Smoke->IsActive() && !Smoke->bWasDeactivated;
				if (bRunning != bSmoking)
				{
					if (bSmoking) { Smoke->ActivateSystem(); } else { Smoke->DeactivateSystem(); }
				}
			}
		}

		if (P.Comp->IsVisible() != bVisible)
		{
			P.Comp->SetVisibility(bVisible);
		}
		if (bVisible)
		{
			P.Comp->SetRelativeScale3D(P.Scale * V);
			if (P.MID)
			{
				P.MID->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(P.LCol, P.ACol, A));
				P.MID->SetVectorParameterValue(TEXT("Emissive"), FMath::Lerp(P.LEm, P.AEm, A));
			}
		}
	}
}
