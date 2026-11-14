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
/** Deer..Bull: wildlife and livestock from Content/Animals (team Animal, never fight, not targeted). Keep Deer first: IsWildlife() is Role >= Deer. */
enum class ETSCRole : uint8 { Worker, Hunter, Woodcutter, Healer, Shield, Creature, Raider, Boar, Deer, Stag, Fox, Wolf, Husky, Cow, Bull };
enum class ETSCJob : uint8 { Idle, Wood, Food };
enum class ETSCNeed : uint8 { None, Wood, Leaf, Heal };
/** Wander: roam around HoldPos (graze, rest, move on). Follow: keep near Leader (the hunter's dog). */
enum class ETSCOrder : uint8 { Work, Hold, Shelter, Leave, Assault, Wander, Follow };
enum class ETSCStructure : uint8 { None, Gate, Longhouse };
/** Wildlife confinement relative to the palisade: Outside = forest animals (never cross the wall inwards), Inside = livestock (never leave). */
enum class ETSCZone : uint8 { Free, Outside, Inside };

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
	/** Wildlife: roam within Radius of Center. */
	void OrderWander(const FVector& Center, float Radius);
	/** Wildlife: stay close behind InLeader (falls back to wandering where it stood if the leader is gone). */
	void OrderFollow(ATSCUnit* InLeader);
	bool IsWildlife() const { return Role >= ETSCRole::Deer; }
	void Teleport(const FVector& Pos);
	FVector Loc2D() const { FVector L = GetActorLocation(); L.Z = 0.f; return L; }
	bool AtHoldPos() const;
	/** Scripted figures that are not simulated: only the idle animation. */
	void Idle(float Dt);

	virtual void Refresh() override;

	ETSCTeam Team = ETSCTeam::Villager;
	ETSCRole Role = ETSCRole::Worker;
	ETSCJob Job = ETSCJob::Idle;
	ETSCNeed Need = ETSCNeed::None;
	ETSCOrder Order = ETSCOrder::Hold;
	/** Wildlife: which side of the palisade the animal must stay on (checked on every step). */
	ETSCZone Zone = ETSCZone::Free;
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
	/** Wildlife: the skinned animal from Content/Animals/<Name>. False if missing (then nothing is shown). */
	bool BuildAnimal();
	/** Wildlife movement (Wander / Follow orders). Returns true while moving. */
	bool SimWildlife(float Dt);
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
	/** Sinks the unit into the shallows (visual only) and throws splashes while it walks there. */
	void UpdateWading(float Dt);
	float WadeStepTimer = 0.f;
	FVector LastWadePos = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<ATSCGameMode> GM;

	TWeakObjectPtr<ATSCUnit> Target;
	/** Follow order: the unit to trail (the husky follows Hakon). */
	TWeakObjectPtr<ATSCUnit> Leader;
	/** Wander order: roaming radius around HoldPos, current goal and the pause before moving on. */
	float WanderRadius = 0.f;
	FVector WanderGoal = FVector::ZeroVector;
	float WanderPause = 0.f;
	/** Wildlife moving fast (gallop) rather than walking. */
	bool bGallop = false;
	/** Set by StepToward when the next step would cross the palisade against Zone (the animal stays put and picks a new goal). */
	bool bZoneBlocked = false;
	/** Village dog playing with the jarl: seconds left, pause before it may do so again, and its angle on the circle around him. */
	float PlayTimer = 0.f;
	float PlayCooldown = 0.f;
	float PlayAngle = 0.f;
	/** True if P is on the side of the palisade this animal is allowed on. */
	bool ZoneAllows(const FVector& P) const;
	/** A random wander goal around HoldPos that Zone allows (falls back to the current spot). */
	FVector PickWanderGoal() const;
	/** Prey (deer, stag, fox): seconds left running from FleeFrom (the last wolf seen within range). */
	float FleeTimer = 0.f;
	FVector FleeFrom = FVector::ZeroVector;
	/** Walking speed from Init (fleeing and following change Speed temporarily). */
	float BaseSpeed = 190.f;
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
	/** Hand bones and weapon size factor of the model in use (weapon fallback bone, see AddWeapon). */
	FName HandR;
	FName HandL;
	float WeaponScale = 1.f;
	/** While > 0 a one-shot clip (attack, hit, work) plays and locomotion does not override it. */
	float OneShotTimer = 0.f;
	float DiagT = 0.f; // TEMP DIAG (remove)

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
