// The Saga Cutter - game jam project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSCTerrain.generated.h"

struct FTSCOceanInput;   // include "TSCOcean.h" in the .cpp
class ATSCOcean;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;
class UAudioComponent;
class USoundBase;

/** The land, trails, river and ambience around the colony. */
UCLASS()
class ATSCTerrain : public AActor
{
	GENERATED_BODY()
public:
	ATSCTerrain();
	/** Builds the meshes. Ocean gives the waterline (may be null: the straight line X = 2200 is used). */
	void Build(ATSCOcean* Ocean, const TArray<TArray<FVector2D>>& TrailRoutes);
	void BuildForestFloor(const ATSCOcean* Ocean, TFunctionRef<bool(const FVector2D&, float)> HasClearance,
		TFunctionRef<bool(const FVector2D&)> IsClearing);
	void SetTrailsVisible(bool bVisible);
	void SetClearingStumpsVisible(bool bVisible);
	void StartGroundTransition(int32 Region, const FBox2D& Bounds, const FVector2D& Origin);
	/** Pushes time and ash to the materials, mixes the ambience. Call once per tick. Ash = ATSCGameMode::AshAmount. */
	void Sim(float Dt, const FTSCOceanInput& In, float Ash);
	/** World Z of the land at P (exactly -7 inside the flat rectangle; river bed and sea floor included). */
	FLinearColor GroundColorAt(const FVector2D& P, float Z, const FVector& Normal, const ATSCOcean* Ocean) const;
	float GroundRelief(const FVector2D& P) const;
	float HeightAt(const FVector2D& P) const;
	/** River centreline Y at world X, and its water level. */
	static float RiverY(float X);
	static float RiverZ(float X);
	static float RiverMouthCut(float X, float Y);
	/** 0 on the river centreline, 1 at 3500 units or more from it. */
	float RiverProximity(const FVector2D& P) const;

	UPROPERTY(EditAnywhere, Category = "Saga|Terrain") float RiverVolume = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Saga|Terrain") float WindVolume = 0.35f;
	UPROPERTY(EditAnywhere, Category = "Saga|Terrain") float BirdsVolume = 0.3f;
	UPROPERTY(EditAnywhere, Category = "Saga|Terrain") float SfxVolume = 0.4f;
private:
	void BuildLand(const ATSCOcean* Ocean);
	bool OverlapsTrail(const FVector2D& P, float Radius) const;
	struct FGroundMound
	{
		FVector2D Center, Axis;
		float Radius, Height;
	};
	TArray<FGroundMound> Mounds;
	void BuildTrails(const TArray<TArray<FVector2D>>& Routes, const ATSCOcean* Ocean);
	struct FTrailSegment
	{
		FVector2D Start, End;
		float HalfWidth;
	};
	TArray<FTrailSegment> TrailSegments;
	struct FGroundTransition
	{
		FVector2D Origin = FVector2D::ZeroVector;
		float Elapsed = -1.f;
	};
	FGroundTransition GroundTransitions[3];
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Land;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Trails;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> River;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LandMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TrailMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RockMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RiverMID;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ClearingStumps;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> ForestFloor;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pines;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Boulders;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> GroundPlantMIDs;
	UPROPERTY() TObjectPtr<UAudioComponent> RiverLoopComp;
	UPROPERTY() TObjectPtr<UAudioComponent> WindLoopComp;
	UPROPERTY() TObjectPtr<UAudioComponent> BirdsLoopComp;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> Ravens;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> Owls;
	float CallTimer = 12.f;
	float MixSmooth[3] = { 0.01f, 0.01f, 0.01f };   // smoothed river / wind / birds volumes
	float MixLogTimer = 0.f;
	float LastAsh = -1.f;
	struct FRiverImpact
	{
		FVector Position;
		FVector2D Downstream;
		float Cooldown;
	};
	TWeakObjectPtr<ATSCOcean> SprayRenderer;
	TArray<FRiverImpact> RiverImpacts;
	FRandomStream SprayRandom{36017};
};
