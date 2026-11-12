// The Saga Cutter - game jam project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSCPrimActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;

enum class ETSCShape : uint8 { Cube, Sphere, Cylinder, Cone, Plane };

/** In which saga a part exists. */
enum class ETSCVis : uint8 { Both, LivingOnly, AshenOnly };

/** One engine primitive inside a TSCPrimActor, with a Living and an Ashen look. */
struct FTSCPart
{
	UStaticMeshComponent* Comp = nullptr;
	UMaterialInstanceDynamic* MID = nullptr;
	FLinearColor LCol = FLinearColor::White;
	FLinearColor ACol = FLinearColor::Gray;
	FLinearColor LEm = FLinearColor::Black;
	FLinearColor AEm = FLinearColor::Black;
	FVector Scale = FVector::OneVector;
	ETSCVis Vis = ETSCVis::Both;
	/** >= 0: the part appears when construction reaches this step. */
	int32 BuildStep = -1;
	/** Extra manual visibility switch (crops trampled, crates stolen...). */
	bool bShown = true;
};

struct FTSCLight
{
	UPointLightComponent* Comp = nullptr;
	float LInt = 0.f;
	float AInt = 0.f;
	float Phase = 0.f;
};

/**
 * Base for every object in the game: a bag of engine primitives (cube, sphere, cylinder, cone, plane) with flat
 * colours. Each part has a Living Saga and an Ashen Saga look; SagaAlpha blends between them (0 = Living, 1 = Ashen).
 * No custom meshes anywhere.
 */
UCLASS()
class ATSCPrimActor : public AActor
{
	GENERATED_BODY()

public:
	ATSCPrimActor();

	int32 AddPart(ETSCShape Shape, const FVector& Loc, const FRotator& Rot, const FVector& Scale, const FLinearColor& Col,
		USceneComponent* Parent = nullptr);
	int32 AddPartQ(ETSCShape Shape, const FVector& Loc, const FQuat& Rot, const FVector& Scale, const FLinearColor& Col,
		USceneComponent* Parent = nullptr);
	USceneComponent* AddPivot(const FVector& Loc, USceneComponent* Parent = nullptr);
	void AddLight(const FVector& Loc, const FLinearColor& Col, float LivingIntensity, float AshenIntensity, float Radius);

	void SetAshen(int32 Idx, const FLinearColor& Col) { Parts[Idx].ACol = Col; }
	void SetGlow(int32 Idx, const FLinearColor& Living, const FLinearColor& Ashen) { Parts[Idx].LEm = Living; Parts[Idx].AEm = Ashen; }
	void SetVis(int32 Idx, ETSCVis Vis) { Parts[Idx].Vis = Vis; }
	void SetShown(int32 Idx, bool bShown) { Parts[Idx].bShown = bShown; }

	/** Start blending to the given saga after a delay (the delay makes the change sweep across the map). */
	void StartSagaTransition(float Target, float Delay);
	void SetSagaImmediate(float Alpha);
	void SetBuildProgress(float Progress);
	/** Advances saga blending and light flicker. Called by the game mode, also while the simulation is frozen. */
	void SimVisual(float Dt, float Time);
	/** Push colours, emissive, scale and visibility of every part to the components. */
	void Refresh();

	static FLinearColor Ashify(const FLinearColor& C);

	TArray<FTSCPart> Parts;
	TArray<FTSCLight> Lights;

	float SagaAlpha = 0.f;
	float SagaTarget = 0.f;
	float SagaDelay = 0.f;
	float BuildProgress = 1.f;
	int32 NumBuildSteps = 0;
	/** While true, new parts get consecutive build steps (tile-by-tile construction). */
	bool bStepParts = false;
	/** Editable region this object belongs to (0 = none). */
	int32 Region = 0;
	/** Tag used by the game mode to find special objects. */
	FName Kind;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
};
