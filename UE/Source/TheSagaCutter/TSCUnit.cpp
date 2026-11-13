// The Saga Cutter - game jam project.

#include "TSCUnit.h"

#include "TSCBuilders.h"
#include "TSCGameMode.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"

using namespace TSC;

namespace
{
	/** A Viking character imported into Content/Characters/<folder>: skeletal mesh + one AnimSequence per clip. */
	struct FVikingModel
	{
		const TCHAR* Folder;
		const TCHAR* Mesh;
		const TCHAR* AnimPrefix;
	};
	const FVikingModel VikingMale{ TEXT("/Game/Characters/viking_male"), TEXT("Viking_Male"), TEXT("Viking_MaleCharacterArmature_") };
	const FVikingModel VikingFemale{ TEXT("/Game/Characters/viking_female"), TEXT("Viking_Female"), TEXT("Viking_FemaleCharacterArmature_") };
	/** The one lord (Torvald Shield-Bearer): horned helmet. */
	const FVikingModel VikingLord{ TEXT("/Game/Characters/viking_lord"), TEXT("VikingHelmet"), TEXT("VikingHelmetCharacterArmature_") };

	const TCHAR* VikingAnimNames[] = { TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("Walk_Carry"), TEXT("Run_Carry"),
		TEXT("SwordSlash"), TEXT("Shoot_OneHanded"), TEXT("Punch"), TEXT("PickUp"), TEXT("RecieveHit"), TEXT("Death") };
	static_assert(UE_ARRAY_COUNT(VikingAnimNames) == (int32)ETSCAnim::Num, "One clip name per ETSCAnim");

	/** Colour parameter of the imported FBX materials. */
	const FName DiffuseParam(TEXT("DiffuseColor"));

	// Tuning: the FBX faces +Y, units walk along +X.
	constexpr float VikingYaw = -90.f;
	/** Height of the figure in cm (named heroes are 12% taller), same as the old primitive figure. */
	constexpr float VikingHeight = 120.f;
	// Weapon meshes (Content/Weapons). Several candidates where the folder name may still change:
	// the axe folder was created with a Cyrillic "А" (U+0410), the arrow folder is spelt "Errow".
	const TCHAR* const WeaponAxe[] = { TEXT("/Game/Weapons/Axe/Axe_small.Axe_small"), TEXT("/Game/Weapons/Аxe/Axe_small.Axe_small") };
	const TCHAR* const WeaponSword[] = { TEXT("/Game/Weapons/Sword/Sword.Sword") };
	const TCHAR* const WeaponShield[] = { TEXT("/Game/Weapons/Shield/Shield_Round.Shield_Round") };
	const TCHAR* const WeaponBow[] = { TEXT("/Game/Weapons/Bow/Bow_Wooden.Bow_Wooden") };
	const TCHAR* const WeaponStaff[] = { TEXT("/Game/Weapons/Staff/Staff.Staff") };
	const TCHAR* const WeaponArrow[] = { TEXT("/Game/Weapons/Arrow/Arrow.Arrow"), TEXT("/Game/Weapons/Errow/Arrow.Arrow") };

	UStaticMesh* LoadFirstMesh(TArrayView<const TCHAR* const> Paths)
	{
		for (const TCHAR* Path : Paths)
		{
			// FindObject first so a missing candidate path does not log a load warning every spawn.
			if (UStaticMesh* SM = FindObject<UStaticMesh>(nullptr, Path)) { return SM; }
			if (FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(FString(Path))))
			{
				if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, Path)) { return SM; }
			}
		}
		return nullptr;
	}

	/** Rotation that turns a mesh so its local LocalX axis lies on +X and LocalZ on +Z. */
	FQuat AxesToXZ(const FVector& LocalX, const FVector& LocalZ)
	{
		return FRotationMatrix::MakeFromXZ(LocalX, LocalZ).ToQuat().Inverse();
	}

	/** Arrow mesh: tip at +Z (fletching at -Z); turned so the tip points along +X, the flight direction. */
	const FQuat ArrowMeshRot = AxesToXZ(FVector(0.f, 0.f, 1.f), FVector(1.f, 0.f, 0.f));
	/** Arrow length as a fraction of the figure height. */
	constexpr float ArrowSize = 0.4f;
}

/**
 * How a weapon mesh sits in a hand socket. Sockets on the Viking skeletons (made by script, editable in the
 * Skeleton Editor) follow one convention: +Z = where the shaft / blade points, +X = edge / facing / shooting side.
 * Each mesh says which of its local axes map onto those, where it is gripped, and how big it is.
 */
struct FTSCWeaponDef
{
	TArrayView<const TCHAR* const> Paths;
	FName Socket;
	FName FallbackBone;
	/** Local axis of the mesh that goes to the socket's +X / +Z. */
	FVector LocalX;
	FVector LocalZ;
	/** Grip point in mesh units (lands on the socket). */
	FVector Grip;
	/** Longest dimension as a fraction of the figure height (the imported meshes come in mixed scales). */
	float Size;
};

namespace
{
	// Measured from the meshes: sword blade -Y, edges +/-X; axe head -Y, edge +X, origin right under the head;
	// shield face (boss) -Y; bow limbs along Y, bending towards -X (the target side); staff up +Z.
	const FTSCWeaponDef DefSword{ WeaponSword, TEXT("weapon_r"), TEXT("Fist_R"), FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 0.03f, 0), 0.55f };
	const FTSCWeaponDef DefAxe{ WeaponAxe, TEXT("weapon_r"), TEXT("Fist_R"), FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 0.45f, 0), 0.45f };
	const FTSCWeaponDef DefShield{ WeaponShield, TEXT("shield_l"), TEXT("Fist_L"), FVector(0, -1, 0), FVector(0, 0, 1), FVector::ZeroVector, 0.4f };
	const FTSCWeaponDef DefBow{ WeaponBow, TEXT("bow_l"), TEXT("Fist_L"), FVector(-1, 0, 0), FVector(0, 1, 0), FVector::ZeroVector, 0.42f };
	const FTSCWeaponDef DefStaff{ WeaponStaff, TEXT("staff_r"), TEXT("Fist_R"), FVector(1, 0, 0), FVector(0, 0, 1), FVector::ZeroVector, 0.5f };
}

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

	if (BuildViking(Tunic))
	{
		bArmed = true;
		SetArmed(bRaider || Role == ETSCRole::Shield);
		return;
	}

	// Fallback (Viking character assets missing): figure made of primitives.
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

bool ATSCUnit::BuildViking(const FLinearColor& Tunic)
{
	// Torvald (the named shield-bearer) is the lord. Named heroes have a fixed sex (Sigrun the Healer is a woman),
	// everyone else (workers, raiders) is a man or a woman at random.
	const FVikingModel* Model = &VikingMale;
	if (bNamed)
	{
		Model = Role == ETSCRole::Shield ? &VikingLord : Role == ETSCRole::Healer ? &VikingFemale : &VikingMale;
	}
	else if (FMath::RandBool())
	{
		Model = &VikingFemale;
	}
	auto LoadMesh = [](const FVikingModel& M)
	{
		return LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), M.Folder, M.Mesh, M.Mesh));
	};
	USkeletalMesh* SkMesh = LoadMesh(*Model);
	if (!SkMesh && Model != &VikingMale)
	{
		Model = &VikingMale;
		SkMesh = LoadMesh(*Model);
	}
	if (!SkMesh) { return false; }

	const bool bRaider = Role == ETSCRole::Raider;
	const float S = bNamed ? 1.12f : 1.f;

	// Fit the imported mesh to the figure height, feet on the ground.
	const FBox Box = SkMesh->GetImportedBounds().GetBox();
	const float Scale = VikingHeight * S / FMath::Max(Box.Max.Z - Box.Min.Z, 1.f);

	Mesh = NewObject<USkeletalMeshComponent>(this);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetSkeletalMeshAsset(SkMesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh->SetupAttachment(Body);
	Mesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -Box.Min.Z * Scale), FRotator(0.f, VikingYaw, 0.f));
	Mesh->SetRelativeScale3D(FVector(Scale));
	Mesh->RegisterComponent();
	AddInstanceComponent(Mesh);

	// Per-slot colours: "Main" is the tunic (role / job colour), raiders get black hair.
	for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I)
	{
		const UMaterialInterface* Src = Mesh->GetMaterial(I);
		const FString SlotName = Src ? Src->GetName() : FString();
		UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(I);
		FLinearColor Col = MID ? MID->K2_GetVectorParameterValue(DiffuseParam) : FLinearColor::White;
		if (SlotName == TEXT("Main")) { TunicSlot = I; Col = Tunic; }
		else if (bRaider && SlotName == TEXT("Hair")) { Col = FLinearColor(0.05f, 0.04f, 0.03f); }
		MeshMIDs.Add(MID);
		MeshCols.Add(Col);
	}

	Anims.SetNum((int32)ETSCAnim::Num);
	for (int32 I = 0; I < (int32)ETSCAnim::Num; ++I)
	{
		const FString Name = FString(Model->AnimPrefix) + VikingAnimNames[I];
		Anims[I] = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), Model->Folder, *Name, *Name));
	}
	const float IdleLen = PlayAnim(ETSCAnim::Idle, true);
	if (IdleLen > 0.f)
	{
		// Desync idle loops so a crowd does not breathe in unison.
		Mesh->SetPosition(FMath::Fmod(AnimPhase, IdleLen), false);
	}

	// Weapons (Content/Weapons). Hunter: bow, healer: staff (always held). Fighters: shield + sword or axe, picked at
	// random; the lord (Torvald) and Ulf the Woodcutter always carry the axe. Shield and blade show only when armed.
	if (Role == ETSCRole::Hunter)
	{
		AddWeapon(DefBow, false);
	}
	else if (Role == ETSCRole::Healer)
	{
		AddWeapon(DefStaff, false);
	}
	else
	{
		const bool bAxe = Role == ETSCRole::Shield || Role == ETSCRole::Woodcutter || FMath::RandBool();
		if (!bAxe || !AddWeapon(DefAxe, true))
		{
			AddWeapon(DefSword, true);
		}
		AddWeapon(DefShield, true);
	}
	return true;
}

UStaticMeshComponent* ATSCUnit::AddWeapon(const FTSCWeaponDef& Def, bool bArmedOnly)
{
	UStaticMesh* SM = LoadFirstMesh(Def.Paths);
	if (!SM || !Mesh) { return nullptr; }
	const FName Socket = Mesh->DoesSocketExist(Def.Socket) ? Def.Socket : Def.FallbackBone;

	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(SM);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetupAttachment(Mesh, Socket);
	Comp->RegisterComponent();
	AddInstanceComponent(Comp);

	// Orient by the socket convention, size it against the figure, and put the grip point on the socket.
	// The skeleton bones carry a x100 scale, so sizes are set in world space, offsets divided by the socket scale.
	const float MeshLen = FMath::Max(SM->GetBoundingBox().GetSize().GetMax(), KINDA_SMALL_NUMBER);
	const float WorldScale = Def.Size * VikingHeight * (bNamed ? 1.12f : 1.f) / MeshLen;
	const float SocketScale = FMath::Max(Mesh->GetSocketTransform(Socket, RTS_World).GetScale3D().GetMax(), KINDA_SMALL_NUMBER);
	const FQuat Rot = AxesToXZ(Def.LocalX, Def.LocalZ);
	Comp->SetRelativeRotation(Rot);
	Comp->SetWorldScale3D(FVector(WorldScale));
	Comp->SetRelativeLocation(-Rot.RotateVector(Def.Grip) * WorldScale / SocketScale);
	AddTintedMaterials(Comp);
	if (bArmedOnly)
	{
		// Visible for now; BuildFigure's SetArmed() hides it for unarmed units.
		ArmedItems.Add(Comp);
	}
	else
	{
		HeldItem = Comp;
	}
	return Comp;
}

void ATSCUnit::AddTintedMaterials(UMeshComponent* Comp)
{
	// Each slot gets a MID so Refresh can blend its colour to the Ashen look.
	for (int32 I = 0; I < Comp->GetNumMaterials(); ++I)
	{
		UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(I);
		MeshMIDs.Add(MID);
		MeshCols.Add(MID ? MID->K2_GetVectorParameterValue(DiffuseParam) : FLinearColor::White);
	}
}

bool ATSCUnit::FireArrow(const FVector& To)
{
	if (!Mesh) { return false; }
	UStaticMesh* SM = LoadFirstMesh(WeaponArrow);
	if (!SM) { return false; }

	// Reuse a finished arrow, otherwise make a new one (world-space, not attached to the moving hunter).
	int32 Idx = Arrows.IndexOfByPredicate([](const FTSCArrowFlight& A) { return A.T >= 1.f; });
	if (Idx == INDEX_NONE)
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetStaticMesh(SM);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetupAttachment(Root);
		Comp->SetUsingAbsoluteLocation(true);
		Comp->SetUsingAbsoluteRotation(true);
		Comp->SetUsingAbsoluteScale(true);
		Comp->RegisterComponent();
		AddInstanceComponent(Comp);
		AddTintedMaterials(Comp);
		Refresh();
		ArrowComps.Add(Comp);
		Idx = Arrows.AddDefaulted();
	}
	FTSCArrowFlight& A = Arrows[Idx];
	A.From = HeldItem ? HeldItem->GetComponentLocation() : GetActorLocation() + FVector(0.f, 0.f, 90.f);
	A.To = To;
	A.T = 0.f;
	A.Dur = FMath::Clamp(FVector::Dist(A.From, To) / 2400.f, 0.15f, 0.45f);
	UStaticMeshComponent* Comp = ArrowComps[Idx];
	Comp->SetWorldScale3D(FVector(ArrowSize * VikingHeight / FMath::Max(SM->GetBoundingBox().GetSize().GetMax(), KINDA_SMALL_NUMBER)));
	Comp->SetVisibility(true);
	UpdateArrows(0.f);
	return true;
}

void ATSCUnit::UpdateArrows(float Dt)
{
	for (int32 I = 0; I < Arrows.Num(); ++I)
	{
		FTSCArrowFlight& A = Arrows[I];
		UStaticMeshComponent* Comp = ArrowComps[I];
		if (A.T >= 1.f || !Comp) { continue; }
		A.T = FMath::Min(1.f, A.T + Dt / A.Dur);
		if (A.T >= 1.f)
		{
			Comp->SetVisibility(false);
			continue;
		}
		// Shallow arc; the arrow points along its flight direction.
		const float Arc = FVector::Dist(A.From, A.To) * 0.06f;
		const FVector P = FMath::Lerp(A.From, A.To, A.T) + FVector(0.f, 0.f, Arc * 4.f * A.T * (1.f - A.T));
		const FVector Dir = (A.To - A.From) + FVector(0.f, 0.f, Arc * 4.f * (1.f - 2.f * A.T));
		Comp->SetWorldLocationAndRotation(P, Dir.Rotation().Quaternion() * ArrowMeshRot);
	}
}

float ATSCUnit::PlayAnim(ETSCAnim Id, bool bLoop, float Rate)
{
	UAnimSequence* A = Anims.IsValidIndex((int32)Id) ? Anims[(int32)Id].Get() : nullptr;
	if (!Mesh || !A) { return 0.f; }
	if (A != CurAnim || !bLoop)
	{
		CurAnim = A;
		Mesh->PlayAnimation(A, bLoop);
	}
	Mesh->SetPlayRate(Rate);
	return A->GetPlayLength();
}

void ATSCUnit::StartSwing(float ArmTime, float Window, ETSCAnim Clip)
{
	SwingTimer = ArmTime;
	if (!Mesh) { return; }
	const UAnimSequence* A = Anims.IsValidIndex((int32)Clip) ? Anims[(int32)Clip].Get() : nullptr;
	if (!A) { return; }
	const float Len = A->GetPlayLength();
	const float Rate = Window > 0.f ? FMath::Max(1.f, Len / Window) : 1.f;
	PlayAnim(Clip, false, Rate);
	OneShotTimer = Len / Rate;
}

void ATSCUnit::Refresh()
{
	Super::Refresh();
	const float A = FMath::SmoothStep(0.f, 1.f, SagaAlpha);
	for (int32 I = 0; I < MeshMIDs.Num(); ++I)
	{
		if (MeshMIDs[I])
		{
			MeshMIDs[I]->SetVectorParameterValue(DiffuseParam, FMath::Lerp(MeshCols[I], Ashify(MeshCols[I]), A));
		}
	}
}

void ATSCUnit::SetArmed(bool bInArmed)
{
	if (bArmed == bInArmed) { return; }
	bArmed = bInArmed;
	for (int32 I : ArmedParts) { SetShown(I, bArmed); }
	for (UStaticMeshComponent* C : ArmedItems) { if (C) { C->SetVisibility(bArmed); } }
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
	if (Role == ETSCRole::Worker && MeshCols.IsValidIndex(TunicSlot))
	{
		MeshCols[TunicSlot] = Job == ETSCJob::Wood ? FLinearColor(0.36f, 0.17f, 0.07f)
			: Job == ETSCJob::Food ? FLinearColor(0.17f, 0.30f, 0.09f) : FLinearColor(0.20f, 0.22f, 0.27f);
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
	else if (Mesh && OneShotTimer <= 0.f && !bDowned)
	{
		OneShotTimer = PlayAnim(ETSCAnim::Hit, false);
	}
}

void ATSCUnit::Kill()
{
	if (bDead) { return; }
	HP = 0.f;
	bDead = true;
	DeathTimer = 0.f;
	Need = ETSCNeed::None;
	if (Mesh)
	{
		OneShotTimer = 0.f;
		PlayAnim(ETSCAnim::Death, false);
	}
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
	UpdateArrows(Dt); // arrows already in the air keep flying even if the hunter falls
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
			if (!Mesh) // the Viking mesh falls with its Death clip
			{
				Body->SetRelativeRotation(FRotator(0.f, 0.f, FMath::Min(DeathTimer * 260.f, 90.f)));
			}
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
		if (!Mesh)
		{
			Body->SetRelativeRotation(FRotator(0.f, 0.f, 82.f));
		}
		else if (Anims[(int32)ETSCAnim::Death] && CurAnim != Anims[(int32)ETSCAnim::Death])
		{
			// Lying down: the Death clip holds its last (lying) pose.
			OneShotTimer = 0.f;
			PlayAnim(ETSCAnim::Death, false);
		}
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
					StartSwing(0.35f, AttackTimer, AttackRange > 300.f ? ETSCAnim::Shoot : Role == ETSCRole::Healer ? ETSCAnim::Punch : ETSCAnim::Slash);
					T->Hurt(Damage);
					if (AttackRange > 300.f)
					{
						// An arrow flies from the bow; the HUD tracer line is the fallback without the Viking / arrow assets.
						const FVector Hit = T->GetActorLocation() + FVector(0.f, 0.f, T->Height * 0.6f);
						if (!FireArrow(Hit))
						{
							GM->AddTracer(GetActorLocation() + FVector(0.f, 0.f, 90.f), Hit, FLinearColor(1.f, 0.9f, 0.6f));
						}
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
			if (WorkPhase == 0 && SwingTimer <= 0.f && OneShotTimer <= 0.f)
			{
				// Chopping (axe) or gathering (picking up).
				const bool bChop = Role == ETSCRole::Woodcutter || (Role == ETSCRole::Worker && Job == ETSCJob::Wood);
				StartSwing(0.6f, 0.f, bChop ? ETSCAnim::Slash : ETSCAnim::PickUp);
			}
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
				StartSwing(0.35f, AttackInterval, ETSCAnim::Slash);
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

	if (Mesh)
	{
		// Skinned Viking: one-shot clips (attack, work, hit) play out unless the unit starts moving, then locomotion.
		if (bMoving) { OneShotTimer = 0.f; }
		OneShotTimer -= Dt;
		if (OneShotTimer > 0.f) { return; }
		if (!bMoving)
		{
			PlayAnim(ETSCAnim::Idle, true);
			return;
		}
		const bool bRun = Target.IsValid() || Order == ETSCOrder::Assault || Order == ETSCOrder::Shelter;
		const bool bCarry = Order == ETSCOrder::Work && WorkPhase == 1 && Role != ETSCRole::Hunter;
		PlayAnim(bCarry ? (bRun ? ETSCAnim::RunCarry : ETSCAnim::WalkCarry) : (bRun ? ETSCAnim::Run : ETSCAnim::Walk), true);
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
