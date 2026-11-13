// The Saga Cutter - game jam project.

#pragma once

#include "CoreMinimal.h"
#include "TSCPrimActor.h"
#include "TSCUnit.generated.h"

class ATSCGameMode;

enum class ETSCTeam : uint8 { Villager, Creature, Raider, Animal };
enum class ETSCRole : uint8 { Worker, Hunter, Woodcutter, Healer, Shield, Creature, Raider, Boar };
enum class ETSCJob : uint8 { Idle, Wood, Food };
enum class ETSCNeed : uint8 { None, Wood, Leaf, Heal };
enum class ETSCOrder : uint8 { Work, Hold, Shelter, Leave, Assault };
enum class ETSCStructure : uint8 { None, Gate, Longhouse };

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
	/** Scripted figures that are not simulated: only the idle animation. */
	void Idle(float Dt) { Animate(Dt, false); }

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
};
