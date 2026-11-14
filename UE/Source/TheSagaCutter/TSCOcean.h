// The Saga Cutter - game jam project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSCOcean.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UAudioComponent;
class USoundBase;
class UInstancedStaticMeshComponent;

struct FTSCOceanInput
{
	float Time = 0.f;                 // ATSCGameMode::WorldTime
	float Ash = 0.f;                  // 0..1, dock region saga alpha
	bool bDaylight = true;            // gulls only cry by day
	FVector Focus = FVector::ZeroVector;  // camera focus point on the ground
	float Zoom01 = 0.5f;              // 0 = zoomed in, 1 = zoomed out
	FLinearColor SunColor = FLinearColor::White;
	float SunIntensity = 1.f;
};

struct FTSCFloater
{
	TWeakObjectPtr<AActor> Actor;
	float HalfLen = 100.f;   // along the actor's local X
	float HalfWidth = 50.f;  // along local Y
	float RestZ = 0.f;       // root Z above the surface at rest (negative = sits lower)
	float FoamRadius = 0.f;  // > 0: reported to the material as a foam ring
	bool bFree = false;      // flotsam: the ocean also moves it in XY
	FVector2D Home = FVector2D::ZeroVector;
	// state
	float Z = 0.f, VelZ = 0.f, Pitch = 0.f, VelPitch = 0.f, Roll = 0.f, VelRoll = 0.f;  // degrees
	FVector2D Vel = FVector2D::ZeroVector;
	FVector2D LastXY = FVector2D::ZeroVector;
	bool bInit = false;
	float SprayTimer = 0.f;   // bow spray emit interval
	bool bSprayLeft = false;  // bow spray alternates sides
};

/** Stylized animated sea: a procedural grid mesh, one wave function shared with the material, buoyant floaters and the ocean soundscape. */
UCLASS()
class ATSCOcean : public AActor
{
	GENERATED_BODY()
public:
	ATSCOcean();
	/** Builds the grid mesh and the material instance. ShoreX = world X of the waterline, SurfaceZ = rest level. */
	void Build(float InShoreX, float InSurfaceZ);
	/** Pushes parameters, steps every floater, mixes the audio. Call once per tick after the game moved its boats. */
	void Sim(float Dt, const FTSCOceanInput& In);
	/** World Z of the water surface at P for the time of the last Sim call. */
	float HeightAt(const FVector2D& P) const;
	/** World X of the (fractal) waterline at Y: ShoreX + S(Y), the same function the material and the beach mesh use. */
	float ShoreXAt(float Y) const;
	/** Registers an actor that rides the waves; the caller keeps moving it in XY (unless bFree), the ocean owns Z, pitch and roll. */
	int32 AddFloater(AActor* Actor, float HalfLen, float HalfWidth, float RestZ, float FoamRadius = 0.f, bool bFree = false);
	/** One-shot: keel on gravel (prologue landfall, raiders landing), with a spray burst at At. */
	void PlayLanding(const FVector2D& At, float Volume = 1.f);
	/** A burst of droplets at the surface. Strength ~0.3 (footstep) .. 2 (boat landing). Dir biases the throw (may be zero). */
	void Splash(const FVector2D& At, float Strength, const FVector2D& Dir = FVector2D::ZeroVector);
	/** Footstep in the shallows: small splash plus a rate-limited wade sound. */
	void WadeStep(const FVector2D& At, const FVector2D& MoveDir);

	UPROPERTY(EditAnywhere, Category = "Saga|Ocean") float SurfVolume = 0.45f;
	UPROPERTY(EditAnywhere, Category = "Saga|Ocean") float SfxVolume = 0.5f;
private:
	/** Analytic gradient of the wave sum at P (ignores the derivative of the shore attenuation). */
	FVector2D SlopeAt(const FVector2D& P) const;
	/** The beach along the fractal waterline (replaces the old straight shore strip): dry flat top and a wet slope into the water. */
	void BuildBeach();

	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Beach;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;
	UPROPERTY() TObjectPtr<UAudioComponent> Surf;
	UPROPERTY() TObjectPtr<USoundBase> SurfLoop;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> Gulls;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> Creaks;
	UPROPERTY() TObjectPtr<USoundBase> Landing;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Drops;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> Wades;
	struct FDrop { FVector Pos; FVector Vel; float Life; float Size; };
	TArray<FDrop> DropSim;  // at most MaxDrops
	TArray<FTSCFloater> Floaters;
	float ShoreX = 0.f, SurfaceZ = 0.f, Time = 0.f;
	float GullTimer = 6.f, CreakCooldown = 0.f, Proximity = 0.f;
	float WadeSoundCooldown = 0.f, ShoreSprayTimer = 0.f;
};
