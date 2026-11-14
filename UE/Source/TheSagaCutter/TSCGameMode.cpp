// The Saga Cutter - game jam project.

#include "TSCGameMode.h"

#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "ParticleEmitterInstances.h"
#include "ParticleHelper.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/UObjectIterator.h"
#include "SceneInterface.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "TSCColonyCameraPawn.h"
#include "UObject/ConstructorHelpers.h"
#include "TSCHUD.h"
#include "TSCLoc.h"
#include "TSCOcean.h"
#include "TSCTerrain.h"
#include "TSCPlayerController.h"
#include "TSCPrimActor.h"
#include "TheSagaCutter.h"

using namespace TSC;

namespace
{
	const int32 GateX = 8, GateY = 11;          // gate cells (8,11) and (8,12)
	const int32 TreeX = 9, TreeY = 10;          // the spruce by the gate
	const FIntRect ForestRect(-6, -4, 8, 28);   // max exclusive
	const FIntRect GardenRect(15, 14, 21, 19);
	const FIntRect GroveRect(10, 19, 21, 27);
	const FIntRect DockRect(26, 9, 34, 15);

	// The palisade: a superellipse in cell units (between a circle and a square) with its west apex on the gate,
	// running from the north shore round the west to the south shore. Open to the water in the east.
	const float WallCX = 19.f, WallCY = 12.f, WallA = 11.f, WallB = 14.f, WallN = 2.8f, WallEndX = 27.55f;
	float SignedPow(float V, float E) { return FMath::Sign(V) * FMath::Pow(FMath::Abs(V), E); }
	FVector2D WallCellAt(float T)
	{
		return FVector2D(WallCX - WallA * SignedPow(FMath::Cos(T), 2.f / WallN), WallCY + WallB * SignedPow(FMath::Sin(T), 2.f / WallN));
	}
	/** < 1 inside the wall curve. */
	float WallShape(float Cx, float Cy) { return FMath::Pow(FMath::Abs(Cx - WallCX) / WallA, WallN) + FMath::Pow(FMath::Abs(Cy - WallCY) / WallB, WallN); }
	/** Curve parameter of the south shore end (the north end is its negative). */
	float WallEndT() { return FMath::Acos(-FMath::Pow((WallEndX - WallCX) / WallA, WallN * 0.5f)); }

	uint32 Hash2(int32 X, int32 Y) { return HashCombine(GetTypeHash(X * 73856093), GetTypeHash(Y * 19349663)); }
	float Rand01(int32 X, int32 Y, int32 Salt = 0) { return (Hash2(X + Salt * 31, Y - Salt * 17) % 10000) / 10000.f; }

	// Feeds the level's wind (the WindDirectionalSource that also drives the banner cloth) into MPC_Wind, which
	// M_PineTree reads for its sway. The phase is integrated here so a changing wind speed never makes the trees jump.
	void UpdatePineWind(UWorld* World, float Dt)
	{
		static UMaterialParameterCollection* Wind = []
		{
			UMaterialParameterCollection* Loaded = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Trees/MPC_Wind.MPC_Wind"));
			if (Loaded) { Loaded->AddToRoot(); }
			return Loaded;
		}();
		if (!Wind || !World || !World->Scene) { return; }

		FVector Dir = FVector::ForwardVector;
		float Speed = 0.f, MinGust = 0.f, MaxGust = 0.f;
		World->Scene->GetWindParameters_GameThread(FVector::ZeroVector, Dir, Speed, MinGust, MaxGust);

		static float Phase = 0.f;
		static float SmoothSpeed = 1.f;
		static FVector SmoothDir = FVector::ForwardVector;
		SmoothSpeed = FMath::FInterpTo(SmoothSpeed, Speed, Dt, 1.5f);
		if (!Dir.IsNearlyZero()) { SmoothDir = FMath::VInterpTo(SmoothDir, Dir.GetSafeNormal(), Dt, 1.5f); }
		Phase = FMath::Fmod(Phase + Dt * (0.9f + 0.45f * SmoothSpeed), 2000.f * UE_PI);

		UKismetMaterialLibrary::SetScalarParameterValue(World, Wind, TEXT("WindPhase"), Phase);
		UKismetMaterialLibrary::SetScalarParameterValue(World, Wind, TEXT("WindAmp"), FMath::Clamp(0.45f + 0.3f * SmoothSpeed, 0.3f, 2.f));
		UKismetMaterialLibrary::SetVectorParameterValue(World, Wind, TEXT("WindDir"), FLinearColor(SmoothDir.X, SmoothDir.Y, 0.f, 0.f));

		// Chimney smoke: the plume rises straight up and the current wind carries every live particle along, so the
		// whole column bends together when the wind shifts. Gusts come from two slow sines on top of the level's wind.
		static float GustTime = 0.f;
		GustTime += Dt;
		const float Gust = 1.f + 0.35f * FMath::Sin(GustTime * 0.37f) + 0.2f * FMath::Sin(GustTime * 1.13f + 1.7f);
		const FVector Side(-SmoothDir.Y, SmoothDir.X, 0.f);
		const FVector Drift = (SmoothDir * (75.f * SmoothSpeed * Gust) + Side * (28.f * SmoothSpeed * FMath::Sin(GustTime * 0.23f))) * Dt;
		for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
		{
			if (It->GetWorld() != World || !It->ComponentHasTag(TEXT("TSC_ChimneySmoke"))) { continue; }
			It->SetWorldRotation(FRotator::ZeroRotator);
			It->ForceAsyncWorkCompletion(UParticleSystemComponent::STALL);
			for (FParticleEmitterInstance* Inst : It->EmitterInstances)
			{
				if (!Inst || !Inst->ParticleData || !Inst->ParticleIndices) { continue; }
				for (int32 I = 0; I < Inst->ActiveParticles; ++I)
				{
					DECLARE_PARTICLE(Particle, Inst->ParticleData + Inst->ParticleStride * Inst->ParticleIndices[I]);
					Particle.Location += Drift;
					Particle.OldLocation += Drift;
				}
			}
		}
	}

	// Prologue beats (seconds): landfall, clearing, raising, the forest wakes, hold and fade.
	const float ProTLand = 8.f, ProTClear = 20.f, ProTRaise = 34.f, ProTWake = 45.f, ProTEnd = 51.f;
	const float ProShipRun = 2400.f;
	/** Seconds the sail takes to come down once the ship has stopped; the crew lands after that. */
	const float ProSailDrop = 2.f;
	/** The road fence is put up one piece after another over this many seconds, starting once the village wall is done. */
	const float ProFenceSpan = 8.f;
	const float DefaultCamArm = 16500.f;
	TAutoConsoleVariable<int32> CVarPrologue(TEXT("tsc.Prologue"), 1,
		TEXT("0: the title screen shows the finished village instead of the landfall prologue (also -NoPrologue)."));
}

ATSCGameMode::ATSCGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ATSCColonyCameraPawn::StaticClass();
	PlayerControllerClass = ATSCPlayerController::StaticClass();
	HUDClass = ATSCHUD::StaticClass();

	static ConstructorHelpers::FObjectFinder<USoundBase> MenuCue(TEXT("/Game/Vikings/Audio/Music/SC_Menu.SC_Menu"));
	static ConstructorHelpers::FObjectFinder<USoundBase> DayCue(TEXT("/Game/Vikings/Audio/Music/SC_MainTheme.SC_MainTheme"));
	static ConstructorHelpers::FObjectFinder<USoundBase> NightCue(TEXT("/Game/Vikings/Audio/Music/SC_Action.SC_Action"));
	static ConstructorHelpers::FObjectFinder<USoundBase> VictoryCue(TEXT("/Game/Vikings/Audio/Music/SC_Victory.SC_Victory"));
	MenuMusic = MenuCue.Object;
	DayMusic = DayCue.Object;
	NightMusic = NightCue.Object;
	VictoryMusic = VictoryCue.Object;
}

void ATSCGameMode::PlayMusic(USoundBase* Sound)
{
	if (Sound == CurrentMusic && MusicComp && MusicComp->IsPlaying()) { return; }
	GetWorldTimerManager().ClearTimer(MusicTimer);
	if (MusicComp) { MusicComp->FadeOut(MusicFadeTime, 0.f); }  // auto-destroys when the fade ends
	MusicComp = nullptr;
	CurrentMusic = Sound;
	UE_LOG(LogSaga, Display, TEXT("[Saga] Music: %s"), Sound ? *Sound->GetName() : TEXT("none"));
	if (!Sound) { return; }
	MusicComp = UGameplayStatics::SpawnSound2D(this, Sound, MusicVolume, 1.f, 0.f, nullptr, false, true);
	if (MusicComp)
	{
		MusicComp->bIsUISound = true;  // keeps playing while the game is paused
		MusicComp->FadeIn(MusicFadeTime, MusicVolume);
	}
}

// =====================================================================================================================
// Grid
// =====================================================================================================================

FVector ATSCGameMode::CellCorner(int32 Cx, int32 Cy) const { return FVector((Cx - GW / 2) * CELL, (Cy - GH / 2) * CELL, 0.f); }
FVector ATSCGameMode::CellCenter(int32 Cx, int32 Cy) const { return CellCorner(Cx, Cy) + FVector(CELL * 0.5f, CELL * 0.5f, 0.f); }
FVector ATSCGameMode::CellF(float Cx, float Cy) const { return FVector((Cx - GW / 2) * CELL, (Cy - GH / 2) * CELL, 0.f); }

FIntPoint ATSCGameMode::WorldToCell(const FVector& P) const
{
	return FIntPoint(FMath::FloorToInt(P.X / CELL) + GW / 2, FMath::FloorToInt(P.Y / CELL) + GH / 2);
}

void ATSCGameMode::SetBlocked(int32 Cx, int32 Cy, bool bBlock)
{
	if (InGrid(Cx, Cy))
	{
		Blocked[Cy * GW + Cx] = bBlock ? 1 : 0;
		++GridVersion;
	}
}

void ATSCGameMode::BlockRect(int32 Cx, int32 Cy, int32 W, int32 H, bool bBlock)
{
	for (int32 X = Cx; X < Cx + W; ++X) { for (int32 Y = Cy; Y < Cy + H; ++Y) { SetBlocked(X, Y, bBlock); } }
}

FIntPoint ATSCGameMode::NearestFree(FIntPoint C) const
{
	C.X = FMath::Clamp(C.X, 0, GW - 1);
	C.Y = FMath::Clamp(C.Y, 0, GH - 1);
	if (!IsBlocked(C.X, C.Y)) { return C; }
	for (int32 R = 1; R < 8; ++R)
	{
		FIntPoint Best = C;
		int32 BestD = MAX_int32;
		for (int32 Dx = -R; Dx <= R; ++Dx)
		{
			for (int32 Dy = -R; Dy <= R; ++Dy)
			{
				if (!IsBlocked(C.X + Dx, C.Y + Dy) && Dx * Dx + Dy * Dy < BestD)
				{
					BestD = Dx * Dx + Dy * Dy;
					Best = FIntPoint(C.X + Dx, C.Y + Dy);
				}
			}
		}
		if (BestD != MAX_int32) { return Best; }
	}
	return C;
}

bool ATSCGameMode::LineFree(const FVector& A, const FVector& B) const
{
	const float Len = FVector::Dist2D(A, B);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Len / 40.f));
	const FVector Side = FVector(-(B - A).Y, (B - A).X, 0.f).GetSafeNormal() * 34.f;
	for (int32 I = 0; I <= Steps; ++I)
	{
		const FVector P = FMath::Lerp(A, B, I / (float)Steps);
		for (int32 S = -1; S <= 1; ++S)
		{
			const FIntPoint C = WorldToCell(P + Side * S);
			if (IsBlocked(C.X, C.Y)) { return false; }
		}
	}
	return true;
}

bool ATSCGameMode::FindPath(const FVector& From, const FVector& To, TArray<FVector>& Out) const
{
	Out.Reset();
	const FIntPoint RawGoal = WorldToCell(To);
	const FIntPoint Start = NearestFree(WorldToCell(From));
	const FIntPoint Goal = NearestFree(RawGoal);
	const bool bGoalExact = Goal == RawGoal;

	TArray<int32> Parent;
	Parent.Init(-2, GW * GH);
	TArray<int32> Queue;
	Queue.Reserve(GW * GH);
	const int32 StartIdx = Start.Y * GW + Start.X;
	const int32 GoalIdx = Goal.Y * GW + Goal.X;
	Parent[StartIdx] = -1;
	Queue.Add(StartIdx);
	static const int32 Dx[4] = { 1, -1, 0, 0 };
	static const int32 Dy[4] = { 0, 0, 1, -1 };
	for (int32 Head = 0; Head < Queue.Num() && Parent[GoalIdx] == -2; ++Head)
	{
		const int32 Cur = Queue[Head];
		const int32 Cx = Cur % GW, Cy = Cur / GW;
		for (int32 D = 0; D < 4; ++D)
		{
			const int32 Nx = Cx + Dx[D], Ny = Cy + Dy[D];
			if (IsBlocked(Nx, Ny)) { continue; }
			const int32 N = Ny * GW + Nx;
			if (Parent[N] != -2) { continue; }
			Parent[N] = Cur;
			Queue.Add(N);
		}
	}
	if (Parent[GoalIdx] == -2)
	{
		return false;
	}
	TArray<FVector> Cells;
	for (int32 Cur = GoalIdx; Cur != -1; Cur = Parent[Cur])
	{
		Cells.Insert(CellCenter(Cur % GW, Cur / GW), 0);
	}
	if (bGoalExact) { Cells.Add(FVector(To.X, To.Y, 0.f)); }

	// String pulling: skip waypoints while the straight line stays on free cells.
	FVector Cur(From.X, From.Y, 0.f);
	const FIntPoint FromCell = WorldToCell(From);
	if (IsBlocked(FromCell.X, FromCell.Y)) { Out.Add(Cells[0]); Cur = Cells[0]; }
	int32 I = 0;
	while (I < Cells.Num())
	{
		int32 Far = I;
		for (int32 J = Cells.Num() - 1; J > I; --J)
		{
			if (LineFree(Cur, Cells[J])) { Far = J; break; }
		}
		Out.Add(Cells[Far]);
		Cur = Cells[Far];
		I = Far + 1;
	}
	return true;
}

bool ATSCGameMode::CanReach(const ATSCUnit* A, const ATSCUnit* B) const
{
	// outside = beyond the curve on the forest side (the open shore end counts as inside)
	auto Outside = [](const FVector& P)
	{
		const float Cx = P.X / CELL + GW / 2, Cy = P.Y / CELL + GH / 2;
		return Cx < WallCX && WallShape(Cx, Cy) > 1.f;
	};
	const bool bAWest = Outside(A->GetActorLocation());
	const bool bBWest = Outside(B->GetActorLocation());
	if (bAWest == bBWest) { return true; }
	return (!bGateClosed || bGateBroken) && !IsBlocked(TreeX, TreeY + 1);
}

bool ATSCGameMode::IsInsideSettlement(const FVector& P) const
{
	const float Cx = P.X / CELL + GW / 2, Cy = P.Y / CELL + GH / 2;
	return Cx < WallEndX && WallShape(Cx, Cy) < 1.f;
}

// =====================================================================================================================
// World
// =====================================================================================================================

ATSCPrimActor* ATSCGameMode::SpawnPrim(const FVector& Loc, int32 Region)
{
	ATSCPrimActor* A = GetWorld()->SpawnActor<ATSCPrimActor>(ATSCPrimActor::StaticClass(), Loc, FRotator::ZeroRotator);
	A->Region = Region;
	Prims.Add(A);
	return A;
}

ATSCUnit* ATSCGameMode::SpawnUnit(ETSCRole InRole, const FString& Name, const FVector& Loc)
{
	ATSCUnit* U = GetWorld()->SpawnActor<ATSCUnit>(ATSCUnit::StaticClass(), FVector(Loc.X, Loc.Y, 0.f), FRotator::ZeroRotator);
	U->Init(this, InRole, Name);
	Units.Add(U);
	return U;
}

// Ultra Dynamic Sky (Blueprint actor, /Game/ThirdParty/UltraDynamicSky): placed in the level or spawned; optional.
// The level owns it, so a weak pointer is enough.
static TWeakObjectPtr<AActor> GTSCSky;
static float GTSCSkyTime = 1100.f;
static TWeakObjectPtr<USkyLightComponent> GTSCSkyLight; // owned by the atmosphere actor
static FQuat GTSCDaySun = FRotator(-42.f, -22.f, 0.f).Quaternion(); // where the sun stands on its daily arc (eased)
static float GTSCSkyDrift = 0.f, GTSCShadowDrift = 0.f; // extra cloud travel earned while the game is fast-forwarded
static void TSCUpdateSky(UDirectionalLightComponent* Sun, float AshAmount, float K, bool bNight, float DayF, float Thick, float Dt, float SpeedMul);
ATSCUnit* ATSCGameMode::SpawnWildlife(ETSCRole InRole, const FVector& Loc, float Radius, int32 Region)
{
	ATSCUnit* U = GetWorld()->SpawnActor<ATSCUnit>(ATSCUnit::StaticClass(), FVector(Loc.X, Loc.Y, 0.f), FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
	U->Init(this, InRole, FString());
	U->Region = Region;
	// Forest animals stay outside the palisade, livestock inside it; the stag and the grove fox live where they were put.
	switch (InRole)
	{
	case ETSCRole::Deer: case ETSCRole::Wolf: case ETSCRole::Stag: case ETSCRole::Fox: U->Zone = ETSCZone::Outside; break; // all forest animals
	case ETSCRole::Cow: case ETSCRole::Bull: U->Zone = ETSCZone::Inside; break;
	case ETSCRole::Husky: if (Radius > 0.f && IsInsideSettlement(Loc)) { U->Zone = ETSCZone::Inside; } break; // the village dog

	default: break;
	}
	U->OrderWander(Loc, Radius);
	// Livestock and the village dog (a husky that roams instead of following) arrive only after the village is built.
	if (InRole == ETSCRole::Cow || InRole == ETSCRole::Bull || (InRole == ETSCRole::Husky && Radius > 0.f)) { FarmAnimals.Add(U); }
	Wildlife.Add(U);
	Prims.Add(U); // takes part in the Living / Ashen transition of its region
	return U;
}

void ATSCGameMode::SpawnAllWildlife()
{
	// The stag lives in the forest, outside the palisade (the grove by the stag stone is inside the wall).
	SpawnWildlife(ETSCRole::Stag, CellF(2.0f, 21.5f), 320.f, TSCRegion::Forest);
	// Deer graze in clearings of the western forest (north and south of the road).
	SpawnWildlife(ETSCRole::Deer, CellF(3.f, 6.f), 350.f, TSCRegion::Forest);
	SpawnWildlife(ETSCRole::Deer, CellF(3.6f, 6.8f), 350.f, TSCRegion::Forest);
	SpawnWildlife(ETSCRole::Deer, CellF(2.6f, 17.5f), 350.f, TSCRegion::Forest);
	// A wolf pack prowls the deep forest, far from the wall.
	SpawnWildlife(ETSCRole::Wolf, CellF(-2.f, 4.5f), 420.f, TSCRegion::Forest);
	SpawnWildlife(ETSCRole::Wolf, CellF(-2.6f, 5.2f), 420.f, TSCRegion::Forest);
	SpawnWildlife(ETSCRole::Wolf, CellF(-1.4f, 5.6f), 420.f, TSCRegion::Forest);
	// Foxes sneak along the forest edges: north of the clearing and behind the grove.
	SpawnWildlife(ETSCRole::Fox, CellF(6.4f, 2.2f), 300.f, TSCRegion::Forest);
	SpawnWildlife(ETSCRole::Fox, CellF(10.8f, 25.6f), 300.f, TSCRegion::Grove);
	// Village livestock grazes in the pasture on the right (east / south-east) side of the village, inside the palisade:
	// the free ground between the south-east house (23,15) and the wall, clear of the garden and the grove. It never leaves the wall.
	SpawnWildlife(ETSCRole::Cow, CellF(23.0f, 18.9f), 220.f, TSCRegion::None);
	SpawnWildlife(ETSCRole::Cow, CellF(24.8f, 19.8f), 220.f, TSCRegion::None);
	SpawnWildlife(ETSCRole::Bull, CellF(23.9f, 20.9f), 220.f, TSCRegion::None);
	// The village dog runs about inside the palisade, between the hall and the pasture, and plays with the jarl.
	SpawnWildlife(ETSCRole::Husky, CellF(13.0f, 14.6f), 450.f, TSCRegion::None);
	// Hakon's husky trots after its master.
	if (Hunter)
	{
		ATSCUnit* Dog = SpawnWildlife(ETSCRole::Husky, Hunter->GetActorLocation() + FVector(-110.f, 60.f, 0.f), 0.f, TSCRegion::None);
		Dog->OrderFollow(Hunter);
		HunterDog = Dog;
	}
}

void ATSCGameMode::BuildAtmosphere()
{
	AtmoActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	USceneComponent* AtmoRoot = NewObject<USceneComponent>(AtmoActor);
	AtmoActor->SetRootComponent(AtmoRoot);
	AtmoRoot->RegisterComponent();

	auto MakeLight = [&](const FRotator& Rot, bool bShadows)
	{
		UDirectionalLightComponent* L = NewObject<UDirectionalLightComponent>(AtmoActor);
		L->SetMobility(EComponentMobility::Movable);
		L->SetupAttachment(AtmoRoot);
		L->SetCastShadows(bShadows);
		L->RegisterComponent();
		L->SetWorldRotation(Rot);
		return L;
	};
	Sun = MakeLight(FRotator(-42.f, -22.f, 0.f), true);
	Sun->SetAtmosphereSunLight(false);
	Sun->SetForwardShadingPriority(3);
	// ambient: a sky light that captures the sky dome live, so the bounce light follows the sky's colour through the day
	USkyLightComponent* Sky = NewObject<USkyLightComponent>(AtmoActor);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SetupAttachment(AtmoRoot);
	Sky->bRealTimeCapture = true;
	Sky->bLowerHemisphereIsBlack = false;
	Sky->SetCastShadows(false);
	Sky->RegisterComponent();
	GTSCSkyLight = Sky;

	Fog = NewObject<UExponentialHeightFogComponent>(AtmoActor);
	Fog->SetupAttachment(AtmoRoot);
	Fog->RegisterComponent();
	Fog->SetFogHeightFalloff(0.55f);
	Fog->SetStartDistance(2000.f);

	// Sky: use the Ultra Dynamic Sky actor placed in the level, or spawn one. The game keeps its own lights and fog;
	// the sky only follows the game's clock (see UpdateSky). Everything is optional: no asset, no sky.
	AActor* SkyActor = nullptr;
	GTSCSkyTime = 1100.f;
	GTSCSkyDrift = GTSCShadowDrift = 0.f;
	if (UClass* SkyClass = LoadClass<AActor>(nullptr, TEXT("/Game/ThirdParty/UltraDynamicSky/Blueprints/Ultra_Dynamic_Sky_BP.Ultra_Dynamic_Sky_BP_C")))
	{
		for (TActorIterator<AActor> It(GetWorld(), SkyClass); It; ++It) { SkyActor = *It; break; }
		if (!SkyActor) { SkyActor = GetWorld()->SpawnActor<AActor>(SkyClass, FVector::ZeroVector, FRotator::ZeroRotator); }
	}
	GTSCSky = SkyActor;
	if (SkyActor)
	{
		if (FBoolProperty* P = FindFProperty<FBoolProperty>(SkyActor->GetClass(), TEXT("Animate Day/Night Cycle"))) { P->SetPropertyValue_InContainer(SkyActor, false); }
		if (FBoolProperty* P = FindFProperty<FBoolProperty>(SkyActor->GetClass(), TEXT("Simulate Cloud Density Changes"))) { P->SetPropertyValue_InContainer(SkyActor, false); }
		Fog->SetFogCutoffDistance(60000.f); // keep the height fog off the sky dome
		TInlineComponentArray<UPrimitiveComponent*> SkyPrims(SkyActor);
		for (UPrimitiveComponent* Prim : SkyPrims) { Prim->SetCastShadow(false); }
	}

	PostFx = NewObject<UPostProcessComponent>(AtmoActor);
	PostFx->SetupAttachment(AtmoRoot);
	PostFx->bUnbound = true;
	PostFx->RegisterComponent();
	FPostProcessSettings& S = PostFx->Settings;
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = AEM_Manual;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = 0.f;
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	S.AutoExposureApplyPhysicalCameraExposure = false;
	S.bOverride_ColorSaturation = true;
	S.bOverride_ColorGain = true;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 1.1f;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.55f;
	UpdateAtmosphere(0.f);
}

void ATSCGameMode::BuildWorld()
{
	Blocked.Init(0, GW * GH);

	// ----- continuous ground and trail network -----
	{
		// The shore (a beach along a fractal waterline, X 1800 .. ShoreXAt(Y)) is built by the ocean.
		Ocean =GetWorld()->SpawnActor<ATSCOcean>(ATSCOcean::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
		Ocean->Build(CellF(28.f, 12.f).X, 0.f);
		Terrain = GetWorld()->SpawnActor<ATSCTerrain>(ATSCTerrain::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
		// Keep these routes in the same clearances as the original paths and through the gate opening.
		const TArray<TArray<FVector2D>> TrailCells = {
			{{-6, 12}, {1, 12.13}, {5, 11.89}, {8, 12}, {11.5, 12}, {12.9, 13.3}, {16.4, 13.5},
			 {19.7, 13.35}, {20.9, 12.3}, {23.5, 12}, {26.3, 12}},
			{{12.05, 12.5}, {13.5, 10.4}, {13.6, 6.8}, {13.7, 4.35}, {14.5, 3.6}, {16.1, 3.5}},
			{{14.6, 13.4}, {14.4, 15.2}, {14.56, 17.6}, {14.5, 19.6}},
			{{21.6, 14.65}, {21.45, 13.35}, {20.6, 11.8}, {20.5, 9.45}},
		};
		TArray<TArray<FVector2D>> TrailRoutes;
		for (const TArray<FVector2D>& Cells : TrailCells)
		{
			TArray<FVector2D>& Route = TrailRoutes.AddDefaulted_GetRef();
			for (const FVector2D& Cell : Cells) { Route.Add(FVector2D(CellF(Cell.X, Cell.Y))); }
		}
		Terrain->Build(Ocean, TrailRoutes);
	}
	// ----- palisade line (dense samples, north shore end to south shore end) -----
	TArray<FVector> WallLine;
	{
		const float TE = WallEndT();
		for (int32 I = 0; I <= 2400; ++I)
		{
			const FVector2D C = WallCellAt(FMath::Lerp(-TE, TE, I / 2400.f));
			WallLine.Add(CellF(C.X, C.Y));
		}
	}
	auto WallDist = [&WallLine](const FVector& P)
	{
		float D = BIG_NUMBER;
		for (int32 I = 0; I < WallLine.Num(); I += 8) { D = FMath::Min(D, (float)FVector::Dist2D(P, WallLine[I])); }
		return D;
	};

	// ----- forest (west) and the framing trees -----
	for (int32 X = -6; X < GW + 2; ++X)
	{
		for (int32 Y = -4; Y < GH + 4; ++Y)
		{
			const bool bForest = X <= 6;
			// framing trees outside the palisade (not on the shore side nor in front of the gate), grove trees inside it
			const float Shape = WallShape(X + 0.5f, Y + 0.5f);
			const bool bFrame = !bForest && X >= 8 && X <= 27 && Shape > 1.f && (X + 0.5f < WallCX || !InGrid(X, Y))
				&& !(X <= 9 && Y >= 9 && Y <= 14);
			const bool bGroveTree = X >= 10 && X <= 20 && Y >= 21 && Y <= GH && !(X >= 13 && X <= 17 && Y <= 22) && Shape < 1.f;
			if (!bForest && !bFrame && !bGroveTree) { continue; }
			if (X > 27) { UE_LOG(LogSaga, Warning, TEXT("[Saga] tree loop: X=%d Y=%d forest=%d frame=%d grove=%d"), X, Y, bForest, bFrame, bGroveTree); continue; }
			if (bForest && (Y == 11 || Y == 12)) { continue; } // forest road
			if (!bForest && WallDist(CellCenter(X, Y)) < 170.f) { continue; } // keep the palisade clear
			if (!bForest && (Y == -1 || Y == GH)) { continue; } // north/south log walls (placed in Map_Saga)
			const float Density = bForest ? (X >= 5 ? 0.42f : 0.62f) : bGroveTree ? 0.4f : 0.5f;
			if (Rand01(X, Y, 7) > Density)
			{
				if ((bForest || bGroveTree) && Rand01(X, Y, 9) < 0.22f && InGrid(X, Y))
				{
					ATSCPrimActor* M = SpawnPrim(CellCenter(X, Y), bForest ? TSCRegion::Forest : TSCRegion::Grove);
					BuildMushrooms(M, X * 31 + Y);
					M->Refresh();
				}
				continue;
			}
			const FVector Jitter((Rand01(X, Y, 4) - 0.5f) * 70.f, (Rand01(X, Y, 5) - 0.5f) * 70.f, 0.f);
			ATSCPrimActor* T = SpawnPrim(CellCenter(X, Y) + Jitter, bForest ? TSCRegion::Forest : bGroveTree ? TSCRegion::Grove : TSCRegion::None);
			BuildTree(T, X * 57 + Y * 13, true);
			T->Refresh();
			SetBlocked(X, Y, true);
		}
	}
	// glowing mushrooms in the stag grove (foraged for food)
	const FVector2D GroveShrooms[7] = { {12.5f, 20.4f}, {13.6f, 21.3f}, {14.2f, 19.6f}, {16.5f, 20.6f}, {17.5f, 21.4f}, {18.6f, 20.2f}, {11.4f, 19.5f} };
	for (int32 I = 0; I < 7; ++I)
	{
		ATSCPrimActor* M = SpawnPrim(CellF(GroveShrooms[I].X, GroveShrooms[I].Y), TSCRegion::Grove);
		BuildMushrooms(M, 100 + I);
		M->Refresh();
	}

	// ----- palisade and gate (west) -----
	// One rounded log wall along WallLine, broken by the gate: straight segments of about 3 m, each an SM_WoodenFence
	// section with the Ashen stone footing. The cells under the wall block the paths.
	{
		const float GateY0 = CellCorner(GateX, GateY).Y, GateY1 = CellCorner(GateX, GateY + 2).Y;
		TArray<FVector> Runs[2];
		for (const FVector& P : WallLine)
		{
			if (P.Y > GateY0 && P.Y < GateY1) { continue; }
			Runs[P.Y <= GateY0 ? 0 : 1].Add(P);
			for (const FVector& O : { FVector::ZeroVector, FVector(30.f, 0.f, 0.f), FVector(-30.f, 0.f, 0.f), FVector(0.f, 30.f, 0.f), FVector(0.f, -30.f, 0.f) })
			{
				const FIntPoint C = WorldToCell(P + O);
				SetBlocked(C.X, C.Y, true);
			}
		}
		Runs[0].Add(FVector(WallX(), GateY0, 0.f));
		Runs[1].Insert(FVector(WallX(), GateY1, 0.f), 0);
		int32 Seg = 0;
		for (const TArray<FVector>& Run : Runs)
		{
			float Len = 0.f;
			for (int32 I = 1; I < Run.Num(); ++I) { Len += FVector::Dist2D(Run[I - 1], Run[I]); }
			const int32 N = FMath::Max(1, FMath::RoundToInt(Len / 295.f));
			const float Step = Len / N;
			// cut the run into N straight segments of equal length
			TArray<FVector> Knots = { Run[0] };
			float Acc = 0.f, Next = Step;
			for (int32 I = 1; I < Run.Num(); ++I)
			{
				const float D = FVector::Dist2D(Run[I - 1], Run[I]);
				while (Knots.Num() < N && D > 0.f && Acc + D >= Next)
				{
					Knots.Add(FMath::Lerp(Run[I - 1], Run[I], (Next - Acc) / D));
					Next += Step;
				}
				Acc += D;
			}
			Knots.Add(Run.Last());
			for (int32 I = 0; I + 1 < Knots.Num(); ++I)
			{
				const FVector Dir = Knots[I + 1] - Knots[I];
				ATSCPrimActor* P = SpawnPrim(Knots[I]);
				P->SetActorRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)) - 90.f, 0.f));
				BuildPalisadeSegment(P, Dir.Size2D(), Seg++);
				P->Refresh();
				WallSegs.Add(P);
			}
		}
		// the gate and the road in front of it stay open
		SetBlocked(GateX, GateY, bGateClosed);
		SetBlocked(GateX, GateY + 1, bGateClosed);
		SetBlocked(GateX - 1, GateY, false);
		SetBlocked(GateX - 1, GateY + 1, false);
	}
	Gate = SpawnPrim(CellCorner(GateX, GateY));
	BuildGate(Gate, GateLeaves);
	Gate->Refresh();
	ProDressing.Add(Gate);

	// the spruce by the gate: a separate stump stands in for its Ashen version
	EventTree = SpawnPrim(CellCenter(TreeX, TreeY), TSCRegion::Forest);
	BuildTree(EventTree, 4242, false);
	EventTree->SetActorScale3D(FVector(1.25f, 1.25f, 1.32f));
	EventTree->SetActorRotation(FRotator::ZeroRotator);
	EventTree->Refresh();
	{
		ATSCPrimActor* Stump = SpawnPrim(CellCenter(TreeX, TreeY), TSCRegion::Forest);
		BuildStump(Stump);
		Stump->Refresh();
	}
	SetBlocked(TreeX, TreeY, true);

	// ----- constructions -----
	TArray<FBox2D> GroundExclusions;
	auto Place = [&](int32 X, int32 Y, int32 W, int32 H, int32 Region = 0)
	{
		ATSCPrimActor* A = SpawnPrim(CellCorner(X, Y), Region);
		BlockRect(X, Y, W, H);
		GroundExclusions.Emplace(FVector2D(CellCorner(X, Y)), FVector2D(CellCorner(X + W, Y + H)));
		A->bStepParts = true;  // lets the prologue raise it tile by tile; at full progress nothing changes
		ProBuildings.Add(A);
		return A;
	};
	{
		ATSCPrimActor* A = Place(14, 10, 6, 3); BuildLonghouse(A); A->Refresh();
		A = Place(10, 4, 3, 2); BuildSawmill(A); A->Refresh();
		A = Place(22, 9, 2, 2); BuildHealer(A); A->Refresh();
		const FIntPoint Houses[4] = { {10, 15}, {12, 17}, {21, 5}, {23, 15} };
		for (int32 I = 0; I < 4; ++I)
		{
			A = Place(Houses[I].X, Houses[I].Y, 2, 2);
			BuildHouse(A, I + 1);
			A->Refresh();
		}
		for (const FVector2D& L : { FVector2D(13.5f, 4.5f), FVector2D(10.2f, 6.4f) })
		{
			A = SpawnPrim(CellF(L.X, L.Y));
			BuildLogPile(A);
			A->Refresh();
			ProDressing.Add(A);
			const FIntPoint C = WorldToCell(CellF(L.X, L.Y));
			SetBlocked(C.X, C.Y, true);
		}
	}
	Garden = SpawnPrim(CellCorner(16, 15), TSCRegion::Garden);
	BuildGarden(Garden, 4, 3, CropParts);
	Garden->Refresh();
	ProDressing.Add(Garden);

	DockActor = SpawnPrim(CellCorner(26, 11), TSCRegion::Dock);
	BuildDock(DockActor, 6, 2, CrateParts);
	DockActor->Refresh();
	ProDressing.Add(DockActor);
	{
		ShipMooring = CellF(30.6f, 14.4f) + FVector(0.f, 0.f, -6.f);
		Longship = SpawnPrim(ShipMooring, TSCRegion::Dock);
		LongshipSail = BuildLongship(Longship);
		Longship->Refresh();
		SetSailHoisted(0.f); // moored: the sail is down (the prologue hoists it for the voyage)
	}
	// water is blocked except the dock and the shallows
	for (int32 X = 30; X < GW; ++X)
	{
		for (int32 Y = 0; Y < GH; ++Y) { if (!(Y == 11 || Y == 12) || X >= 32) { SetBlocked(X, Y, true); } }
	}
	RaiderBoat = SpawnPrim(CellF(40.f, 12.f) + FVector(0.f, 0.f, -4.f));
	BuildRaiderBoat(RaiderBoat);
	RaiderBoat->Refresh();
	RaiderBoat->SetActorHiddenInGame(true);

	// the sea owns the Z, pitch and roll of everything that floats
	Ocean->AddFloater(Longship, 300.f, 55.f, -6.f, 420.f);
	Ocean->AddFloater(RaiderBoat, 190.f, 45.f, -4.f, 300.f);
	{
		const FVector2D BarrelCells[3] = { FVector2D(29.3f, 9.6f), FVector2D(30.4f, 15.9f), FVector2D(32.2f, 10.4f) };
		const FVector2D WoodCells[4] = { FVector2D(29.8f, 10.3f), FVector2D(31.0f, 13.4f), FVector2D(32.6f, 12.6f), FVector2D(30.2f, 8.4f) };
		for (int32 I = 0; I < 3; ++I)
		{
			ATSCPrimActor* A = SpawnPrim(CellF(BarrelCells[I].X, BarrelCells[I].Y), TSCRegion::Dock);
			BuildBarrel(A);
			A->Refresh();
			A->SetActorRotation(FRotator(0.f, Rand01(I, 7) * 360.f, 0.f));
			Flotsam.Add(A);
			ProDressing.Add(A);
			Ocean->AddFloater(A, 40.f, 22.f, 0.f, 0.f, true);
		}
		for (int32 I = 0; I < 4; ++I)
		{
			ATSCPrimActor* A = SpawnPrim(CellF(WoodCells[I].X, WoodCells[I].Y), TSCRegion::Dock);
			BuildDriftwood(A, I);
			A->Refresh();
			A->SetActorRotation(FRotator(0.f, Rand01(I, 7) * 360.f, 0.f));
			Flotsam.Add(A);
			Ocean->AddFloater(A, 80.f, 14.f, 0.f, 0.f, true);
		}
	}

	HammerStone = SpawnPrim(CellCenter(16, 2));
	BuildRuneStone(HammerStone, true);
	HammerStone->Refresh();
	SetBlocked(16, 2, true);
	StagStone = SpawnPrim(CellCenter(15, 21), TSCRegion::Grove);
	BuildRuneStone(StagStone, false);
	StagStone->Refresh();
	SetBlocked(15, 21, true);

	const FIntPoint Rocks[8] = { {26, 8}, {27, 16}, {26, 19}, {27, 4}, {26, 2}, {27, 21}, {25, 22}, {24, 1} };
	for (int32 I = 0; I < 8; ++I)
	{
		ATSCPrimActor* R = SpawnPrim(CellCenter(Rocks[I].X, Rocks[I].Y) + FVector(0.f, 0.f, 4.f));
		BuildRock(R, I + 3);
		R->SetActorScale3D(FVector(1.3f + 0.4f * (I % 3)));
		R->Refresh();
		SetBlocked(Rocks[I].X, Rocks[I].Y, true);
	}
	// Torch Blueprints are placed in Map_Saga and collected by CollectLevelProps.

	// ----- construction plots -----
	Plots.SetNum(2);
	Plots[0].Cell = FIntPoint(10, 8);
	Plots[1].Cell = FIntPoint(21, 18);
	for (FTSCPlot& P : Plots)
	{
		GroundExclusions.Emplace(FVector2D(CellCorner(P.Cell.X, P.Cell.Y)),
			FVector2D(CellCorner(P.Cell.X + 3, P.Cell.Y + 2)));
		P.Marker = SpawnPrim(CellCorner(P.Cell.X, P.Cell.Y));
		BuildPlotMarker(P.Marker, 3, 2);
		P.Marker->Refresh();
		P.Marker->SetActorHiddenInGame(true);
	}
	Plots[0].bAvailable = true;
	Plots[0].Marker->SetActorHiddenInGame(false);
	ProDressing.Add(Plots[0].Marker);

	GroundExclusions.Emplace(FVector2D(CellCorner(GardenRect.Min.X, GardenRect.Min.Y)),
		FVector2D(CellCorner(GardenRect.Max.X, GardenRect.Max.Y)));
	// The north and south log walls are placed in the level. Their clearances are
	// also used by the framing trees above.
	Terrain->BuildForestFloor(Ocean,
		[&](const FVector2D& P, float Radius)
		{
			if (WallDist(FVector(P, 0.f)) < Radius + 95.f) { return false; }
			const FIntPoint Min = WorldToCell(FVector(P - FVector2D(Radius), 0.f));
			const FIntPoint Max = WorldToCell(FVector(P + FVector2D(Radius), 0.f));
			for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
			{
				for (int32 X = Min.X; X <= Max.X; ++X)
				{
					if (InGrid(X, Y) && IsBlocked(X, Y)) { return false; }
					if (X >= GateX && X < WallEndX && (Y == -1 || Y == GH)) { return false; }
				}
			}
			for (const FBox2D& Box : GroundExclusions)
			{
				if (Box.ComputeSquaredDistanceToPoint(P) < FMath::Square(Radius)) { return false; }
			}
			return true;
		},
		[](const FVector2D& P)
		{
			const float X = P.X / CELL + GW / 2;
			return X < WallEndX && WallShape(X, P.Y / CELL + GH / 2) < 1.f;
		});

	// ----- people -----
	Hunter = SpawnUnit(ETSCRole::Hunter, TEXT("Hakon the Hunter"), CellF(9.6f, 12.f));
	Woodcutter = SpawnUnit(ETSCRole::Woodcutter, TEXT("Ulf the Woodcutter"), CellF(11.5f, 6.6f));
	Healer = SpawnUnit(ETSCRole::Healer, TEXT("Sigrun the Healer"), CellF(22.6f, 11.6f));
	Shield = SpawnUnit(ETSCRole::Shield, TEXT("Torvald Shield-Bearer"), CellF(20.5f, 12.f));
	for (int32 I = 0; I < 10; ++I)
	{
		ATSCUnit* W = SpawnUnit(ETSCRole::Worker, FString(), CellF(14.5f + (I % 5) * 1.1f, 13.5f + (I / 5) * 0.6f));
		Workers.Add(W);
	}
	NumWood = 4;
	NumFood = 4;
	NumHomeUnits = Units.Num();
	for (ATSCUnit* U : Units) { UnitHome.Add(U->GetActorLocation()); }
	SpawnAllWildlife();
	ResetVillagers();
}

void ATSCGameMode::ResetVillagers()
{
	for (int32 I = 0; I < NumHomeUnits && I < Units.Num(); ++I)
	{
		if (ATSCUnit* U = Units[I])
		{
			U->Teleport(UnitHome[I]);
			U->SetActorRotation(FRotator::ZeroRotator);
			U->SetActorHiddenInGame(false);
		}
	}
	Hunter->OrderWork(CellF(3.5f, 12.f), CellF(5.6f, 11.4f));
	Woodcutter->OrderWork(CellF(7.5f, 9.5f), CellF(11.5f, 6.5f));
	Healer->OrderWork(CellF(22.6f, 11.5f), CellF(21.4f, 12.3f));
	Shield->OrderWork(CellF(9.7f, 11.3f), CellF(25.4f, 12.f));
	UpdateJobs();
}

void ATSCGameMode::BeginPlay()
{
	Super::BeginPlay();
	EditsLeft = MaxEdits;
	DefineScenes();
	CollectLevelProps();  // before anything is spawned at runtime
	BuildAtmosphere();
	BuildWorld();
	BuildPrologueExtras();
	float W, F;
	ComputeRates(W, F);
	WoodRate = FMath::RoundToInt(W);
	FoodRate = FMath::RoundToInt(F);
	PlayMusic(MenuMusic);
	if (CVarPrologue.GetValueOnGameThread() != 0 && !FParse::Param(FCommandLine::Get(), TEXT("NoPrologue")))
	{
		StartPrologue();
	}
}

// =====================================================================================================================
// Prologue: the settlers land, clear the forest and build; the forest notices. Plays in a loop behind the title.
// =====================================================================================================================

/** A level actor that is a piece of wooden fence (static mesh or prop blueprint with "Fence" in its mesh / class path). */
static bool IsFenceProp(const AActor* A)
{
	if (A->GetClass()->GetPathName().Contains(TEXT("Fence"))) { return true; }
	if (const AStaticMeshActor* SM = Cast<AStaticMeshActor>(A))
	{
		const UStaticMesh* Mesh = SM->GetStaticMeshComponent() ? SM->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
		return Mesh && Mesh->GetPathName().Contains(TEXT("Fence"));
	}
	return false;
}

void ATSCGameMode::CollectLevelProps()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!A || A->IsHidden()) { continue; }
		const AStaticMeshActor* SM = Cast<AStaticMeshActor>(A);
		const bool bPropBlueprint = A->GetClass()->GetPathName().StartsWith(TEXT("/Game/Props/"));
		if (!SM && !bPropBlueprint) { continue; }
		if (SM)
		{
			const UStaticMesh* Mesh = SM->GetStaticMeshComponent() ? SM->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
			if (!Mesh || Mesh->GetPathName().Contains(TEXT("Mushroom"))) { continue; }  // mushrooms belong to the forest
		}
		const FVector P = A->GetActorLocation();
		const float Cx = P.X / CELL + GW / 2, Cy = P.Y / CELL + GH / 2;
		if (Cx < 27.5f && WallShape(Cx, Cy) < 1.08f) { ProLevelProps.Add(A); }
		else if (Cx < WallCX && IsFenceProp(A)) { ProRoadFence.Add(A); } // the double fence running out of the gate along the road
	}
	ProLevelProps.Sort([](const AActor& A, const AActor& B) { return A.GetActorLocation().X > B.GetActorLocation().X; });
	// The road fence is built last, outwards from the gate; remember each piece's own scale (the level may have resized it).
	const FVector GatePos = CellCorner(GateX, GateY + 1);
	ProRoadFence.Sort([GatePos](const AActor& A, const AActor& B)
	{
		return FVector::DistSquared2D(A.GetActorLocation(), GatePos) < FVector::DistSquared2D(B.GetActorLocation(), GatePos);
	});
	for (const AActor* F : ProRoadFence) { ProRoadFenceScale.Add(F->GetActorScale3D()); }
	UE_LOG(LogSaga, Display, TEXT("[Saga] Prologue: %d level props inside the wall, %d road fence pieces"), ProLevelProps.Num(), ProRoadFence.Num());
}

void ATSCGameMode::BuildPrologueExtras()
{
	// the forest that stood where the village is: never blocks a cell, only shown during the prologue
	for (int32 X = 9; X <= 26; ++X)
	{
		for (int32 Y = 0; Y < 20; ++Y)
		{
			if (WallShape(X + 0.5f, Y + 0.5f) > 0.9f || Rand01(X, Y, 23) > 0.36f) { continue; }
			const FVector Jitter((Rand01(X, Y, 24) - 0.5f) * 90.f, (Rand01(X, Y, 25) - 0.5f) * 90.f, 0.f);
			ATSCPrimActor* T = SpawnPrim(CellCenter(X, Y) + Jitter);
			BuildTree(T, X * 41 + Y * 7, false);
			T->Refresh();
			T->SetActorHiddenInGame(true);
			ProTrees.Add(T);
			// felled in a wave from the shore to the west
			ProTreeFell.Add(ProTLand + 1.2f + (26 - X) / 17.f * 9.f + Rand01(X, Y, 29) * 1.6f);
			ProTreeYaw.Add(Rand01(X, Y, 30) * 2.f * PI);
		}
	}
	// the watchers at the tree line: not in Units, so nobody targets them and they fight nobody
	for (int32 I = 0; I < 6; ++I)
	{
		ATSCUnit* W = SpawnUnit(ETSCRole::Creature, FString(), CellF(3.f, 10.f + I));
		Units.Remove(W);
		W->SetActorHiddenInGame(true);
		ProWatchers.Add(W);
	}
	const FVector GatePos = CellCorner(GateX, GateY + 1);
	WallSegs.Sort([GatePos](const ATSCPrimActor& A, const ATSCPrimActor& B)
	{
		return FVector::DistSquared2D(A.GetActorLocation(), GatePos) < FVector::DistSquared2D(B.GetActorLocation(), GatePos);
	});
}

void ATSCGameMode::SetSailHoisted(float Alpha)
{
	if (LongshipSail) { LongshipSail->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Clamp(Alpha, 0.03f, 1.f))); }
}

void ATSCGameMode::SetCameraView(const FVector& Loc, float Arm)
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (ATSCColonyCameraPawn* Cam = Cast<ATSCColonyCameraPawn>(PC->GetPawn())) { Cam->SetView(Loc, Arm); }
	}
}

void ATSCGameMode::StartPrologue()
{
	bPrologue = true;
	if (Terrain)
	{
		Terrain->SetTrailsVisible(false);
		Terrain->SetClearingStumpsVisible(false);
	}
	ProTime = 0.f;
	ProLanded = 0;
	ProStage = 0;
	bProLandingPlayed = false;
	for (ATSCUnit* A : FarmAnimals) { if (A) { A->SetActorHiddenInGame(true); } } // they come once the village is built
	if (HunterDog) { HunterDog->SetActorHiddenInGame(true); }
	ProDusk = 0.f;
	ProFade = 1.f;
	ProCaption.Empty();
	ProCaptionAlpha = 0.f;

	Longship->SetActorLocation(ShipMooring + FVector(ProShipRun, 0.f, 0.f));
	SetSailHoisted(1.f); // under sail on the way in
	for (ATSCPrimActor* S : WallSegs) { S->SetActorHiddenInGame(true); }
	for (ATSCPrimActor* D : ProDressing) { D->SetActorHiddenInGame(true); }
	for (AActor* A : ProLevelProps) { if (A) { A->SetActorHiddenInGame(true); } }
	for (int32 I = 0; I < ProRoadFence.Num(); ++I)
	{
		if (ProRoadFence[I]) { ProRoadFence[I]->SetActorScale3D(ProRoadFenceScale[I]); ProRoadFence[I]->SetActorHiddenInGame(true); }
	}
	for (ATSCPrimActor* B : ProBuildings) { B->SetBuildProgress(0.f); }
	for (ATSCPrimActor* T : ProTrees)
	{
		T->SetActorRotation(FRotator::ZeroRotator);
		T->SetActorScale3D(FVector::OneVector);
		T->SetActorHiddenInGame(false);
	}
	for (ATSCUnit* W : ProWatchers) { W->SetActorHiddenInGame(true); }
	for (int32 I = 0; I < NumHomeUnits && I < Units.Num(); ++I)
	{
		Units[I]->Teleport(ShipMooring);
		Units[I]->SetActorHiddenInGame(true);
	}
	ProCamLoc = CellF(25.5f, 13.f);
	ProCamArm = 10500.f;
	SetCameraView(ProCamLoc, ProCamArm);
	PlayMusic(MenuMusic);
}

void ATSCGameMode::EndPrologue()
{
	if (!bPrologue) { return; }
	bPrologue = false;
	if (Terrain)
	{
		Terrain->SetTrailsVisible(true);
		Terrain->SetClearingStumpsVisible(true);
	}
	ProFade = 0.f;
	ProDusk = 0.f;
	ProCaptionAlpha = 0.f;

	Longship->SetActorLocation(ShipMooring);
	SetSailHoisted(0.f);
	// the farm animals stay for good, Hakon's dog beside him
	for (ATSCUnit* A : FarmAnimals) { if (A) { A->SetActorScale3D(FVector::OneVector); A->SetActorHiddenInGame(false); } }
	if (HunterDog) { HunterDog->SetActorScale3D(FVector::OneVector); HunterDog->SetActorHiddenInGame(false); }
	for (ATSCPrimActor* S : WallSegs)
	{
		S->SetActorScale3D(FVector::OneVector);
		S->SetActorHiddenInGame(false);
	}
	for (ATSCPrimActor* D : ProDressing) { D->SetActorHiddenInGame(false); }
	for (AActor* A : ProLevelProps) { if (A) { A->SetActorHiddenInGame(false); } }
	for (int32 I = 0; I < ProRoadFence.Num(); ++I)
	{
		if (ProRoadFence[I]) { ProRoadFence[I]->SetActorScale3D(ProRoadFenceScale[I]); ProRoadFence[I]->SetActorHiddenInGame(false); }
	}
	for (ATSCPrimActor* B : ProBuildings) { B->SetBuildProgress(1.f); }
	for (ATSCPrimActor* T : ProTrees) { T->SetActorHiddenInGame(true); }
	for (ATSCUnit* W : ProWatchers) { W->SetActorHiddenInGame(true); }
	ResetVillagers();
	++GridVersion;  // everyone plans a fresh path from home
	SetCameraView(CellF(17.f, 12.f), DefaultCamArm);
	PlayMusic(MenuMusic);
}

void ATSCGameMode::SimPrologue(float Dt)
{
	ProTime += Dt;
	const float T = ProTime;
	if (Terrain && T >= ProTClear + 1.5f)
	{
		Terrain->SetTrailsVisible(true);
		Terrain->SetClearingStumpsVisible(true);
	}
	if (T >= ProTEnd)
	{
		StartPrologue();
		return;
	}
	ProFade = FMath::Clamp(FMath::Max(1.f - T / 1.2f, (T - (ProTEnd - 1.4f)) / 1.2f), 0.f, 1.f);

	// ----- captions: one line per beat -----
	{
		static const TCHAR* Lines[4] = {
			TEXT("They came on the grey tide, one ship and forty souls."),
			TEXT("The forest was old. They did not ask its leave."),
			TEXT("Timber became wall, and wall became home."),
			TEXT("But every stump was counted. The forest remembers.") };
		const float Edges[5] = { 0.f, ProTLand, ProTClear, ProTRaise, ProTWake };
		ProCaptionAlpha = 0.f;
		for (int32 I = 0; I < 4; ++I)
		{
			if (T >= Edges[I] && T < Edges[I + 1])
			{
				ProCaption = Lines[I];
				ProCaptionAlpha = FMath::Clamp(FMath::Min(T - Edges[I] - 0.7f, Edges[I + 1] - 0.3f - T) / 0.8f, 0.f, 1.f);
			}
		}
	}

	// ----- the ship comes in -----
	{
		const float A = FMath::Clamp(T / (ProTLand - 0.5f), 0.f, 1.f);
		const float Left = FMath::Pow(1.f - A, 3.f);
		Longship->SetActorLocation(ShipMooring + FVector(ProShipRun * Left, 0.f, 0.f));
		// The ship stops at ProTLand - 0.5; then the sail comes down, and only after that the crew goes ashore.
		const float Lowered = FMath::Clamp((T - (ProTLand - 0.5f)) / ProSailDrop, 0.f, 1.f);
		SetSailHoisted(1.f - FMath::SmoothStep(0.f, 1.f, Lowered));
		if (T >= ProTLand - 0.6f && !bProLandingPlayed)
		{
			bProLandingPlayed = true;
			if (Ocean) { Ocean->PlayLanding(FVector2D(Longship->GetActorLocation()) - FVector2D(300.f, 0.f)); }
		}
	}

	// ----- people: ashore one by one, then to the trees, then to the building sites, then home -----
	while (ProLanded < NumHomeUnits && T > ProTLand - 0.5f + ProSailDrop + 0.3f + ProLanded * 0.45f)
	{
		const int32 I = ProLanded++;
		ATSCUnit* U = Units[I];
		U->Teleport(CellF(29.4f, 11.3f + (I % 2) * 0.7f));
		U->SetActorHiddenInGame(false);
		U->OrderWork(CellF(23.5f - (I % 7) * 2.f, 4.5f + ((I * 5) % 15)), CellF(25.2f, 9.5f + (I % 5)));
	}
	if (ProStage == 0 && T >= ProTClear)
	{
		ProStage = 1;
		static const FVector2D Sites[14] = { {13.5f, 13.4f}, {20.4f, 13.4f}, {11.6f, 6.4f}, {21.5f, 11.3f}, {11.f, 17.3f}, {13.f, 19.3f}, {22.f, 7.3f},
			{24.f, 17.3f}, {9.6f, 10.4f}, {9.6f, 13.5f}, {12.5f, 8.5f}, {16.5f, 9.5f}, {18.5f, 13.5f}, {10.5f, 13.f} };
		for (int32 I = 0; I < ProLanded; ++I)
		{
			Units[I]->OrderWork(CellF(Sites[I % 14].X, Sites[I % 14].Y), CellF(13.2f + (I % 3) * 0.6f, 5.6f));
		}
	}
	if (ProStage == 1 && T >= ProTRaise)
	{
		ProStage = 2;
		for (int32 I = 0; I < ProLanded; ++I) { Units[I]->OrderHold(UnitHome[I]); }
		PlayMusic(NightMusic);
	}
	for (int32 I = 0; I < ProLanded; ++I) { Units[I]->Sim(Dt); }

	// ----- the trees come down -----
	for (int32 I = 0; I < ProTrees.Num(); ++I)
	{
		ATSCPrimActor* Tree = ProTrees[I];
		const float D = T - ProTreeFell[I];
		if (D <= 0.f || Tree->IsHidden()) { continue; }
		if (D < 1.3f)
		{
			const float A = D / 1.3f;
			const float Ang = A * A * HALF_PI * 0.985f;
			const float S = FMath::Sin(Ang);
			Tree->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, FVector(FMath::Cos(ProTreeYaw[I]) * S, FMath::Sin(ProTreeYaw[I]) * S, FMath::Cos(Ang))));
		}
		else if (D < 2.5f)
		{
			Tree->SetActorScale3D(FVector(FMath::Max(0.02f, 1.f - (D - 1.3f) / 1.2f)));  // limbed and hauled off
		}
		else
		{
			Tree->SetActorHiddenInGame(true);
		}
	}

	// ----- the wall goes up from the gate outwards, then the houses and everything in them -----
	for (int32 I = 0; I < WallSegs.Num(); ++I)
	{
		const float D = (T - (ProTClear + 0.3f + 9.5f * I / WallSegs.Num())) / 0.3f;
		if (D <= 0.f) { continue; }
		WallSegs[I]->SetActorScale3D(FVector(1.f, 1.f, FMath::Clamp(D, 0.05f, 1.f)));
		if (WallSegs[I]->IsHidden()) { WallSegs[I]->SetActorHiddenInGame(false); }
	}
	for (int32 I = 0; I < ProDressing.Num(); ++I)
	{
		if (ProDressing[I]->IsHidden() && T > ProTClear + 1.5f + 10.f * I / ProDressing.Num()) { ProDressing[I]->SetActorHiddenInGame(false); }
	}
	for (int32 I = 0; I < ProBuildings.Num(); ++I)
	{
		const float P = FMath::Clamp((T - (ProTClear + 1.f + I * 1.2f)) / 5.5f, 0.f, 1.f);
		if (!FMath::IsNearlyEqual(ProBuildings[I]->BuildProgress, P)) { ProBuildings[I]->SetBuildProgress(P); }
	}
	for (int32 I = 0; I < ProLevelProps.Num(); ++I)
	{
		AActor* A = ProLevelProps[I];
		if (A && A->IsHidden() && T > ProTClear + 5.f + 8.f * I / ProLevelProps.Num()) { A->SetActorHiddenInGame(false); }
	}

	// ----- last of all the road fence grows out of the gate, piece by piece, as the village is finished -----
	{
		const float FenceStart = ProTClear + 10.5f;
		for (int32 I = 0; I < ProRoadFence.Num(); ++I)
		{
			AActor* F = ProRoadFence[I];
			if (!F) { continue; }
			const float D = (T - (FenceStart + ProFenceSpan * I / ProRoadFence.Num())) / 0.4f;
			if (D <= 0.f) { continue; }
			F->SetActorScale3D(ProRoadFenceScale[I] * FVector(1.f, 1.f, FMath::Clamp(D, 0.05f, 1.f)));
			if (F->IsHidden()) { F->SetActorHiddenInGame(false); }
		}
	}

	// ----- animals: Hakon's dog comes ashore with him; livestock and the village dog arrive once the village is finished -----
	{
		if (HunterDog && HunterDog->IsHidden() && Hunter && !Hunter->IsHidden())
		{
			HunterDog->Teleport(Hunter->GetActorLocation() + FVector(-110.f, 60.f, 0.f));
			HunterDog->SetActorHiddenInGame(false);
		}
		// finished = the last building and the road fence are both done
		const float BuiltAt = FMath::Max(ProTClear + 1.f + FMath::Max(ProBuildings.Num() - 1, 0) * 1.2f + 5.5f, ProTClear + 10.5f + ProFenceSpan + 0.4f);
		const float FarmT = FMath::Min(BuiltAt + 0.5f, ProTWake - 1.5f);
		if (T >= FarmT)
		{
			const float Grow = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((T - FarmT) / 1.2f, 0.f, 1.f));
			for (ATSCUnit* A : FarmAnimals)
			{
				if (!A) { continue; }
				A->SetActorScale3D(FVector(FMath::Max(Grow, 0.05f)));
				if (A->IsHidden()) { A->SetActorHiddenInGame(false); }
			}
		}
	}

	// ----- dusk, fog, and the things that grow in it -----
	ProDusk = FMath::SmoothStep(ProTRaise - 2.5f, ProTRaise + 2.5f, T);
	for (int32 I = 0; I < ProWatchers.Num(); ++I)
	{
		ATSCUnit* W = ProWatchers[I];
		const float In = FMath::Clamp((T - (ProTRaise + 1.2f + I * 0.35f)) / 1.6f, 0.f, 1.f);
		const float Out = FMath::Clamp((T - (ProTWake - 3.6f + I * 0.2f)) / 2.2f, 0.f, 1.f);
		const float A = FMath::SmoothStep(0.f, 1.f, In) * (1.f - FMath::SmoothStep(0.f, 1.f, Out));
		const bool bShow = A > 0.01f;
		if (W->IsHidden() == bShow) { W->SetActorHiddenInGame(!bShow); }
		if (!bShow) { continue; }
		const FVector Front = CellF(6.7f - (I % 2) * 1.1f, 9.3f + I * 0.95f);
		W->SetActorLocation(FMath::Lerp(Front + FVector(-700.f, 0.f, 0.f), Front, A));
		W->SetActorRotation(FRotator::ZeroRotator);  // facing the gate
		W->SetActorScale3D(FVector(FMath::Max(0.05f, A)));
		W->Idle(Dt);
	}

	// ----- camera: one slow move per beat -----
	{
		const FVector Target = T < ProTLand ? CellF(25.5f, 13.f) : T < ProTClear ? CellF(20.f, 12.f) : T < ProTRaise ? CellF(17.f, 12.f)
			: T < ProTWake ? CellF(10.5f, 11.5f) : CellF(17.f, 12.f);
		const float Arm = T < ProTLand ? 10500.f : T < ProTClear ? 14500.f : T < ProTRaise ? DefaultCamArm : T < ProTWake ? 9500.f : DefaultCamArm;
		ProCamLoc = FMath::VInterpTo(ProCamLoc, Target, Dt, 0.8f);
		ProCamArm = FMath::FInterpTo(ProCamArm, Arm, Dt, 0.8f);
		SetCameraView(ProCamLoc, ProCamArm);
	}
}

// =====================================================================================================================
// Tick
// =====================================================================================================================

void ATSCGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float RealDt = FMath::Min(DeltaSeconds, 0.1f);
	WorldTime += RealDt;
	UpdatePineWind(GetWorld(), RealDt);

	if (!bCameraSet)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				Pawn->SetActorLocationAndRotation(CellF(17.f, 12.f), FRotator(0.f, -45.f, 0.f));
				bCameraSet = true;
			}
		}
	}

	// visuals keep running while the simulation is frozen
	for (ATSCPrimActor* P : Prims) { if (P) { P->SimVisual(RealDt, WorldTime); } }
	SimEmbers(RealDt);
	for (int32 I = Tracers.Num() - 1; I >= 0; --I) { Tracers[I].Time -= RealDt; if (Tracers[I].Time <= 0.f) { Tracers.RemoveAtSwap(I); } }
	for (int32 I = Toasts.Num() - 1; I >= 0; --I) { Toasts[I].Time -= RealDt; if (Toasts[I].Time <= 0.f) { Toasts.RemoveAt(I); } }
	BannerTimer = FMath::Max(0.f, BannerTimer - RealDt);
	FlashTimer = FMath::Max(0.f, FlashTimer - RealDt);
	if (TransitionTimer > 0.f && !bCutOpen)
	{
		TransitionTimer -= RealDt;
		if (FMath::FRand() < RealDt * 12.f) { for (const FTSCScene& S : Scenes) { if (S.bAshen && S.Timer < 0.f) { SpawnEmbers(S.Region, 2); } } }
	}

	// the scout boat rows away once its scene is over
	if (RaiderBoat && Scenes[3].bResolved && !RaiderBoat->IsHidden())
	{
		RaiderBoat->AddActorWorldOffset(FVector(320.f * RealDt, 0.f, 0.f));
		if (RaiderBoat->GetActorLocation().X > CellF(44.f, 0.f).X) { RaiderBoat->SetActorHiddenInGame(true); }
	}

	// gate leaves
	{
		const float Want = (bGateClosed && !bGateBroken) ? 0.f : 1.f;
		GateOpenAlpha = FMath::FInterpConstantTo(GateOpenAlpha, Want, RealDt, 1.2f);
		if (GateLeaves.Num() == 2)
		{
			const float Fall = bGateBroken ? 75.f : 0.f;
			GateLeaves[0]->SetRelativeRotation(FRotator(0.f, 100.f * GateOpenAlpha, Fall * GateOpenAlpha));
			GateLeaves[1]->SetRelativeRotation(FRotator(0.f, -100.f * GateOpenAlpha, -Fall * GateOpenAlpha));
		}
	}

	if (bPrologue && Phase == ETSCPhase::Title) { SimPrologue(RealDt); }

	if (!IsFrozen())
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(SpeedMul));
		const float Dt = RealDt * SpeedMul / Steps;
		for (int32 S = 0; S < Steps && !IsFrozen(); ++S)
		{
			if (Phase == ETSCPhase::Day) { SimDay(Dt); }
			else if (Phase == ETSCPhase::Night) { SimNight(Dt); }
			for (int32 I = 0; I < Units.Num(); ++I) { if (Units[I]) { Units[I]->Sim(Dt); } }
			for (ATSCUnit* A : Wildlife) { if (A && !A->IsHidden()) { A->Sim(Dt); } } // hidden = not arrived yet (prologue)
		}
	}
	{
		FTSCOceanInput In;
		In.Time = WorldTime;
		In.Ash = DockActor ? DockActor->SagaAlpha : 0.f;
		In.bDaylight = Phase == ETSCPhase::Day || Phase == ETSCPhase::Title;
		In.Focus = CellF(17.f, 12.f);
		In.Zoom01 = 0.5f;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn()) { In.Focus = Pawn->GetActorLocation(); }
			if (PC->PlayerCameraManager)
			{
				In.Zoom01 = FMath::Clamp((FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), In.Focus) - 6000.f) / 14000.f, 0.f, 1.f);
			}
		}
		if (Sun)
		{
			In.SunColor = Sun->GetLightColor();
			In.SunIntensity = Sun->Intensity;
		}
		In.MeadowColor = Pal::ForestFloor;
		if (Ocean) { Ocean->Sim(RealDt, In); }
		if (Terrain) { Terrain->Sim(RealDt, In, AshAmount); }
	}
	UpdateAtmosphere(RealDt);
	if (bAuto) { AutoPlay(RealDt); }
}

float ATSCGameMode::GroundReliefAt(const FVector& P) const
{
	return Terrain ? Terrain->GroundRelief(FVector2D(P)) : 0.f;
}

float ATSCGameMode::WadeDepthAt(const FVector& P) const
{
	const float ShoreX = Ocean ? Ocean->ShoreXAt(P.Y) : CellF(28.f, 0.f).X;
	if (P.X <= ShoreX) { return 0.f; }
	if (DockActor && !DockActor->IsHidden() && DockActor->SagaAlpha < 0.5f)
	{
		const FVector C = CellCorner(26, 11);
		if (P.X >= C.X && P.X <= C.X + 6.f * CELL && P.Y >= C.Y && P.Y <= C.Y + 2.f * CELL) { return 0.f; }
	}
	return FMath::Clamp((P.X - ShoreX) / 400.f, 0.f, 1.f) * 55.f;
}

void ATSCGameMode::WadeStep(const FVector& P, const FVector& Dir)
{
	if (Ocean) { Ocean->WadeStep(FVector2D(P), FVector2D(Dir)); }
}

void ATSCGameMode::UpdateAtmosphere(float Dt)
{
	if (!Sun) { return; }
	int32 NumAshen = 0;
	for (const FTSCScene& S : Scenes) { NumAshen += S.bAshen ? 1 : 0; }
	AshAmount = FMath::FInterpTo(AshAmount, NumAshen / 4.f, Dt, 0.6f);

	const bool bNight = Phase == ETSCPhase::Night || (Phase == ETSCPhase::Epilogue);
	const float F = Phase == ETSCPhase::Day ? FMath::Clamp(DayTime / DayLength, 0.f, 1.f) : (Phase == ETSCPhase::Title ? (bPrologue ? FMath::Lerp(0.1f, 1.f, ProDusk) : 0.55f) : 1.f);
	// cold blue-grey day sliding into an orange dusk; the night is a deep blue lit by fires
	FLinearColor SunCol = FMath::Lerp(FLinearColor(0.62f, 0.74f, 1.f), FLinearColor(1.f, 0.56f, 0.34f), F * F);
	// ambient comes from the sky light only, so the sun's moving shadows read; the sun is a little stronger to hold the overall brightness
	float SunInt = FMath::Lerp(6.5f, 4.f, F);
	float FillInt = FMath::Lerp(0.17f, 0.14f, F);
	if (bNight)
	{
		SunCol = FLinearColor(0.35f, 0.48f, 1.f);
		SunInt = 1.0f;
		FillInt = 4.f; // the night sky is nearly black: lift what little it gives
	}
	static float CurSun = 2.f, CurFill = 0.7f;
	static FLinearColor CurCol(0.6f, 0.7f, 1.f);
	const float K = Dt <= 0.f ? 1.f : FMath::Clamp(Dt * 0.8f, 0.f, 1.f);
	CurSun = FMath::Lerp(CurSun, SunInt, K);
	CurFill = FMath::Lerp(CurFill, FillInt, K);
	CurCol = FMath::Lerp(CurCol, SunCol, K);

	// the sun crosses the sky over each day: low in the morning, high at noon, low again at dusk, swinging 130 degrees
	// around the clearing, so the shadows turn and stretch. It follows DayTime, hence also the fast-forward speed.
	// At night the same light is the moon and settles at a fixed angle.
	static FQuat CurSunRot = GTSCDaySun;
	const FRotator DayArc(-(FMath::Lerp(28.f, 22.f, F) + 32.f * FMath::Sin(F * PI)), -22.f + FMath::Lerp(-65.f, 65.f, F), 0.f);
	GTSCDaySun = FQuat::Slerp(GTSCDaySun, DayArc.Quaternion(), K);
	CurSunRot = FQuat::Slerp(CurSunRot, bNight ? FRotator(-42.f, -22.f, 0.f).Quaternion() : GTSCDaySun, K);
	Sun->SetWorldRotation(CurSunRot);
	// a low sun lights the ground less; make up most of it so mornings and dusks keep the brightness the game was tuned for (42 degrees)
	const float SunHeight = FMath::Max(-CurSunRot.GetForwardVector().Z, 0.3f);
	Sun->SetIntensity(CurSun * FMath::Pow(0.669f / SunHeight, 0.6f));
	Sun->SetLightColor(CurCol);
	if (USkyLightComponent* Sky = GTSCSkyLight.Get())
	{
		Sky->SetIntensity(CurFill);
		Sky->SetLightColor(FMath::Lerp(FLinearColor(0.8f, 0.9f, 1.f), FLinearColor(1.f, 0.9f, 0.8f), AshAmount));
	}

	const float Thick = (Phase == ETSCPhase::Day && Day >= 3) || bNight || (bPrologue && ProDusk > 0.5f) ? 1.7f : 1.f;
	static float CurThick = 1.f;
	CurThick = FMath::Lerp(CurThick, Thick, K);
	Fog->SetFogDensity(0.012f * CurThick);
	Fog->SetFogInscatteringColor(FMath::Lerp(FLinearColor(0.16f, 0.23f, 0.30f), FLinearColor(0.24f, 0.21f, 0.19f), AshAmount) * (bNight ? 0.45f : 1.f));
	TSCUpdateSky(Sun, AshAmount, K, bNight, F, Thick, Dt, SpeedMul);

	// colour drain: the more of the world is Ashen, the greyer it gets; a hard dip while a cut is transitioning
	float Sat = FMath::Lerp(1.08f, 0.72f, AshAmount);
	float Gain = 1.f;
	if (TransitionTimer > 0.f)
	{
		const float T = FMath::Sin(FMath::Clamp(TransitionTimer / 3.4f, 0.f, 1.f) * PI);
		Sat *= 1.f - 0.85f * T;
		Gain += 0.25f * T;
	}
	if (bCutOpen) { Sat *= 0.35f; Gain = 0.6f; }
	PostFx->Settings.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
	PostFx->Settings.ColorGain = FVector4(Gain, Gain * (1.f - 0.04f * AshAmount), Gain * (1.f - 0.1f * AshAmount), 1.f);
}

// Drives the Ultra Dynamic Sky Blueprint from the game's clock. The Blueprint's variables are set by name, so the C++
// module does not depend on the asset. Its own Tick pushes them into the sky material every frame.
static void TSCUpdateSky(UDirectionalLightComponent* Sun, float AshAmount, float K, bool bNight, float DayF, float Thick, float Dt, float SpeedMul)
{
	AActor* SkyActor = GTSCSky.Get();
	if (!SkyActor || !Sun) { return; }
	UClass* C = SkyActor->GetClass();
	auto SetNum = [&](const TCHAR* Name, double V)
	{
		if (FNumericProperty* P = FindFProperty<FNumericProperty>(C, Name)) { P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(SkyActor), V); }
	};

	// 0..2400 clock: morning sliding to sunset over the day, then deep night (the epilogue stays dark too)
	const float TargetTime = bNight ? 2330.f : FMath::Lerp(800.f, 1850.f, DayF);
	GTSCSkyTime = FMath::Lerp(GTSCSkyTime, TargetTime, K);
	SetNum(TEXT("Time of Day"), GTSCSkyTime);
	// the sky's own night is pitch black; lift it so the cloud deck still reads over the fires
	SetNum(TEXT("Night Brightness"), 0.5f * FMath::GetMappedRangeValueClamped(FVector2f(1850.f, 2330.f), FVector2f(0.f, 1.f), GTSCSkyTime));

	// heavier cloud when the fog thickens, overcast as the world turns Ashen; the Blueprint eases towards the target
	const float Clouds = FMath::Lerp(Thick > 1.f ? 2.1f : 1.25f, 3.2f, AshAmount);
	SetNum(TEXT("Cloud Density_target"), Clouds);
	if (K >= 1.f) { SetNum(TEXT("Cloud Density"), Clouds); }
	SetNum(TEXT("Saturation"), FMath::Lerp(1.f, 0.35f, AshAmount));

	// cloud shadows: the sky's light-function material goes on the game's own sun and follows the same cloud cover
	// (the sky's defaults are sized for a far bigger world, so the pattern is scaled down to this clearing)
	SetNum(TEXT("Cloud Shadows Scale"), 40000.f);
	SetNum(TEXT("Cloud Shadows Intensity"), bNight ? 0.2f : 0.4f);
	if (FBoolProperty* P = FindFProperty<FBoolProperty>(C, TEXT("Use Cloud Shadows"))) { P->SetPropertyValue_InContainer(SkyActor, true); }
	FObjectProperty* ShadowProp = FindFProperty<FObjectProperty>(C, TEXT("Cloud Shadows MID"));
	if (UMaterialInstanceDynamic* Shadows = ShadowProp ? Cast<UMaterialInstanceDynamic>(ShadowProp->GetObjectPropertyValue_InContainer(SkyActor)) : nullptr)
	{
		FNumericProperty* Density = FindFProperty<FNumericProperty>(C, TEXT("Cloud Density"));
		if (Density) { Shadows->SetScalarParameterValue(TEXT("Cloud Density"), Density->GetFloatingPointPropertyValue(Density->ContainerPtrToValuePtr<void>(SkyActor))); }
		// fast-forward: the materials run on real time, so the extra travel is added as a time offset (no jump on toggle)
		float ShadowSpeed = 0.3f;
		Shadows->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Cloud Speed")), ShadowSpeed);
		GTSCShadowDrift += Dt * (SpeedMul - 1.f) * ShadowSpeed;
		Shadows->SetScalarParameterValue(TEXT("Cloud Time Offset"), GTSCShadowDrift);
		Sun->SetLightFunctionMaterial(Shadows);
		Sun->SetLightFunctionFadeDistance(1000000.f);
	}

	FObjectProperty* SkyMatProp = FindFProperty<FObjectProperty>(C, TEXT("UDM_mat"));
	FNumericProperty* SkySpeed = FindFProperty<FNumericProperty>(C, TEXT("Cloud Speed"));
	if (UMaterialInstanceDynamic* SkyMat = SkyMatProp ? Cast<UMaterialInstanceDynamic>(SkyMatProp->GetObjectPropertyValue_InContainer(SkyActor)) : nullptr)
	{
		const float Speed = SkySpeed ? (float)SkySpeed->GetFloatingPointPropertyValue(SkySpeed->ContainerPtrToValuePtr<void>(SkyActor)) : 0.05f;
		GTSCSkyDrift += Dt * (SpeedMul - 1.f) * Speed;
		SkyMat->SetScalarParameterValue(TEXT("Cloud Time Offset"), GTSCSkyDrift);
	}

	// sun disc: exactly where the game's sun light stands on its daily arc; after dusk it sinks below the horizon
	FStructProperty* RotProp = FindFProperty<FStructProperty>(C, TEXT("Sun Rotation"));
	if (RotProp && RotProp->Struct == TBaseStructure<FRotator>::Get())
	{
		FRotator R = GTSCDaySun.Rotator();
		R.Pitch += 40.f * FMath::GetMappedRangeValueClamped(FVector2f(1850.f, 2100.f), FVector2f(0.f, 1.f), GTSCSkyTime);
		*RotProp->ContainerPtrToValuePtr<FRotator>(SkyActor) = R;
	}
}

// =====================================================================================================================
// Embers
// =====================================================================================================================

void ATSCGameMode::SpawnEmbers(int32 Region, int32 Count)
{
	const FIntRect& R = Region == TSCRegion::Forest ? ForestRect : Region == TSCRegion::Garden ? GardenRect : Region == TSCRegion::Grove ? GroveRect : DockRect;
	for (int32 I = 0; I < Count && Embers.Num() < 140; ++I)
	{
		const FVector P = CellF(FMath::FRandRange((float)FMath::Max(R.Min.X, -2), (float)FMath::Min(R.Max.X, GW)), FMath::FRandRange((float)FMath::Max(R.Min.Y, 0), (float)FMath::Min(R.Max.Y, GH)));
		ATSCPrimActor* A = GetWorld()->SpawnActor<ATSCPrimActor>(ATSCPrimActor::StaticClass(), P + FVector(0.f, 0.f, FMath::FRandRange(10.f, 200.f)), FRotator(FMath::FRandRange(0.f, 90.f), FMath::FRandRange(0.f, 90.f), 0.f));
		const bool bAsh = FMath::FRand() < 0.35f;
		const int32 Idx = A->AddPart(ETSCShape::Cube, FVector::ZeroVector, FRotator::ZeroRotator, FVector(bAsh ? 0.26f : 0.18f, bAsh ? 0.26f : 0.18f, 0.04f), bAsh ? Pal::Ash * 2.f : Pal::Fire);
		if (!bAsh) { A->SetGlow(Idx, FLinearColor(18.f, 5.f, 0.5f), FLinearColor(18.f, 5.f, 0.5f)); }
		A->Parts[Idx].ACol = A->Parts[Idx].LCol;
		A->Refresh();
		FTSCEmber E;
		E.Actor = A;
		E.Vel = FVector(FMath::FRandRange(-40.f, 60.f), FMath::FRandRange(-40.f, 40.f), bAsh ? FMath::FRandRange(-30.f, 40.f) : FMath::FRandRange(70.f, 190.f));
		E.Life = FMath::FRandRange(1.8f, 3.6f);
		Embers.Add(E);
	}
}

void ATSCGameMode::SimEmbers(float Dt)
{
	for (int32 I = Embers.Num() - 1; I >= 0; --I)
	{
		FTSCEmber& E = Embers[I];
		E.Life -= Dt;
		if (E.Life <= 0.f || !E.Actor)
		{
			if (E.Actor) { E.Actor->Destroy(); }
			Embers.RemoveAtSwap(I);
			continue;
		}
		E.Actor->AddActorWorldOffset(E.Vel * Dt);
		E.Actor->AddActorLocalRotation(FRotator(120.f * Dt, 90.f * Dt, 0.f));
		E.Actor->SetActorScale3D(FVector(FMath::Clamp(E.Life, 0.05f, 1.f)));
	}
}

// =====================================================================================================================
// Flow
// =====================================================================================================================

void ATSCGameMode::StartGame()
{
	if (Phase != ETSCPhase::Title) { return; }
	EndPrologue();
	StartDay(1);
}

void ATSCGameMode::RestartGame()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void ATSCGameMode::ToggleSpeed()
{
	SpeedMul = SpeedMul < 1.5f ? 2.f : (SpeedMul < 3.f ? 4.f : 1.f);
}

FString ATSCGameMode::PhaseLabel() const
{
	switch (Phase)
	{
	case ETSCPhase::Day: return Day == 1 ? TEXT("DAY 1 - SETTLING IN") : Day == 2 ? TEXT("DAY 2 - GROWING THE VILLAGE") : TEXT("DAY 3 - PREPARING");
	case ETSCPhase::Night: return TEXT("FINAL NIGHT - THE DIRECTOR'S CUT");
	case ETSCPhase::Epilogue: return TEXT("EPILOGUE");
	default: return TEXT("");
	}
}

float ATSCGameMode::PhaseFrac() const
{
	if (Phase == ETSCPhase::Day) { return FMath::Clamp(DayTime / DayLength, 0.f, 1.f); }
	if (Phase == ETSCPhase::Night) { return FMath::Clamp(NightTime / NightLength, 0.f, 1.f); }
	return 0.f;
}

void ATSCGameMode::ShowBanner(const FString& Text, float Time)
{
	Banner = Text;
	BannerTimer = Time;
}

void ATSCGameMode::Toast(const FString& Text)
{
	UE_LOG(LogSaga, Display, TEXT("[Saga] %s"), *Text);
	FTSCToast T;
	T.Text = Text;
	T.Time = 9.f;
	Toasts.Add(T);
	if (Toasts.Num() > 5) { Toasts.RemoveAt(0); }
}

void ATSCGameMode::AddTracer(const FVector& From, const FVector& To, const FLinearColor& Col)
{
	FTSCTracer T;
	T.From = From;
	T.To = To;
	T.Color = Col;
	T.Time = 0.22f;
	Tracers.Add(T);
}

void ATSCGameMode::StartDay(int32 InDay)
{
	Phase = ETSCPhase::Day;
	PlayMusic(DayMusic);  // keeps the current take running from one day to the next
	Day = InDay;
	DayTime = 0.f;
	DayFlags = 0;
	DayFoodMul = 1.f;
	if (Day == 1)
	{
		bDayOneOnboarding = !bAuto && UGameplayStatics::GetCurrentLevelName(this, true) == TEXT("Map_Saga");
		ShowBanner(TEXT("Three days. One night. Two sagas."), 3.f);
	}
	else if (Day == 2)
	{
		ShowBanner(TEXT("Day 2 - Growing the village.  Choose what to raise on the marked plot with the build bar below."), 12.f);
		if (bPendingTimber)
		{
			bPendingTimber = false;
			Wood += 40.f;
			EventTree->SetActorHiddenInGame(true);
			BlockRect(TreeX, TreeY, 1, 3, false);
			if (Hunter->IsAlive()) { Hunter->OrderWork(CellF(3.5f, 12.f), CellF(5.6f, 11.4f)); }
			Toast(TEXT("At dawn the fallen spruce is sawn up and the gate is cleared: +40 wood."));
		}
	}
	else
	{
		ShowBanner(TEXT("Day 3 - The fog is thickening, and sails have been sighted.  They come tonight. Finish your preparations."), 14.f);
		Plots[1].bAvailable = true;
		if (!Plots[1].bUsed) { Plots[1].Marker->SetActorHiddenInGame(false); }
		Toast(TEXT("A second plot has been cleared by the shore path."));
	}
	UpdateJobs();
}

void ATSCGameMode::SimDay(float Dt)
{
	// scripted beats; the clock waits while a scene is playing
	if (ActiveScene >= 0)
	{
		SimScene(ActiveScene, Dt);
	}
	else
	{
		DayTime += Dt;
	}
	auto Once = [this](int32 Bit, float T) { if (DayTime >= T && !(DayFlags & (1 << Bit))) { DayFlags |= 1 << Bit; return true; } return false; };
	if (ActiveScene < 0)
	{
		if (Day == 1)
		{
			if (Once(0, 24.f)) { StartScene(0); }
			if (Once(1, 62.f) && Woodcutter->IsAlive()) { Woodcutter->OrderHold(CellF(10.4f, 10.5f)); }
			if (Once(2, 74.f)) { StartScene(1); }
		}
		else if (Day == 2)
		{
			if (Once(1, 40.f) && Healer->IsAlive()) { Healer->OrderHold(CellF(14.4f, 20.3f)); Toast(TEXT("Sigrun walks to the stag grove to gather glowing mushrooms.")); }
			if (Once(2, 58.f)) { StartScene(2); }
		}
		else if (Day == 3)
		{
			if (Once(2, 46.f)) { StartScene(3); }
		}
	}

	SimConstruction(Dt);
	EconomyTimer += Dt;
	if (EconomyTimer >= EconomyInterval)
	{
		EconomyTimer -= EconomyInterval;
		EconomyTick();
	}

	if (DayTime >= DayLength && ActiveScene < 0)
	{
		if (Day < 3) { StartDay(Day + 1); }
		else { StartNight(); }
	}
}

// ----- economy ------------------------------------------------------------------------------------------------------

void ATSCGameMode::ComputeRates(float& OutWood, float& OutFood) const
{
	int32 Living = 0;
	for (const ATSCUnit* U : Units) { if (U && U->Team == ETSCTeam::Villager && U->IsAlive()) { ++Living; } }
	OutWood = NumWood * 1.f * ForestMul + 1.f + ExtraSawmills * 2.f + (Woodcutter && Woodcutter->IsAlive() ? 1.f : 0.f);
	OutFood = (NumFood * 1.5f * GardenMul + GroveBonus + DockBonus + (Hunter && Hunter->IsAlive() ? 2.f : 0.f) + (bStagShrine ? 2.f : 0.f)) * DayFoodMul - Living * 0.5f;
}

void ATSCGameMode::EconomyTick()
{
	float W, F;
	ComputeRates(W, F);
	WoodRate = FMath::RoundToInt(W);
	FoodRate = FMath::RoundToInt(F);
	Wood = FMath::Max(0.f, Wood + W);
	const bool bWasFed = Food > 0.f;
	Food = FMath::Max(0.f, Food + F);
	if (bWasFed && Food <= 0.f) { Toast(TEXT("The food stores are empty. Hungry defenders will fight poorly.")); }
	int32 Living = 0;
	for (const ATSCUnit* U : Units) { if (U && U->Team == ETSCTeam::Villager && U->IsAlive()) { ++Living; } }
	Pop = Living;
}

void ATSCGameMode::AdjustJob(ETSCJob Job, int32 Delta)
{
	if (IsFrozen()) { return; }
	int32& N = Job == ETSCJob::Wood ? NumWood : NumFood;
	const int32 Idle = Workers.Num() - NumWood - NumFood;
	if (Delta > 0 && Idle > 0)
	{
		++N;
		if (bDayOneOnboarding && Day == 1)
		{
			(Job == ETSCJob::Wood ? bAssignedWoodOnDayOne : bAssignedFoodOnDayOne) = true;
		}
	}
	else if (Delta < 0 && N > 0) { --N; }
	UpdateJobs();
	float W, F;
	ComputeRates(W, F);
	WoodRate = FMath::RoundToInt(W);
	FoodRate = FMath::RoundToInt(F);
}

void ATSCGameMode::UpdateJobs()
{
	Workers.RemoveAll([](const TObjectPtr<ATSCUnit>& U) { return !U || !U->IsAlive(); });
	NumWood = FMath::Clamp(NumWood, 0, Workers.Num());
	NumFood = FMath::Clamp(NumFood, 0, Workers.Num() - NumWood);
	NumIdle = Workers.Num() - NumWood - NumFood;
	int32 Living = 0;
	for (const ATSCUnit* U : Units) { if (U && U->Team == ETSCTeam::Villager && U->IsAlive()) { ++Living; } }
	Pop = Living;
	if (Phase == ETSCPhase::Night || Phase == ETSCPhase::Epilogue) { return; }

	const bool bForestOpen = ForestMul >= 1.f && !IsBlocked(TreeX, TreeY + 1) && !(ActiveScene == 1);
	int32 BuildPlot = -1;
	for (int32 I = 0; I < Plots.Num(); ++I) { if (Plots[I].bUsed && Plots[I].Progress < 1.f) { BuildPlot = I; } }

	for (int32 I = 0; I < Workers.Num(); ++I)
	{
		ATSCUnit* U = Workers[I];
		const ETSCJob Job = I < NumWood ? ETSCJob::Wood : (I < NumWood + NumFood ? ETSCJob::Food : ETSCJob::Idle);
		U->SetJob(Job);
		if (Job == ETSCJob::Wood)
		{
			static const float ChopY[6] = { 8.5f, 13.6f, 7.4f, 14.6f, 6.5f, 15.5f };
			const FVector A = bForestOpen ? CellF(7.5f, ChopY[I % 6]) : CellF(12.6f + (I % 3) * 0.5f, 6.5f + (I % 2) * 0.5f);
			U->OrderWork(A, CellF(10.6f + (I % 4) * 0.5f, 6.5f));
			U->Need = ETSCNeed::Wood;
		}
		else if (Job == ETSCJob::Food)
		{
			const int32 K = I - NumWood;
			const bool bForage = (K % 3 == 2) && GroveBonus > 0.f;
			const FVector A = bForage ? CellF(12.4f + K * 0.9f, 19.7f) : CellF(16.5f + (K % 4), 15.6f + (K / 4 % 3) * 0.9f);
			U->OrderWork(A, CellF(15.4f + (K % 5) * 0.7f, 13.7f));
			U->Need = ETSCNeed::Leaf;
		}
		else
		{
			const int32 K = I - NumWood - NumFood;
			U->Need = ETSCNeed::None;
			if (BuildPlot >= 0)
			{
				const FIntPoint C = Plots[BuildPlot].Cell;
				U->OrderWork(CellF(C.X + 0.4f + (K % 5) * 0.6f, C.Y + 2.35f), CellF(C.X + 0.4f + (K % 5) * 0.6f, C.Y + 2.9f));
			}
			else if (K % 2 == 0)
			{
				U->OrderWork(CellF(15.6f + (K % 6) * 0.4f, 3.7f), CellF(18.6f + (K % 4) * 0.5f, 14.f)); // pray at the hammer stone
			}
			else
			{
				U->OrderWork(CellF(14.2f + (K % 6) * 0.4f, 20.2f), CellF(18.6f + (K % 4) * 0.5f, 14.2f)); // pray at the stag stone
			}
		}
	}
}

// ----- construction -------------------------------------------------------------------------------------------------

int32 ATSCGameMode::BuildCost(EBuilding Type) const
{
	switch (Type)
	{
	case EBuilding::House: return 90;
	case EBuilding::Sawmill: return 110;
	case EBuilding::Healer: return 100;
	default: return 130;
	}
}

int32 ATSCGameMode::FreePlot() const
{
	for (int32 I = 0; I < Plots.Num(); ++I) { if (Plots[I].bAvailable && !Plots[I].bUsed) { return I; } }
	return -1;
}

bool ATSCGameMode::CanBuild(EBuilding Type) const
{
	if (Phase != ETSCPhase::Day || IsFrozen() || FreePlot() < 0 || Wood < BuildCost(Type)) { return false; }
	for (const FTSCPlot& P : Plots) { if (P.bUsed && P.Type == Type && (Type == EBuilding::HammerShrine || Type == EBuilding::StagShrine)) { return false; } }
	return true;
}

void ATSCGameMode::StartBuild(EBuilding Type)
{
	if (!CanBuild(Type)) { return; }
	const int32 Idx = FreePlot();
	FTSCPlot& P = Plots[Idx];
	Wood -= BuildCost(Type);
	P.bUsed = true;
	P.Type = Type;
	P.Progress = 0.f;
	P.Marker->SetActorHiddenInGame(true);
	const int32 W = Type == EBuilding::Sawmill ? 3 : 2;
	P.Building = SpawnPrim(CellCorner(P.Cell.X, P.Cell.Y));
	BuildPlotBuilding(P.Building, Type, Idx + 2);
	P.Building->SetBuildProgress(0.f);
	BlockRect(P.Cell.X, P.Cell.Y, W, 2);
	static const TCHAR* Names[5] = { TEXT("a house"), TEXT("a second sawmill"), TEXT("a healer's hut"), TEXT("a shrine to the Hammer"), TEXT("a shrine to the Stag") };
	Toast(TSCLoc::Fmt(TEXT("Your people begin raising {0}, tile by tile."), { TSCLoc::Tr(Names[(int32)Type]) }));
	UpdateJobs();
}

void ATSCGameMode::SimConstruction(float Dt)
{
	for (FTSCPlot& P : Plots)
	{
		if (!P.bUsed || P.Progress >= 1.f) { continue; }
		P.Progress = FMath::Min(1.f, P.Progress + Dt * (0.018f + 0.011f * NumIdle));
		P.Building->SetBuildProgress(P.Progress);
		if (P.Progress >= 1.f)
		{
			switch (P.Type)
			{
			case EBuilding::House:
				PopCap += 3;
				++HousesBuilt;
				for (int32 I = 0; I < 2; ++I)
				{
					ATSCUnit* W = SpawnUnit(ETSCRole::Worker, FString(), CellF(25.f, 11.6f + I * 0.6f));
					Workers.Add(W);
				}
				BuiltNames.Add(TEXT("a new house"));
				Toast(TEXT("The house stands. Two settlers arrive from the shore: more hands, and more spears for the night."));
				break;
			case EBuilding::Sawmill:
				++ExtraSawmills;
				BuiltNames.Add(TEXT("a second sawmill"));
				Toast(TEXT("The second sawmill is running: +2 wood income."));
				break;
			case EBuilding::Healer:
				++ExtraHealers;
				BuiltNames.Add(TEXT("a healer's hut"));
				Toast(TEXT("The healer's hut is ready: the wounded will be tended on the final night."));
				break;
			case EBuilding::HammerShrine:
				bHammerShrine = true;
				BuiltNames.Add(TEXT("a shrine to the Hammer"));
				Toast(TEXT("The Hammer shrine is consecrated: your defenders will strike harder."));
				break;
			case EBuilding::StagShrine:
				bStagShrine = true;
				BuiltNames.Add(TEXT("a shrine to the Stag"));
				Toast(TEXT("The Stag shrine is consecrated: the forest gives more food and sends fewer horrors."));
				break;
			}
			UpdateJobs();
		}
	}
}

// =====================================================================================================================
// Scenes and the CUT! mechanic
// =====================================================================================================================

void ATSCGameMode::DefineScenes()
{
	Scenes.SetNum(4);
	{
		FTSCScene& S = Scenes[0];
		S.Title = TEXT("The Boar in the Garden");
		S.Region = TSCRegion::Garden;
		S.Alert = TEXT("A boar has broken out of the forest and is charging the vegetable garden!");
		S.LivingName = TEXT("LIVING SAGA - Wooden fence");
		S.LivingText = TEXT("Lose 25 food to the boar and halve today's harvest. Keep the fertile soil.");
		S.AshenName = TEXT("ASHEN SAGA - Stone wall");
		S.AshenText = TEXT("The wall saves the crops. Garden yields drop 25% for the rest of the saga.");
	}
	{
		FTSCScene& S = Scenes[1];
		S.Title = TEXT("The Spruce at the Gate");
		S.Region = TSCRegion::Forest;
		S.Alert = TEXT("The spruce has fallen across the west gate! Hakon the hunter is trapped outside, and the fog is moving.");
		S.LivingName = TEXT("THE LIVING SAGA - The fallen spruce");
		S.LivingText = TEXT("Hakon stands alone against the fog-born and will not live to see the gate cleared. At dawn the spruce is sawn up: +40 wood, and the forest keeps giving timber.");
		S.AshenName = TEXT("THE ASHEN SAGA - The burnt stump");
		S.AshenText = TEXT("Here the forest burned years ago. No spruce stood by the gate, so nothing blocks it and Hakon walks home. But scorched stumps give no timber: no +40 wood, and wood from the forest falls by 40% for good.");
	}
	{
		FTSCScene& S = Scenes[2];
		S.Title = TEXT("Spores in the Stag Grove");
		S.Region = TSCRegion::Grove;
		S.Alert = TEXT("Sigrun the healer has breathed the grove's spores and fallen! No one else can tend the wounded.");
		S.LivingName = TEXT("THE LIVING SAGA - The glowing grove");
		S.LivingText = TEXT("The spores take Sigrun before nightfall. The grove keeps feeding the village (+2 food income), but no healer will tend your defenders on the final night.");
		S.AshenName = TEXT("THE ASHEN SAGA - The dead roots");
		S.AshenText = TEXT("Here the grove burned in the old war and nothing grows that could harm her. Sigrun lives. But the glowing mushrooms are gone (-2 food income) and the Stag's blessing is weakened.");
	}
	{
		FTSCScene& S = Scenes[3];
		S.Title = TEXT("Scouts at the Dock");
		S.Region = TSCRegion::Dock;
		S.Alert = TEXT("A scout boat has made fast to the dock! Torvald the shield-bearer runs to meet the raiders alone.");
		S.LivingName = TEXT("THE LIVING SAGA - The timber dock");
		S.LivingText = TEXT("Torvald drives the scouts off but is badly wounded, and they carry away the stores (-30 food). The dock stands: fishing goes on (+2 food income), and tonight the raiders land in force.");
		S.AshenName = TEXT("THE ASHEN SAGA - The charred posts");
		S.AshenText = TEXT("Here the dock burned with the fleet. The scouts find nowhere to land and row away; Torvald is unhurt and tonight fewer raiders struggle ashore. But the longship is a wreck and the fishing is gone (-2 food income).");
	}
}

void ATSCGameMode::StartScene(int32 Idx)
{
	FTSCScene& S = Scenes[Idx];
	S.bStarted = true;
	S.Step = 0;
	S.Timer = 0.f;
	ActiveScene = Idx;
	UE_LOG(LogSaga, Display, TEXT("[Saga] Scene %d starts: %s"), Idx, *S.Title);

	if (Idx == 0)
	{
		Boar = SpawnUnit(ETSCRole::Boar, FString(), CellF(0.5f, 12.f));
		Boar->Speed = 185.f;
		Boar->OrderHold(CellF(18.f, 14.6f));
		S.bCutOpenable = true;
		ShowBanner(S.Alert, 60.f);
	}
	else if (Idx == 1)
	{
		if (Woodcutter->IsAlive()) { Woodcutter->OrderHold(CellF(10.4f, 10.5f)); }
		if (Hunter->IsAlive()) { Hunter->OrderHold(CellF(3.5f, 12.f)); }
		UpdateJobs(); // call the woodcutters in from the forest
		ShowBanner(TEXT("Ulf the woodcutter sets his axe to the old spruce beside the west gate..."), 20.f);
	}
	else if (Idx == 2)
	{
		if (!Healer->IsAlive()) { FinishScene(Idx); return; }
		Healer->OrderHold(CellF(14.4f, 20.3f));
	}
	else if (Idx == 3)
	{
		RaiderBoat->SetActorHiddenInGame(false);
		RaiderBoat->SetActorLocation(CellF(40.f, 12.f) + FVector(0.f, 0.f, -4.f));
		ShowBanner(TEXT("A dark sail slips out of the fog toward the dock..."), 20.f);
	}
}

void ATSCGameMode::SimScene(int32 Idx, float Dt)
{
	FTSCScene& S = Scenes[Idx];
	S.Timer += Dt;

	if (Idx == 0)
	{
		if (Boar && (Boar->AtHoldPos() || S.Timer > 40.f)) { ResolveLiving(Idx); }
	}
	else if (Idx == 1)
	{
		if (S.Step == 0)
		{
			// Ulf walks up and chops
			const bool bThere = !Woodcutter->IsAlive() || Woodcutter->AtHoldPos();
			if (bThere || S.Timer > 14.f)
			{
				if (Woodcutter->IsAlive()) { Woodcutter->Teleport(CellF(10.4f, 10.5f)); Woodcutter->OrderWork(CellF(10.4f, 10.5f), CellF(10.4f, 10.5f)); Woodcutter->SetActorRotation(FRotator(0.f, 180.f, 0.f)); }
				S.Step = 1;
				S.Timer = 0.f;
			}
		}
		else if (S.Step == 1 && S.Timer > 4.5f)
		{
			S.Step = 2;
			S.Timer = 0.f;
		}
		else if (S.Step == 2)
		{
			// the spruce falls south, across the gateway
			const float A = FMath::Clamp(S.Timer / 1.3f, 0.f, 1.f);
			const float Ang = A * A * HALF_PI * 0.985f;
			EventTree->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, FVector(0.f, FMath::Sin(Ang), FMath::Cos(Ang))));
			if (A >= 1.f)
			{
				BlockRect(TreeX, TreeY + 1, 1, 2, true);
				if (Woodcutter->IsAlive()) { Woodcutter->OrderHold(CellF(10.6f, 10.5f)); }
				if (Hunter->IsAlive())
				{
					Hunter->OrderHold(CellF(7.5f, 12.f));
					Hunter->bFights = true;
				}
				for (int32 I = 0; I < 6; ++I)
				{
					ATSCUnit* C = SpawnUnit(ETSCRole::Creature, FString(), CellF(-3.f - (I / 3) * 1.5f, 10.4f + (I % 3) * 1.1f));
					C->MaxHP = C->HP = 62.f;
					C->AggroRange = 2600.f;
					C->OrderHold(CellF(6.6f, 11.4f + (I % 3) * 0.5f));
					EventUnits.Add(C);
				}
				S.bCutOpenable = true;
				S.Step = 3;
				S.Timer = 0.f;
				ShowBanner(S.Alert, 90.f);
				Toast(TEXT("The spruce crashes down across the gateway. Hakon is on the wrong side of it."));
			}
		}
		else if (S.Step == 3)
		{
			int32 Alive = 0;
			for (ATSCUnit* C : EventUnits) { if (C && C->IsAlive()) { ++Alive; } }
			if (!Hunter->IsAlive() || Alive == 0 || S.Timer > 80.f) { ResolveLiving(Idx); }
		}
	}
	else if (Idx == 2)
	{
		if (S.Step == 0 && (Healer->AtHoldPos() || S.Timer > 20.f))
		{
			Healer->Teleport(CellF(14.4f, 20.3f));
			Healer->bDowned = true;
			Healer->Need = ETSCNeed::Heal;
			S.Step = 1;
			S.Timer = 0.f;
			S.bCutOpenable = true;
			ShowBanner(S.Alert, 60.f);
		}
		else if (S.Step == 1 && S.Timer > 24.f)
		{
			ResolveLiving(Idx);
		}
	}
	else if (Idx == 3)
	{
		if (S.Step == 0)
		{
			const float A = FMath::Clamp(S.Timer / 6.f, 0.f, 1.f);
			RaiderBoat->SetActorLocation(FMath::Lerp(CellF(40.f, 12.f), CellF(33.6f, 12.f), FMath::SmoothStep(0.f, 1.f, A)) + FVector(0.f, 0.f, -4.f));
			if (A >= 1.f)
			{
				for (int32 I = 0; I < 4; ++I)
				{
					ATSCUnit* R = SpawnUnit(ETSCRole::Raider, FString(), CellF(31.6f, 11.3f + I * 0.45f));
					R->OrderHold(CellF(27.6f + (I % 2) * 0.6f, 11.4f + (I / 2) * 0.8f));
					R->Damage = 5.f;
					EventUnits.Add(R);
				}
				if (Shield->IsAlive())
				{
					Shield->OrderHold(CellF(26.4f, 12.f));
					Shield->bInvulnerable = true;
					Shield->BarTimer = 30.f;
				}
				S.Step = 1;
				S.Timer = 0.f;
				S.bCutOpenable = true;
				ShowBanner(S.Alert, 60.f);
			}
		}
		else if (S.Step == 1 && S.Timer > 22.f)
		{
			ResolveLiving(Idx);
		}
	}
}

void ATSCGameMode::ResolveLiving(int32 Idx)
{
	FTSCScene& S = Scenes[Idx];
	if (Idx == 0)
	{
		Food = FMath::Max(0.f, Food - 25.f);
		DayFoodMul = 0.6f;
		for (int32 I = 0; I < CropParts.Num(); ++I) { if (I % 5 != 0) { Garden->SetShown(CropParts[I], false); } }
		Garden->Refresh();
		if (Boar) { Boar->OrderLeave(CellF(-4.f, 12.f)); }
		Toast(TEXT("The boar tramples the garden: -25 food, and the harvest is halved for today."));
	}
	else if (Idx == 1)
	{
		for (ATSCUnit* C : EventUnits) { if (C && C->IsAlive()) { C->OrderLeave(CellF(-5.f, 12.f)); } }
		EventUnits.Reset();
		if (Hunter->IsAlive())
		{
			Toast(TEXT("Against all odds Hakon fights the fog-born off and waits out the night by the wall."));
			Hunter->OrderHold(CellF(7.5f, 12.f));
		}
		else
		{
			Toast(TEXT("Hakon the hunter falls outside the gate he could not reach."));
		}
		bPendingTimber = true;
	}
	else if (Idx == 2)
	{
		Healer->bDowned = false;
		Healer->Kill();
		Toast(TEXT("The spores take Sigrun the healer. The grove glows on, indifferent."));
	}
	else if (Idx == 3)
	{
		Food = FMath::Max(0.f, Food - 30.f);
		for (int32 C : CrateParts) { DockActor->SetShown(C, false); }
		DockActor->Refresh();
		for (ATSCUnit* R : EventUnits) { if (R && R->IsAlive()) { R->bFights = false; R->OrderLeave(CellF(31.6f, 12.f)); } }
		EventUnits.Reset();
		if (Shield->IsAlive())
		{
			Shield->bInvulnerable = false;
			Shield->HP = Shield->MaxHP * 0.45f;
			Shield->Need = ETSCNeed::Heal;
			Shield->BarTimer = 20.f;
			Shield->OrderWork(CellF(24.6f, 12.f), CellF(21.f, 12.4f));
			bShieldWounded = true;
		}
		S.Step = 5; // boat leaves in the day sim below
		Toast(TEXT("The scouts are driven off, but Torvald is badly wounded and the stores are gone: -30 food."));
	}
	FinishScene(Idx);
}

void ATSCGameMode::TransitionRegion(int32 Region, const FVector& Origin)
{
	for (ATSCPrimActor* P : Prims)
	{
		if (P && P->Region == Region)
		{
			P->StartSagaTransition(1.f, FMath::Min(FVector::Dist2D(P->GetActorLocation(), Origin) / 2400.f, 2.4f));
		}
	}
	if (Terrain && Region >= TSCRegion::Forest && Region <= TSCRegion::Grove)
	{
		const FIntRect& Cells = Region == TSCRegion::Forest ? ForestRect
			: Region == TSCRegion::Garden ? GardenRect : GroveRect;
		Terrain->StartGroundTransition(Region,
			FBox2D(FVector2D(CellCorner(Cells.Min.X, Cells.Min.Y)), FVector2D(CellCorner(Cells.Max.X, Cells.Max.Y))),
			FVector2D(Origin));
	}
	SpawnEmbers(Region, 60);
	TransitionTimer = 3.4f;
	FlashTimer = 0.5f;
}

void ATSCGameMode::ApplyAshen(int32 Idx)
{
	FTSCScene& S = Scenes[Idx];
	S.bAshen = true;
	TransitionTitle = S.AshenName;
	if (Idx == 0)
	{
		TransitionRegion(TSCRegion::Garden, CellF(18.f, 16.5f));
		GardenMul = 0.75f;
		if (Boar) { Boar->OrderLeave(CellF(-4.f, 12.f)); }
		Toast(TEXT("EDIT: the boar turns away from an old stone wall. The crops are safe, but the ash-poor soil will always yield less."));
	}
	else if (Idx == 1)
	{
		TransitionRegion(TSCRegion::Forest, CellCenter(TreeX, TreeY));
		ForestMul = 0.6f;
		bPendingTimber = false;
		BlockRect(TreeX, TreeY + 1, 1, 2, false);
		for (ATSCUnit* C : EventUnits) { if (C && C->IsAlive()) { C->bFights = false; C->OrderLeave(CellF(-5.f, 12.f)); } }
		EventUnits.Reset();
		if (Hunter->IsAlive())
		{
			Hunter->bInvulnerable = true;
			Hunter->bFights = false;
			Hunter->HP = Hunter->MaxHP;
			Hunter->OrderWork(CellF(10.2f, 12.3f), CellF(12.f, 13.3f));
		}
		if (Woodcutter->IsAlive()) { Woodcutter->OrderWork(CellF(13.f, 6.5f), CellF(11.5f, 6.5f)); }
		Toast(TEXT("EDIT: the forest burned years ago. The gate stands clear and Hakon walks home, but that timber is ash."));
	}
	else if (Idx == 2)
	{
		TransitionRegion(TSCRegion::Grove, CellCenter(15, 21));
		GroveBonus = 0.f;
		Healer->bDowned = false;
		Healer->Need = ETSCNeed::None;
		Healer->OrderWork(CellF(22.6f, 11.5f), CellF(21.4f, 12.3f));
		Toast(TEXT("EDIT: the grove is dead roots and ash. Sigrun rises unharmed, but the glowing mushrooms will never feed you again."));
	}
	else if (Idx == 3)
	{
		TransitionRegion(TSCRegion::Dock, CellF(28.f, 12.f));
		DockBonus = 0.f;
		for (ATSCUnit* R : EventUnits) { if (R && R->IsAlive()) { R->bGone = true; R->SetActorHiddenInGame(true); } }
		EventUnits.Reset();
		if (Shield->IsAlive())
		{
			Shield->bInvulnerable = false;
			Shield->HP = Shield->MaxHP;
			Shield->OrderWork(CellF(9.7f, 11.3f), CellF(25.4f, 12.f));
		}
		S.Step = 5;
		Toast(TEXT("EDIT: the dock burned with the fleet. The scouts row away and Torvald is unhurt, but the longship and the fishing are lost."));
	}
	S.Timer = -1.f; // marks the scene whose region is burning (ember spawning)
	UpdateJobs();
}

void ATSCGameMode::FinishScene(int32 Idx)
{
	Scenes[Idx].bResolved = true;
	Scenes[Idx].bCutOpenable = false;
	ActiveScene = -1;
	BannerTimer = 0.f;
	UE_LOG(LogSaga, Display, TEXT("[Saga] Scene %d resolved: %s"), Idx, Scenes[Idx].bAshen ? TEXT("ASHEN") : TEXT("LIVING"));
	UpdateJobs();
	float W, F;
	ComputeRates(W, F);
	WoodRate = FMath::RoundToInt(W);
	FoodRate = FMath::RoundToInt(F);
}

bool ATSCGameMode::CanCut() const
{
	return Phase == ETSCPhase::Day && ActiveScene >= 0 && Scenes[ActiveScene].bCutOpenable && !Scenes[ActiveScene].bResolved && EditsLeft > 0
		&& !bCutOpen && TransitionTimer <= 0.f;
}

ETSCDayOnePrompt ATSCGameMode::GetDayOnePrompt() const
{
	if (!bDayOneOnboarding || Phase != ETSCPhase::Day || Day != 1 || IsFrozen())
	{
		return ETSCDayOnePrompt::None;
	}
	if (ActiveScene == 0 && !bTriedFirstCut && CanCut()) { return ETSCDayOnePrompt::Cut; }
	if (ActiveScene >= 0) { return ETSCDayOnePrompt::None; }
	if (!bAssignedWoodOnDayOne) { return ETSCDayOnePrompt::Wood; }
	if (!bAssignedFoodOnDayOne) { return ETSCDayOnePrompt::Food; }
	if (FreePlot() >= 0) { return ETSCDayOnePrompt::Build; }
	return ETSCDayOnePrompt::None;
}

bool ATSCGameMode::IsFirstCutFramed() const
{
	return bFramingFirstCut && (bCutOpen || TransitionTimer > 0.f);
}

void ATSCGameMode::PressCut()
{
	if (!CanCut()) { return; }
	bCutOpen = true;
	SelectedTake = 0;
	bFramingFirstCut = bDayOneOnboarding && ActiveScene == 0;
	if (bFramingFirstCut)
	{
		bTriedFirstCut = true;
		SetCameraView(CellF(18.f, 16.5f), 8000.f);
	}
}

void ATSCGameMode::ConfirmCut()
{
	if (!bCutOpen) { return; }
	bCutOpen = false;
	if (SelectedTake == 1 && ActiveScene >= 0)
	{
		const int32 Idx = ActiveScene;
		--EditsLeft;
		for (FTSCScene& S : Scenes) { if (S.Timer < 0.f) { S.Timer = 0.f; } }
		ApplyAshen(Idx);
		FinishScene(Idx);
		if (bFramingFirstCut)
		{
			Toast(FString::Printf(TEXT("%d cuts left"), EditsLeft));
		}
	}
}

// =====================================================================================================================
// Final night
// =====================================================================================================================

void ATSCGameMode::SetGate(bool bClosed)
{
	bGateClosed = bClosed;
	SetBlocked(GateX, GateY, bClosed);
	SetBlocked(GateX, GateY + 1, bClosed);
}

void ATSCGameMode::StartNight()
{
	Phase = ETSCPhase::Night;
	PlayMusic(NightMusic);
	NightTime = 0.f;
	DayFlags = 0;
	BlockRect(TreeX, TreeY + 1, 1, 2, false);
	bPendingTimber = false;
	for (FTSCPlot& P : Plots) { P.Marker->SetActorHiddenInGame(true); }
	for (ATSCUnit* U : Units) { if (U && U->Team != ETSCTeam::Villager && U->IsAlive()) { U->bGone = true; U->SetActorHiddenInGame(true); } }
	RaiderBoat->SetActorHiddenInGame(true);

	SetGate(true);
	StartNightWood = Wood;
	const float Spent = FMath::Min(Wood, 200.f);
	Wood -= Spent;
	GateMaxHP = GateHP = 190.f + Spent * 0.8f + (Scenes[1].bAshen ? 80.f : 0.f);
	HallMaxHP = HallHP = 420.f;
	bHungryNight = Food < 20.f;
	bFedNight = Food >= 110.f;
	const float HpMul = bHungryNight ? 0.72f : (bFedNight ? 1.18f : 1.f);
	const float DmgMul = bHammerShrine ? 1.3f : 1.f;

	auto Arm = [&](ATSCUnit* U, const FVector& Pos)
	{
		U->bDefender = true;
		U->bFights = true;
		U->bInvulnerable = false;
		U->Need = ETSCNeed::None;
		U->SetArmed(true);
		U->MaxHP *= HpMul;
		U->HP = FMath::Min(U->HP * HpMul, U->MaxHP);
		U->Damage *= DmgMul;
		U->OrderHold(Pos);
		U->BarTimer = 999.f;
	};
	if (Hunter->IsAlive()) { Hunter->HP = Hunter->MaxHP; Arm(Hunter, CellF(9.9f, 12.f)); }
	if (Woodcutter->IsAlive()) { Arm(Woodcutter, CellF(10.4f, 11.2f)); }
	if (Shield->IsAlive())
	{
		if (!bShieldWounded) { Shield->HP = Shield->MaxHP; }
		Arm(Shield, CellF(25.8f, 12.f));
	}
	Workers.RemoveAll([](const TObjectPtr<ATSCUnit>& U) { return !U || !U->IsAlive(); });
	const int32 NumDef = FMath::Min(Workers.Num(), 6 + HousesBuilt);
	for (int32 I = 0; I < Workers.Num(); ++I)
	{
		ATSCUnit* U = Workers[I];
		if (I < NumDef)
		{
			const int32 K = I / 2;
			if (I % 2 == 0) { Arm(U, CellF(10.6f + (K % 2) * 0.6f, 11.f + K * 0.7f)); }
			else { Arm(U, CellF(25.2f - (K % 2) * 0.6f, 11.f + K * 0.7f)); }
		}
		else
		{
			U->Need = ETSCNeed::None;
			U->OrderShelter(HallDoor() + FVector((I % 3 - 1) * 60.f, 0.f, 0.f));
		}
	}
	if (Healer->IsAlive()) { Healer->OrderHold(CellF(17.f, 14.4f)); }

	NightVillagersAtStart = 0;
	for (const ATSCUnit* U : Units) { if (U && U->Team == ETSCTeam::Villager && U->IsAlive()) { ++NightVillagersAtStart; } }
	NightDefendersLost = 0;
	EnemiesSlain = 0;
	ShowBanner(TEXT("The Final Night.  The gate is barred with the last of the timber. Creatures gather in the fog; raiders pull for the shore."), 12.f);
	Toast(TSCLoc::Tr(FString::Printf(TEXT("%d wood braces the gate (%d strength)."), FMath::RoundToInt(Spent), FMath::RoundToInt(GateMaxHP)))
		+ TSCLoc::Tr(bHungryNight ? TEXT(" Your defenders are weak with hunger.") : (bFedNight ? TEXT(" Your defenders are well fed.") : TEXT("")))
		+ TSCLoc::Tr(bHammerShrine ? TEXT(" The Hammer blesses their axes.") : TEXT("")));
}

void ATSCGameMode::GetAssaultGoal(const ATSCUnit* U, FVector& OutPos, ETSCStructure& OutStruct) const
{
	const float J = ((int32)(U->GetUniqueID() % 5) - 2) * 34.f;
	if (U->Team == ETSCTeam::Creature && bGateClosed && !bGateBroken)
	{
		OutPos = CellF(7.55f, 12.f) + FVector(((U->GetUniqueID() / 5) % 3) * -60.f, J * 1.6f, 0.f);
		OutStruct = ETSCStructure::Gate;
		return;
	}
	OutPos = HallDoor() + FVector(J * 3.f, ((U->GetUniqueID() / 5) % 2) * 50.f, 0.f);
	OutStruct = ETSCStructure::Longhouse;
}

void ATSCGameMode::DamageStructure(ETSCStructure S, float Amount)
{
	if (Phase != ETSCPhase::Night) { return; }
	if (S == ETSCStructure::Gate)
	{
		if (bGateBroken) { return; }
		GateHP -= Amount * 2.4f;
		if (GateHP <= 0.f)
		{
			GateHP = 0.f;
			bGateBroken = true;
			bWestBreached = true;
			SetBlocked(GateX, GateY, false);
			SetBlocked(GateX, GateY + 1, false);
			Toast(TEXT("The west gate splinters! The fog-born pour through."));
		}
	}
	else if (S == ETSCStructure::Longhouse)
	{
		HallHP -= Amount;
		if (!bEastBreached && !bWestBreached) { bEastBreached = true; }
		if (HallHP <= 0.f)
		{
			HallHP = 0.f;
			EndNight(true);
		}
	}
}

void ATSCGameMode::OnUnitDied(ATSCUnit* U)
{
	if (U->Team == ETSCTeam::Villager)
	{
		if (U->bNamed && Phase == ETSCPhase::Night) { Toast(TSCLoc::Fmt(TEXT("{0} has fallen."), { TSCLoc::Tr(U->DisplayName) })); }
		if (Phase == ETSCPhase::Night) { ++NightDefendersLost; }
		else { UpdateJobs(); }
	}
	else if (Phase == ETSCPhase::Night)
	{
		++EnemiesSlain;
	}
}

void ATSCGameMode::SimNight(float Dt)
{
	NightTime += Dt;
	auto Once = [this](int32 Bit, float T) { if (NightTime >= T && !(DayFlags & (1 << Bit))) { DayFlags |= 1 << Bit; return true; } return false; };

	// creatures: three waves out of the fog. The Stag's blessing thins them.
	int32 PerWave = 3;
	const int32 Calm = bStagShrine ? (Scenes[2].bAshen ? 0 : 1) : 0; // the Stag calls some of them back
	const int32 LastCalm = bStagShrine ? 1 : 0;
	auto CreatureWave = [&](int32 N)
	{
		for (int32 I = 0; I < N; ++I)
		{
			ATSCUnit* C = SpawnUnit(ETSCRole::Creature, FString(), CellF(-3.f - (I / 3) * 1.4f, 10.2f + (I % 4) * 0.9f));
			C->MaxHP = C->HP = 50.f;
			C->Damage = 8.f;
			C->OrderAssault();
		}
	};
	if (Once(0, 5.f)) { CreatureWave(PerWave - Calm); Toast(TEXT("Pale shapes drift out of the fog toward the palisade.")); }
	if (Once(1, 24.f)) { CreatureWave(PerWave - Calm); }
	if (Once(2, 44.f)) { CreatureWave(PerWave + 1 - LastCalm); }

	// raiders: they land at the dock, or wade ashore if the dock is ash
	const bool bDockAshen = Scenes[3].bAshen;
	auto RaiderWave = [&](int32 N)
	{
		for (int32 I = 0; I < N; ++I)
		{
			const FVector P = bDockAshen ? CellF(29.6f, 9.f + I * 1.4f) : CellF(31.5f, 11.3f + (I % 3) * 0.5f);
			ATSCUnit* R = SpawnUnit(ETSCRole::Raider, FString(), P);
			if (bDockAshen) { R->Speed *= 0.55f; R->HP = R->MaxHP = R->MaxHP * 0.85f; }
			R->OrderAssault();
		}
	};
	if (Once(3, 9.f)) { if (Ocean) { Ocean->PlayLanding(FVector2D(CellF(bDockAshen ? 29.6f : 31.5f, 11.5f)), 0.8f); } RaiderWave(bDockAshen ? 2 : 3); Toast(bDockAshen ? TEXT("With no dock to land at, a few raiders wade ashore through the surf.") : TEXT("Raiders leap onto the dock!")); }
	if (Once(4, 34.f)) { RaiderWave(bDockAshen ? 1 : 3); }

	// healing
	const float Heal = (Healer->IsAlive() ? 2.6f : 0.f) + ExtraHealers * 1.6f;
	int32 EnemiesAlive = 0;
	for (ATSCUnit* U : Units)
	{
		if (!U || !U->IsAlive()) { continue; }
		if (U->Team == ETSCTeam::Villager)
		{
			if (U->bDefender && Heal > 0.f && U->HP < U->MaxHP) { U->HP = FMath::Min(U->MaxHP, U->HP + Heal * Dt); }
		}
		else
		{
			++EnemiesAlive;
		}
	}
	// once enemies are at the longhouse door, every defender falls back to it
	if (HallHP < HallMaxHP && Once(5, 0.f))
	{
		int32 K = 0;
		for (ATSCUnit* U : Units)
		{
			if (U && U->IsAlive() && U->bDefender) { U->OrderHold(HallDoor() + FVector((K % 5 - 2) * 70.f, 60.f + (K / 5) * 60.f, 0.f)); ++K; }
		}
		Toast(TEXT("They are at the longhouse door! Every defender falls back to the hearth."));
	}

	const bool bAllSpawned = NightTime > 46.f;
	if (bAllSpawned && EnemiesAlive == 0 && Once(7, 0.f)) { DayFlags |= 1 << 8; NightTime = FMath::Max(NightTime, NightLength - 3.f); Toast(TEXT("The last of them falls. The night goes quiet.")); }
	if (NightTime >= NightLength)
	{
		EndNight(false);
	}
}

void ATSCGameMode::EndNight(bool bHallFell)
{
	if (Phase != ETSCPhase::Night) { return; }
	for (ATSCUnit* U : Units)
	{
		if (U && U->IsAlive() && U->Team != ETSCTeam::Villager) { U->bFights = false; U->OrderLeave(U->Team == ETSCTeam::Creature ? CellF(-5.f, 12.f) : CellF(31.5f, 12.f)); }
	}
	BuildEpilogue(bHallFell);
	Phase = ETSCPhase::Epilogue;
	if (!bHallFell && VictoryMusic)
	{
		// Victory sting, then the calm theme under the epilogue.
		PlayMusic(VictoryMusic);
		GetWorldTimerManager().SetTimer(MusicTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { PlayMusic(MenuMusic); }),
			FMath::Max(1.f, VictoryMusic->GetDuration()), false);
	}
	else
	{
		PlayMusic(MenuMusic);
	}
	for (const FString& L : Epilogue) { UE_LOG(LogSaga, Display, TEXT("[Epilogue] %s"), *L); }
}

void ATSCGameMode::BuildEpilogue(bool bHallFell)
{
	Epilogue.Reset();
	int32 Survivors = 0;
	for (const ATSCUnit* U : Units) { if (U && U->Team == ETSCTeam::Villager && U->IsAlive()) { ++Survivors; } }
	const int32 NumAshen = (Scenes[0].bAshen ? 1 : 0) + (Scenes[1].bAshen ? 1 : 0) + (Scenes[2].bAshen ? 1 : 0) + (Scenes[3].bAshen ? 1 : 0);
	const bool bHakon = Hunter->IsAlive(), bUlf = Woodcutter->IsAlive(), bSigrun = Healer->IsAlive(), bTorvald = Shield->IsAlive();

	if (bHallFell) { EpilogueTitle = TEXT("THE SAGA OF THE COLD HEARTH"); }
	else if (NumAshen >= 3) { EpilogueTitle = TEXT("THE SAGA OF THE ASHEN HOLD"); }
	else if (NumAshen == 0) { EpilogueTitle = TEXT("THE SAGA OF THE GREEN CLEARING"); }
	else { EpilogueTitle = TEXT("THE SAGA OF TWO TELLINGS"); }

	Epilogue.Add(TEXT("Hear me, the skald, for I sing it as the Saga Cutter cut it."));

	// the garden
	Epilogue.Add(Scenes[0].bAshen
		? TEXT("When the boar came, you gave the garden a wall of old burnt stone. Not a cabbage was lost, and ever after the thin grey soil gave a little less.")
		: TEXT("When the boar came, you let the saga run. It trampled the green rows, the gardeners cursed, and the good soil forgave them by next morning."));

	// the gate
	if (Scenes[1].bAshen)
	{
		Epilogue.Add(bHakon
			? TEXT("You burned the western forest out of the telling so that no spruce could bar the gate. Hakon the hunter walked home through ash, and on the last night it was his bow that held that gate.")
			: TEXT("You burned the western forest out of the telling to bring Hakon home through a clear gate. He held it on the last night, and he fell holding it."));
	}
	else
	{
		Epilogue.Add(bHakon
			? TEXT("The spruce fell across the gate and you did not cut the scene. Hakon stood alone before the fog and, past all reckoning, lived to see the dawn saws.")
			: TEXT("The spruce fell across the gate and you let it lie. Hakon the hunter died on the wrong side of it, and in the morning his people sawed that timber into planks and did not speak."));
	}

	// the grove
	if (Scenes[2].bAshen)
	{
		Epilogue.Add(bSigrun
			? TEXT("You traded the glowing grove for Sigrun the healer. The stag stone stands among dead roots now, but her hands closed many wounds on the final night.")
			: TEXT("You traded the glowing grove for Sigrun the healer, and still the night took her at the longhouse door."));
	}
	else if (!Scenes[2].bStarted || bSigrun)
	{
		Epilogue.Add(TEXT("Sigrun the healer still gathers by the stag stone, and the grove still glows."));
	}
	else
	{
		Epilogue.Add(TEXT("You kept the grove and its glowing harvest. Sigrun the healer breathed its spores and did not rise, and on the last night the wounded lay untended."));
	}

	// the dock
	if (Scenes[3].bAshen)
	{
		Epilogue.Add(bTorvald
			? TEXT("You let the dock burn in an older war. The raiders found only black posts and came ashore few and slow, where Torvald Shield-Bearer, whole and unhurt, was waiting. No ship will ever sail from that shore again.")
			: TEXT("You let the dock burn in an older war, so the raiders came few and slow. Torvald Shield-Bearer met them in the surf and did not come back from it."));
	}
	else
	{
		Epilogue.Add(bTorvald
			? TEXT("You kept the dock and the longship. Torvald Shield-Bearer bled for it at dusk and stood again at night, and the fishing boats will go out tomorrow.")
			: TEXT("You kept the dock and the longship. Torvald Shield-Bearer, already bleeding from the scouts' axes, died on its planks when the raiders landed in force."));
	}

	// what was built
	if (BuiltNames.Num() > 0)
	{
		FString B = TSCLoc::Tr(BuiltNames[0]);
		if (BuiltNames.Num() > 1) { B += TSCLoc::Tr(TEXT(" and ")) + TSCLoc::Tr(BuiltNames[1]); }
		FString Why;
		if (bHammerShrine) { Why += TSCLoc::Tr(TEXT(" The Hammer put weight in every axe.")); }
		if (bStagShrine) { Why += TSCLoc::Tr(TEXT(" The Stag called some of the fog back into the trees.")); }
		if (ExtraHealers > 0) { Why += TSCLoc::Tr(TEXT(" The new hut's fires boiled bandages all night.")); }
		if (HousesBuilt > 0) { Why += TSCLoc::Tr(TEXT(" The newcomers took up shields beside the old families.")); }
		if (ExtraSawmills > 0) { Why += TSCLoc::Tr(TEXT(" Its timber went into the gate.")); }
		Epilogue.Add(TSCLoc::Fmt(TEXT("On the empty ground you raised {0}.{1}"), { B, Why }));
	}
	else
	{
		Epilogue.Add(TEXT("The marked plot stayed empty; nothing new was raised before the night came."));
	}

	// the night
	if (bGateBroken)
	{
		Epilogue.Add(FString::Printf(TEXT("The west gate, braced with %d wood, broke, and pale things drifted between the houses."), FMath::RoundToInt(FMath::Min(StartNightWood, 200.f))));
	}
	else
	{
		Epilogue.Add(FString::Printf(TEXT("The west gate, braced with %d wood, held until dawn."), FMath::RoundToInt(FMath::Min(StartNightWood, 200.f))));
	}
	if (bHungryNight) { Epilogue.Add(TEXT("They fought on empty bellies, for the stores had run dry.")); }

	const int32 Lost = NightDefendersLost;
	if (bHallFell)
	{
		Epilogue.Add(FString::Printf(TEXT("%d fell that night, and at the end the longhouse hearth was kicked cold. %d walked away along the shore, carrying the story and little else."), Lost, Survivors));
	}
	else if (Lost == 0)
	{
		Epilogue.Add(FString::Printf(TEXT("Not one of your people fell. %d of them watched the sun come up over the water."), Survivors));
	}
	else
	{
		Epilogue.Add(FString::Printf(TEXT("%d fell that night; %d lived to see the sun come up over the water, and the hearth still burned."), Lost, Survivors));
	}

	// what the settlement became
	FString Became;
	if (bHallFell) { Became = TEXT("So the clearing is a place of cold stones now, and this cut of the saga ends here."); }
	else if (NumAshen >= 3) { Became = TEXT("So it became a hard grey hold of stone and stumps: safe, hungry, and proud of its scars."); }
	else if (NumAshen == 0) { Became = TEXT("So it stayed a green and dangerous place, rich in timber and mushrooms, that buries its dead under living trees."); }
	else { Became = TEXT("So it became a village of two sagas, green on one side and ash on the other, and its children learned both."); }
	const TArray<FString> Names = { bHakon ? TEXT("Hakon") : TEXT(""), bUlf ? TEXT("Ulf") : TEXT(""), bSigrun ? TEXT("Sigrun") : TEXT(""), bTorvald ? TEXT("Torvald") : TEXT("") };
	FString Living;
	int32 NLiving = 0;
	for (const FString& N : Names) { if (!N.IsEmpty()) { Living += (NLiving++ ? TEXT(", ") : TEXT("")) + TSCLoc::Tr(N); } }
	Epilogue.Add(TSCLoc::Tr(Became) + (NLiving > 0 ? TSCLoc::Fmt(TEXT(" Of the four you knew by name, {0} still sit by the fire."), { Living }) : TSCLoc::Tr(TEXT(" Of the four you knew by name, none are left to hear it."))));
	Epilogue.Add(FString::Printf(TEXT("Edits used: %d of %d."), MaxEdits - EditsLeft, MaxEdits));
}

// =====================================================================================================================
// Test helpers
// =====================================================================================================================

void ATSCGameMode::TSCAuto(const FString& Takes, const FString& Build, float Speed)
{
	bAuto = true;
	AutoTakes = Takes.ToUpper();
	AutoBuild = Build.ToUpper();
	SpeedMul = FMath::Clamp(Speed, 1.f, 16.f);
	UE_LOG(LogSaga, Display, TEXT("[Saga] Autoplay takes=%s build=%s speed=%.0f"), *AutoTakes, *AutoBuild, SpeedMul);
}

void ATSCGameMode::TSCLang(const FString& Lang)
{
	TSCLoc::SetRussian(Lang.Equals(TEXT("ru"), ESearchCase::IgnoreCase));
}

void ATSCGameMode::TSCLocMisses()
{
	TSCLoc::DumpMisses();
}

void ATSCGameMode::TSCCut(int32 Take, int32 bConfirm)
{
	PressCut();
	SelectTake(Take);
	if (bConfirm) { ConfirmCut(); }
}

void ATSCGameMode::TSCSkip()
{
	if (Phase == ETSCPhase::Day && ActiveScene < 0) { DayTime += 20.f; }
}

void ATSCGameMode::AutoPlay(float Dt)
{
	if (Phase == ETSCPhase::Title) { StartGame(); return; }
	if (Phase == ETSCPhase::Epilogue) { bAuto = false; SpeedMul = 1.f; return; }
	if (CanCut() && Scenes[ActiveScene].Timer > 2.f && AutoTakes.IsValidIndex(ActiveScene) && AutoTakes[ActiveScene] == TEXT('A'))
	{
		PressCut();
		SelectTake(1);
		ConfirmCut();
	}
	if (AutoBuild.Len() > 0 && Phase == ETSCPhase::Day)
	{
		const TCHAR C = AutoBuild[0];
		if (C != TEXT('H') && C != TEXT('M') && C != TEXT('E') && C != TEXT('S') && C != TEXT('T')) { AutoBuild.RightChopInline(1); return; }
		const EBuilding T = C == TEXT('H') ? EBuilding::House : C == TEXT('M') ? EBuilding::Sawmill : C == TEXT('E') ? EBuilding::Healer : C == TEXT('S') ? EBuilding::HammerShrine : EBuilding::StagShrine;
		if (CanBuild(T)) { StartBuild(T); AutoBuild.RightChopInline(1); }
	}
}
