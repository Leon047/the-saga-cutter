// The Saga Cutter - game jam project.

#include "TSCUnit.h"

#include "TSCBuilders.h"
#include "TSCGameMode.h"

using namespace TSC;

void ATSCUnit::Init(ATSCGameMode* InGM, ETSCRole InRole, const FString& InName)
{
	GM = InGM;
	Role = InRole;
	DisplayName = InName;
	bNamed = !InName.IsEmpty();
	AnimPhase = FMath::FRand() * 6.f;

	switch (Role)
	{
	case ETSCRole::Worker: Team = ETSCTeam::Villager; MaxHP = 60.f; Damage = 9.f; break;
	case ETSCRole::Hunter: Team = ETSCTeam::Villager; MaxHP = 130.f; Damage = 13.f; AttackRange = 950.f; AttackInterval = 1.0f; AggroRange = 1000.f; bFights = true; break;
	case ETSCRole::Woodcutter: Team = ETSCTeam::Villager; MaxHP = 90.f; Damage = 14.f; break;
	case ETSCRole::Healer: Team = ETSCTeam::Villager; MaxHP = 50.f; Damage = 4.f; break;
	case ETSCRole::Shield: Team = ETSCTeam::Villager; MaxHP = 170.f; Damage = 13.f; bFights = true; break;
	case ETSCRole::Creature: Team = ETSCTeam::Creature; MaxHP = 44.f; Damage = 7.f; Speed = 105.f; AttackInterval = 1.5f; AggroRange = 600.f; bFights = true; Height = 170.f; break;
	case ETSCRole::Raider: Team = ETSCTeam::Raider; MaxHP = 62.f; Damage = 10.f; Speed = 165.f; AttackInterval = 1.25f; AggroRange = 650.f; bFights = true; break;
	case ETSCRole::Boar: Team = ETSCTeam::Animal; MaxHP = 50.f; Damage = 0.f; Speed = 250.f; Height = 80.f; break;
	}
	HP = MaxHP;
	BuildFigure();
	Refresh();
}

void ATSCUnit::BuildFigure()
{
	Body = AddPivot(FVector::ZeroVector);

	if (Role == ETSCRole::Creature)
	{
		// Flattened sphere cap with glowing spots, tendrils hanging down.
		const FLinearColor Pale(0.75f, 0.85f, 0.78f);
		const int32 Cap = AddPart(ETSCShape::Sphere, FVector(0.f, 0.f, 130.f), FRotator::ZeroRotator, FVector(1.05f, 1.05f, 0.5f), Pale, Body);
		SetGlow(Cap, FLinearColor(0.25f, 0.6f, 0.4f), FLinearColor(0.25f, 0.6f, 0.4f));
		AddPart(ETSCShape::Sphere, FVector(0.f, 0.f, 112.f), FRotator::ZeroRotator, FVector(0.7f, 0.7f, 0.3f), Pale * 0.5f, Body);
		for (int32 I = 0; I < 6; ++I)
		{
			const FVector D = FRotator(0.f, I * 60.f + 20.f, 0.f).Vector();
			const int32 Spot = AddPart(ETSCShape::Sphere, FVector(D.X * 30.f, D.Y * 30.f, 147.f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 0.1f), Pal::Shroom, Body);
			SetGlow(Spot, FLinearColor(2.f, 9.f, 5.f), FLinearColor(2.f, 9.f, 5.f));
		}
		for (int32 I = 0; I < 6; ++I)
		{
			const FVector D = FRotator(0.f, I * 60.f, 0.f).Vector();
			USceneComponent* Tp = AddPivot(FVector(D.X * 26.f, D.Y * 26.f, 112.f), Body);
			const float Len = 0.7f + 0.25f * (I % 3);
			const int32 Td = AddPart(ETSCShape::Cylinder, FVector(0.f, 0.f, -50.f * Len), FRotator::ZeroRotator, FVector(0.07f, 0.07f, Len), Pale * 0.8f, Tp);
			SetGlow(Td, FLinearColor(0.1f, 0.4f, 0.25f), FLinearColor(0.1f, 0.4f, 0.25f));
			Tendrils.Add(Tp);
		}
		return;
	}

	if (Role == ETSCRole::Boar)
	{
		const FLinearColor Hide(0.13f, 0.08f, 0.05f);
		AddPart(ETSCShape::Sphere, FVector(0.f, 0.f, 44.f), FRotator::ZeroRotator, FVector(1.0f, 0.56f, 0.56f), Hide, Body);
		AddPart(ETSCShape::Sphere, FVector(46.f, 0.f, 40.f), FRotator::ZeroRotator, FVector(0.46f, 0.4f, 0.4f), Hide * 0.8f, Body);
		AddPart(ETSCShape::Cone, FVector(66.f, 12.f, 40.f), FRotator(-50.f, 0.f, 0.f), FVector(0.07f, 0.07f, 0.22f), FLinearColor(0.8f, 0.8f, 0.7f), Body);
		AddPart(ETSCShape::Cone, FVector(66.f, -12.f, 40.f), FRotator(-50.f, 0.f, 0.f), FVector(0.07f, 0.07f, 0.22f), FLinearColor(0.8f, 0.8f, 0.7f), Body);
		for (int32 I = 0; I < 4; ++I)
		{
			AddPart(ETSCShape::Cylinder, FVector(I < 2 ? 26.f : -26.f, I % 2 ? 14.f : -14.f, 12.f), FRotator::ZeroRotator, FVector(0.1f, 0.1f, 0.24f), Hide * 0.6f, Body);
		}
		return;
	}

	// Stocky human figure: cylinder body, sphere head, cone helmet, round shield, axe on a swinging arm.
	FLinearColor Tunic(0.20f, 0.22f, 0.27f);
	switch (Role)
	{
	case ETSCRole::Hunter: Tunic = FLinearColor(0.10f, 0.28f, 0.10f); break;
	case ETSCRole::Woodcutter: Tunic = FLinearColor(0.50f, 0.20f, 0.05f); break;
	case ETSCRole::Healer: Tunic = FLinearColor(0.62f, 0.60f, 0.52f); break;
	case ETSCRole::Shield: Tunic = FLinearColor(0.10f, 0.18f, 0.45f); break;
	case ETSCRole::Raider: Tunic = FLinearColor(0.07f, 0.05f, 0.05f); break;
	default: break;
	}
	const bool bRaider = Role == ETSCRole::Raider;
	const float S = bNamed ? 1.12f : 1.f;

	TunicPart = AddPart(ETSCShape::Cylinder, FVector(0.f, 0.f, 40.f * S), FRotator::ZeroRotator, FVector(0.58f * S, 0.5f * S, 0.62f * S), Tunic, Body);
	AddPart(ETSCShape::Cylinder, FVector(0.f, 0.f, 36.f * S), FRotator::ZeroRotator, FVector(0.6f * S, 0.52f * S, 0.07f), Pal::TimberDark, Body); // belt
	AddPart(ETSCShape::Sphere, FVector(0.f, 0.f, 86.f * S), FRotator::ZeroRotator, FVector(0.4f * S), Pal::Skin, Body);
	AddPart(ETSCShape::Sphere, FVector(9.f, 0.f, 74.f * S), FRotator::ZeroRotator, FVector(0.3f * S, 0.32f * S, 0.26f * S),
		bRaider ? FLinearColor(0.05f, 0.04f, 0.03f) : FLinearColor(0.3f, 0.16f, 0.06f), Body); // beard

	// Helmet (armed) or cap/hood (unarmed).
	const int32 Helm = AddPart(ETSCShape::Cone, FVector(0.f, 0.f, 112.f * S), FRotator::ZeroRotator, FVector(0.42f * S, 0.42f * S, 0.34f * S), Pal::Iron * 1.6f, Body);
	const int32 HelmRim = AddPart(ETSCShape::Cylinder, FVector(0.f, 0.f, 98.f * S), FRotator::ZeroRotator, FVector(0.44f * S, 0.44f * S, 0.05f), Pal::Iron, Body);
	ArmedParts.Add(Helm);
	ArmedParts.Add(HelmRim);
	if (Role == ETSCRole::Hunter || Role == ETSCRole::Healer)
	{
		const int32 Hood = AddPart(ETSCShape::Cone, FVector(-4.f, 0.f, 108.f * S), FRotator(-12.f, 0.f, 0.f), FVector(0.46f * S, 0.46f * S, 0.4f * S), Tunic * 0.8f, Body);
		UnarmedParts.Add(Hood);
	}
	else if (!bRaider)
	{
		const int32 Hair = AddPart(ETSCShape::Sphere, FVector(-5.f, 0.f, 95.f * S), FRotator::ZeroRotator, FVector(0.4f * S, 0.4f * S, 0.3f * S), FLinearColor(0.3f, 0.16f, 0.06f), Body);
		UnarmedParts.Add(Hair);
	}

	// Round shield on the left side.
	const FLinearColor ShieldCol = bRaider ? FLinearColor(0.4f, 0.03f, 0.03f) : (Role == ETSCRole::Shield ? FLinearColor(0.75f, 0.6f, 0.1f) : FLinearColor(0.12f, 0.3f, 0.16f));
	const int32 Sh = AddPart(ETSCShape::Cylinder, FVector(12.f, -34.f * S, 50.f * S), FRotator(0.f, 0.f, 90.f), FVector(0.56f * S, 0.56f * S, 0.06f), ShieldCol, Body);
	const int32 Boss = AddPart(ETSCShape::Sphere, FVector(12.f, -38.f * S, 50.f * S), FRotator::ZeroRotator, FVector(0.16f), Pal::Iron * 2.f, Body);
	ArmedParts.Add(Sh);
	ArmedParts.Add(Boss);

	// Right arm with a tool: axe, or a bow for the hunter.
	Arm = AddPivot(FVector(0.f, 32.f * S, 62.f * S), Body);
	AddPart(ETSCShape::Cube, FVector(14.f, 0.f, -8.f), FRotator(-30.f, 0.f, 0.f), FVector(0.12f, 0.12f, 0.42f), Tunic * 0.8f, Arm);
	if (Role == ETSCRole::Hunter)
	{
		AddPart(ETSCShape::Cube, FVector(34.f, 0.f, 4.f), FRotator::ZeroRotator, FVector(0.05f, 0.05f, 1.0f), Pal::Timber * 1.5f, Arm);
		AddPart(ETSCShape::Cube, FVector(28.f, 0.f, 4.f), FRotator::ZeroRotator, FVector(0.015f, 0.015f, 0.95f), FLinearColor(0.8f, 0.8f, 0.7f), Arm);
	}
	else if (Role != ETSCRole::Healer)
	{
		AddPart(ETSCShape::Cube, FVector(30.f, 0.f, 12.f), FRotator::ZeroRotator, FVector(0.05f, 0.05f, 0.7f), Pal::Timber * 1.4f, Arm);
		AddPart(ETSCShape::Cube, FVector(38.f, 0.f, 40.f), FRotator::ZeroRotator, FVector(0.24f, 0.04f, 0.18f), Pal::Iron * 2.2f, Arm);
	}
	else
	{
		AddPart(ETSCShape::Sphere, FVector(26.f, 0.f, -20.f), FRotator::ZeroRotator, FVector(0.2f), FLinearColor(0.2f, 0.5f, 0.15f), Arm); // herb pouch
	}

	bArmed = true;
	SetArmed(bRaider || Role == ETSCRole::Shield);
}

void ATSCUnit::SetArmed(bool bInArmed)
{
	if (bArmed == bInArmed) { return; }
	bArmed = bInArmed;
	for (int32 I : ArmedParts) { SetShown(I, bArmed); }
	for (int32 I : UnarmedParts) { SetShown(I, !bArmed); }
	Refresh();
}

void ATSCUnit::SetJob(ETSCJob InJob)
{
	Job = InJob;
	if (Role == ETSCRole::Worker && TunicPart >= 0)
	{
		const FLinearColor C = Job == ETSCJob::Wood ? FLinearColor(0.36f, 0.17f, 0.07f)
			: Job == ETSCJob::Food ? FLinearColor(0.17f, 0.30f, 0.09f) : FLinearColor(0.20f, 0.22f, 0.27f);
		Parts[TunicPart].LCol = C;
		Parts[TunicPart].ACol = C;
		Refresh();
	}
}

void ATSCUnit::OrderWork(const FVector& A, const FVector& B)
{
	Order = ETSCOrder::Work;
	WorkA = A;
	WorkB = B;
	WorkPhase = 0;
	WorkTimer = 0.f;
}

void ATSCUnit::OrderHold(const FVector& Pos)
{
	Order = ETSCOrder::Hold;
	HoldPos = Pos;
	if (bSheltered)
	{
		bSheltered = false;
		SetActorHiddenInGame(false);
	}
}

void ATSCUnit::OrderShelter(const FVector& Pos)
{
	Order = ETSCOrder::Shelter;
	HoldPos = Pos;
}

void ATSCUnit::OrderLeave(const FVector& Pos)
{
	Order = ETSCOrder::Leave;
	HoldPos = Pos;
	Target = nullptr;
}

void ATSCUnit::OrderAssault()
{
	Order = ETSCOrder::Assault;
}

void ATSCUnit::Teleport(const FVector& Pos)
{
	SetActorLocation(FVector(Pos.X, Pos.Y, 0.f));
	Path.Reset();
}

bool ATSCUnit::AtHoldPos() const
{
	return FVector::Dist2D(GetActorLocation(), HoldPos) < 60.f;
}

void ATSCUnit::Hurt(float Amount)
{
	if (!IsAlive() || bInvulnerable) { return; }
	HP -= Amount;
	BarTimer = 6.f;
	if (HP <= 0.f)
	{
		Kill();
	}
}

void ATSCUnit::Kill()
{
	if (bDead) { return; }
	HP = 0.f;
	bDead = true;
	DeathTimer = 0.f;
	Need = ETSCNeed::None;
	if (GM) { GM->OnUnitDied(this); }
}

ATSCUnit* ATSCUnit::FindTarget() const
{
	ATSCUnit* Best = nullptr;
	float BestD = AggroRange;
	const FVector Me = GetActorLocation();
	const bool bRanged = AttackRange > 300.f;
	for (ATSCUnit* U : GM->Units)
	{
		if (!U || U == this || !U->IsTargetable() || U->Team == ETSCTeam::Animal) { continue; }
		const bool bEnemy = (Team == ETSCTeam::Villager) != (U->Team == ETSCTeam::Villager);
		if (!bEnemy) { continue; }
		const float D = FVector::Dist2D(Me, U->GetActorLocation());
		if (D < BestD && (bRanged || GM->CanReach(this, U)))
		{
			BestD = D;
			Best = U;
		}
	}
	return Best;
}

bool ATSCUnit::StepToward(const FVector& Dest, float Dt, float StopDist)
{
	const FVector Me = Loc2D();
	if (FVector::Dist2D(Me, Dest) <= StopDist)
	{
		return true;
	}
	if (PathVersion != GM->GridVersion || FVector::Dist2D(PathDest, Dest) > 120.f)
	{
		Path.Reset();
		GM->FindPath(Me, Dest, Path);
		PathIdx = 0;
		PathDest = Dest;
		PathVersion = GM->GridVersion;
	}
	FVector Next = Dest;
	while (PathIdx < Path.Num())
	{
		if (FVector::Dist2D(Me, Path[PathIdx]) < 24.f) { ++PathIdx; continue; }
		Next = Path[PathIdx];
		break;
	}
	FVector Dir = Next - Me;
	Dir.Z = 0.f;
	const float Dist = Dir.Size();
	if (Dist > 1.f)
	{
		Dir /= Dist;
		SetActorLocation(Me + Dir * FMath::Min(Dist, Speed * Dt));
		const FRotator Want = Dir.Rotation();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Want.Yaw, 0.f), Dt, 10.f));
	}
	return false;
}

void ATSCUnit::Sim(float Dt)
{
	if (bGone) { return; }
	if (bDead)
	{
		DeathTimer += Dt;
		if (Role == ETSCRole::Creature)
		{
			SetActorScale3D(FVector(FMath::Max(0.01f, 1.f - DeathTimer * 1.6f)));
		}
		else
		{
			Body->SetRelativeRotation(FRotator(0.f, 0.f, FMath::Min(DeathTimer * 260.f, 90.f)));
			if (DeathTimer > 5.f)
			{
				Body->SetRelativeLocation(FVector(0.f, 0.f, -(DeathTimer - 5.f) * 30.f));
			}
		}
		if (DeathTimer > (Role == ETSCRole::Creature ? 0.7f : 8.f))
		{
			bGone = true;
			SetActorHiddenInGame(true);
		}
		return;
	}
	if (bSheltered) { return; }

	BarTimer = FMath::Max(0.f, BarTimer - Dt);
	AttackTimer = FMath::Max(0.f, AttackTimer - Dt);
	bool bMoving = false;

	if (bDowned)
	{
		Body->SetRelativeRotation(FRotator(0.f, 0.f, 82.f));
		return;
	}
	Body->SetRelativeRotation(FRotator::ZeroRotator);

	// ----- fighting -----
	if (bFights && Order != ETSCOrder::Leave && Order != ETSCOrder::Shelter)
	{
		ThinkTimer -= Dt;
		ATSCUnit* T = Target.Get();
		if (ThinkTimer <= 0.f || !T || !T->IsTargetable())
		{
			ThinkTimer = 0.4f;
			T = FindTarget();
			Target = T;
		}
		if (T)
		{
			BarTimer = 6.f;
			const FVector TL = T->Loc2D();
			const float D = FVector::Dist2D(GetActorLocation(), TL);
			if (D > AttackRange)
			{
				StepToward(TL, Dt, AttackRange * 0.85f);
				bMoving = true;
			}
			else
			{
				const FRotator Want = (TL - Loc2D()).Rotation();
				SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Want.Yaw, 0.f), Dt, 12.f));
				if (AttackTimer <= 0.f)
				{
					AttackTimer = AttackInterval * FMath::FRandRange(0.9f, 1.1f);
					SwingTimer = 0.35f;
					T->Hurt(Damage);
					if (AttackRange > 300.f)
					{
						GM->AddTracer(GetActorLocation() + FVector(0.f, 0.f, 90.f), T->GetActorLocation() + FVector(0.f, 0.f, T->Height * 0.6f), FLinearColor(1.f, 0.9f, 0.6f));
					}
				}
			}
			Animate(Dt, bMoving);
			return;
		}
	}

	// ----- orders -----
	switch (Order)
	{
	case ETSCOrder::Work:
	{
		const FVector& Dest = WorkPhase == 0 ? WorkA : WorkB;
		if (WorkTimer > 0.f)
		{
			WorkTimer -= Dt;
			if (WorkPhase == 0 && SwingTimer <= 0.f) { SwingTimer = 0.6f; }
			if (WorkTimer <= 0.f) { WorkPhase = 1 - WorkPhase; }
		}
		else if (StepToward(Dest, Dt, 40.f))
		{
			WorkTimer = WorkPhase == 0 ? FMath::FRandRange(4.f, 7.f) : FMath::FRandRange(1.f, 2.5f);
		}
		else
		{
			bMoving = true;
		}
		break;
	}
	case ETSCOrder::Hold:
		bMoving = !StepToward(HoldPos, Dt, 30.f);
		break;
	case ETSCOrder::Shelter:
		if (StepToward(HoldPos, Dt, 50.f))
		{
			bSheltered = true;
			SetActorHiddenInGame(true);
		}
		bMoving = true;
		break;
	case ETSCOrder::Leave:
		if (StepToward(HoldPos, Dt, 50.f))
		{
			bGone = true;
			SetActorHiddenInGame(true);
		}
		bMoving = true;
		break;
	case ETSCOrder::Assault:
	{
		FVector Goal;
		ETSCStructure Struct = ETSCStructure::None;
		GM->GetAssaultGoal(this, Goal, Struct);
		if (StepToward(Goal, Dt, 90.f))
		{
			if (Struct != ETSCStructure::None && AttackTimer <= 0.f)
			{
				AttackTimer = AttackInterval;
				SwingTimer = 0.35f;
				GM->DamageStructure(Struct, Damage * 0.3f);
			}
		}
		else
		{
			bMoving = true;
		}
		break;
	}
	}
	Animate(Dt, bMoving);
}

void ATSCUnit::Animate(float Dt, bool bMoving)
{
	AnimPhase += Dt * (bMoving ? 11.f : 2.f);
	SwingTimer = FMath::Max(0.f, SwingTimer - Dt);

	if (Role == ETSCRole::Creature)
	{
		// Drift: slow bob, tendrils sway.
		Body->SetRelativeLocation(FVector(0.f, 0.f, 14.f * FMath::Sin(AnimPhase * 0.25f)));
		for (int32 I = 0; I < Tendrils.Num(); ++I)
		{
			const float Ph = AnimPhase * 0.3f + I * 1.1f;
			Tendrils[I]->SetRelativeRotation(FRotator(16.f * FMath::Sin(Ph) + (bMoving ? -14.f : 0.f), 0.f, 14.f * FMath::Cos(Ph * 0.8f)));
		}
		if (SwingTimer > 0.f)
		{
			Body->SetRelativeLocation(FVector(40.f * FMath::Sin(SwingTimer / 0.35f * PI), 0.f, 0.f));
		}
		return;
	}

	const float Bob = bMoving ? FMath::Abs(FMath::Sin(AnimPhase)) * 7.f : 0.f;
	Body->SetRelativeLocation(FVector(0.f, 0.f, Bob));
	if (bMoving)
	{
		Body->SetRelativeRotation(FRotator(0.f, 0.f, 5.f * FMath::Sin(AnimPhase)));
	}
	if (Arm)
	{
		float Pitch = bMoving ? 18.f * FMath::Sin(AnimPhase) : 0.f;
		if (SwingTimer > 0.f)
		{
			Pitch = -95.f * FMath::Sin(FMath::Clamp(SwingTimer / 0.35f, 0.f, 1.f) * PI);
		}
		Arm->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
	}
}
