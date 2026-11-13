// The Saga Cutter - game jam project.

#pragma once

#include "CoreMinimal.h"
#include "TSCPrimActor.h"
#include "TSCUnit.generated.h"

class ATSCGameMode;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMeshComponent;
class UAnimSequence;
class UMaterialInstanceDynamic;

/** Animation clips shared by the Viking characters (Content/Characters/viking_male, viking_female, viking_lord). */
enum class ETSCAnim : uint8 { Idle, Walk, Run, WalkCarry, RunCarry, Slash, Shoot, Punch, PickUp, Hit, Death, Num };

enum class ETSCTeam : uint8 { Villager, Creature, Raider, Animal };
enum class ETSCRole : uint8 { Worker, Hunter, Woodcutter, Healer, Shield, Creature, Raider, Boar };
enum class ETSCJob : uint8 { Idle, Wood, Food };
enum class ETSCNeed : uint8 { None, Wood, Leaf, Heal };
enum class ETSCOrder : uint8 { Work, Hold, Shelter, Leave, Assault };
enum class ETSCStructure : uint8 { None, Gate, Longhouse };

/** An arrow mesh in flight from the hunter's bow to the target (purely visual, damage is dealt on release). */
struct FTSCArrowFlight
{
	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	/** 0..1 progress; >= 1 means finished and the component can be reused. */
	float T = 1.f;
	float Dur = 0.3f;
};

/**
 * A villager, raider, mushroom creature or animal: a full 3D figure made of primitives that moves freely over the grid.
 * Animated by moving its parts (bobbing walk, swinging arm, drifting tendrils). Simulated by the game mode, not by Tick.
 */
UCLASS()
class ATSCUnit : public ATSCPrimActor
{
	GENERATED_BODY()

public:
	void Init(ATSCGameMode* InGM, ETSCRole InRole, const FString& InName);
	void Sim(float Dt);

	bool IsAlive() const { return !bDead && !bGone; }
	/** Alive and on the map (not sheltering indoors). */
	bool IsTargetable() const { return IsAlive() && !bSheltered; }
	void Hurt(float Amount);
	void Kill();

	void SetJob(ETSCJob InJob);
	void SetArmed(bool bInArmed);
	void OrderWork(const FVector& A, const FVector& B);
	void OrderHold(const FVector& Pos);
	void OrderShelter(const FVector& Pos);
	void OrderLeave(const FVector& Pos);
	void OrderAssault();
	void Teleport(const FVector& Pos);
	FVector Loc2D() const { FVector L = GetActorLocation(); L.Z = 0.f; return L; }
	bool AtHoldPos() const;

	virtual void Refresh() override;

	ETSCTeam Team = ETSCTeam::Villager;
	ETSCRole Role = ETSCRole::Worker;
	ETSCJob Job = ETSCJob::Idle;
	ETSCNeed Need = ETSCNeed::None;
	ETSCOrder Order = ETSCOrder::Hold;
	FString DisplayName;
	bool bNamed = false;

	float HP = 60.f;
	float MaxHP = 60.f;
	float Damage = 8.f;
	float AttackRange = 120.f;
	float AttackInterval = 1.2f;
	float AggroRange = 700.f;
	float Speed = 190.f;
	bool bFights = false;
	bool bDefender = false;
	/** Scripted moments: cannot be hurt. */
	bool bInvulnerable = false;
	/** Seconds the health bar stays visible after a fight. */
	float BarTimer = 0.f;
	float Height = 120.f;

	bool bDead = false;
	bool bGone = false;
	bool bSheltered = false;
	bool bArmed = false;
	/** Lying down (ill or wounded), not moving. */
	bool bDowned = false;

private:
	void BuildFigure();
	/** Humans: a skinned Viking (random man / woman, Torvald is the lord). Returns false if the assets are missing (then primitives are used). */
	bool BuildViking(const FLinearColor& Tunic);
	/** Weapon mesh on its hand socket (or the hand bone if the socket is missing), oriented, sized and gripped per Def.
	 *  bArmedOnly: shown only while armed; otherwise it is the always-held item (bow, staff). */
	UStaticMeshComponent* AddWeapon(const struct FTSCWeaponDef& Def, bool bArmedOnly);
	/** MIDs for every material slot of a mesh, so Refresh blends them to the Ashen look. */
	void AddTintedMaterials(UMeshComponent* Comp);
	/** Hunter: launch a visible arrow towards To. False if there is no arrow mesh (caller draws the tracer line). */
	bool FireArrow(const FVector& To);
	void UpdateArrows(float Dt);
	/** Plays an animation on the Viking mesh. Loops keep running if already playing. Returns the clip length (0 if missing). */
	float PlayAnim(ETSCAnim Id, bool bLoop, float Rate = 1.f);
	/** Attack / work swing: arm swing for primitive figures, a one-shot clip for the Viking mesh.
	 *  Window > 0 speeds the clip up so it fits into that many seconds. */
	void StartSwing(float ArmTime, float Window, ETSCAnim Clip);
	bool StepToward(const FVector& Dest, float Dt, float StopDist);
	ATSCUnit* FindTarget() const;
	void Animate(float Dt, bool bMoving);

	UPROPERTY()
	TObjectPtr<ATSCGameMode> GM;

	TWeakObjectPtr<ATSCUnit> Target;
	FVector WorkA = FVector::ZeroVector;
	FVector WorkB = FVector::ZeroVector;
	FVector HoldPos = FVector::ZeroVector;
	int32 WorkPhase = 0;
	float WorkTimer = 0.f;

	TArray<FVector> Path;
	int32 PathIdx = 0;
	FVector PathDest = FVector::ZeroVector;
	int32 PathVersion = -1;

	float AttackTimer = 0.f;
	float SwingTimer = 0.f;
	float AnimPhase = 0.f;
	float DeathTimer = 0.f;
	float ThinkTimer = 0.f;

	UPROPERTY()
	TObjectPtr<USceneComponent> Body;
	UPROPERTY()
	TObjectPtr<USceneComponent> Arm;
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Tendrils;
	int32 TunicPart = -1;
	TArray<int32> ArmedParts;
	TArray<int32> UnarmedParts;

	/** Viking skeletal mesh (humans only; null for primitive figures). */
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> Anims;
	UPROPERTY()
	TObjectPtr<UAnimSequence> CurAnim;
	/** One MID per material slot; the Living colour is blended to its Ashen version in Refresh. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MeshMIDs;
	TArray<FLinearColor> MeshCols;
	int32 TunicSlot = -1;
	/** While > 0 a one-shot clip (attack, hit, work) plays and locomotion does not override it. */
	float OneShotTimer = 0.f;

	/** Sword / axe / shield: visible only while armed. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ArmedItems;
	/** Bow or staff: always in hand. Arrows start from it. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HeldItem;
	/** Arrow pool, parallel to Arrows. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ArrowComps;
	TArray<FTSCArrowFlight> Arrows;
};
