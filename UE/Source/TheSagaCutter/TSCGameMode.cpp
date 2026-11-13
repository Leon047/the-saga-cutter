// The Saga Cutter - game jam project.

#include "TSCGameMode.h"

#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "TSCColonyCameraPawn.h"
#include "UObject/ConstructorHelpers.h"
#include "TSCHUD.h"
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

	bool InRect(const FIntRect& R, int32 X, int32 Y) { return X >= R.Min.X && X < R.Max.X && Y >= R.Min.Y && Y < R.Max.Y; }
	uint32 Hash2(int32 X, int32 Y) { return HashCombine(GetTypeHash(X * 73856093), GetTypeHash(Y * 19349663)); }
	float Rand01(int32 X, int32 Y, int32 Salt = 0) { return (Hash2(X + Salt * 31, Y - Salt * 17) % 10000) / 10000.f; }
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

int32 ATSCGameMode::RegionOfCell(int32 Cx, int32 Cy) const
{
	if (InRect(ForestRect, Cx, Cy)) { return TSCRegion::Forest; }
	if (InRect(GardenRect, Cx, Cy)) { return TSCRegion::Garden; }
	if (InRect(GroveRect, Cx, Cy)) { return TSCRegion::Grove; }
	if (InRect(DockRect, Cx, Cy)) { return TSCRegion::Dock; }
	return TSCRegion::None;
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
	const float X = WallX() + CELL * 0.5f;
	const bool bAWest = A->GetActorLocation().X < X;
	const bool bBWest = B->GetActorLocation().X < X;
	if (bAWest == bBWest) { return true; }
	return (!bGateClosed || bGateBroken) && !IsBlocked(TreeX, TreeY + 1);
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
	Fill = MakeLight(FRotator(-30.f, 150.f, 0.f), false);
	TopFill = MakeLight(FRotator(-88.f, 0.f, 0.f), false);
	Fill->SetAtmosphereSunLight(false);
	Fill->SetForwardShadingPriority(2);
	TopFill->SetAtmosphereSunLight(false);
	TopFill->SetForwardShadingPriority(1);

	Fog = NewObject<UExponentialHeightFogComponent>(AtmoActor);
	Fog->SetupAttachment(AtmoRoot);
	Fog->RegisterComponent();
	Fog->SetFogHeightFalloff(0.55f);
	Fog->SetStartDistance(2000.f);

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

	// ----- ground: big base tiles, then one tile per cell in the editable regions -----
	{
		ATSCPrimActor* Base = SpawnPrim(FVector(0.f, 0.f, -8.f));
		Base->AddPart(ETSCShape::Cube, FVector::ZeroVector, FRotator::ZeroRotator, FVector(900.f, 900.f, 0.02f), Pal::ForestFloor * 0.8f);
		// settlement green
		Base->AddPart(ETSCShape::Cube, FVector(CellF(17.f, 12.f).X, 0.f, 3.f), FRotator::ZeroRotator, FVector(18.f * 2.f, 34.f * 2.f, 0.02f), Pal::Grass);
		// rocky shore
		Base->AddPart(ETSCShape::Cube, FVector(CellF(27.f, 12.f).X, 0.f, 4.f), FRotator::ZeroRotator, FVector(4.f, 34.f * 2.f, 0.03f), Pal::Shore);
		WaterActor = SpawnPrim(FVector(CellF(28.f, 12.f).X + 20000.f, 0.f, -2.f));
		WaterActor->AddPart(ETSCShape::Cube, FVector::ZeroVector, FRotator::ZeroRotator, FVector(400.f, 900.f, 0.04f), Pal::Water);

		// trodden paths: gate -> hall -> dock, and to the stones
		ATSCPrimActor* Paths = SpawnPrim(FVector(0.f, 0.f, 0.f));
		auto PathCell = [&](int32 X, int32 Y)
		{
			Paths->AddPart(ETSCShape::Cube, CellCenter(X, Y) + FVector(0.f, 0.f, 1.f), FRotator::ZeroRotator, FVector(1.98f, 1.98f, 0.03f),
				Pal::Dirt * (0.85f + 0.3f * Rand01(X, Y, 3)));
		};
		for (int32 X = 9; X <= 25; ++X) { if (X < 14 || X > 19) { PathCell(X, 11); PathCell(X, 12); } }
		for (int32 X = 12; X <= 21; ++X) { PathCell(X, 13); }
		for (int32 Y = 3; Y <= 9; ++Y) { PathCell(13, Y); }
		PathCell(14, 3); PathCell(15, 3); PathCell(16, 3);
		for (int32 Y = 14; Y <= 19; ++Y) { PathCell(14, Y); }
		PathCell(20, 9); PathCell(20, 10); PathCell(21, 13); PathCell(21, 14);
	}
	for (int32 X = -6; X < GW + 2; ++X)
	{
		for (int32 Y = -4; Y < GH + 4; ++Y)
		{
			const int32 Region = RegionOfCell(X, Y);
			if (Region == TSCRegion::None) { continue; }
			if (Region == TSCRegion::Dock && X >= 28) { continue; } // water
			ATSCPrimActor* G = SpawnPrim(CellCenter(X, Y) + FVector(0.f, 0.f, 4.f), Region);
			const float V = 0.8f + 0.4f * Rand01(X, Y, 1);
			const bool bRoad = Region == TSCRegion::Forest && (Y == 11 || Y == 12);
			FLinearColor Living = Region == TSCRegion::Forest ? (bRoad ? Pal::Dirt * 0.8f : Pal::ForestFloor) : Region == TSCRegion::Dock ? Pal::Shore
				: Region == TSCRegion::Grove ? FLinearColor(0.04f, 0.085f, 0.055f) : Pal::Grass;
			BuildGroundTile(G, Living * V, (Rand01(X, Y, 2) > 0.5f ? Pal::Ash : Pal::Char * 1.6f) * V);
			G->Refresh();
		}
	}

	// ----- forest (west) and the framing trees -----
	for (int32 X = -6; X < GW + 2; ++X)
	{
		for (int32 Y = -4; Y < GH + 4; ++Y)
		{
			const bool bForest = X <= 6;
			const bool bFrame = !bForest && X >= 9 && X <= 27 && (Y <= -1 || Y >= GH + 1);
			const bool bGroveTree = X >= 10 && X <= 20 && Y >= 21 && Y <= GH && !(X >= 13 && X <= 17 && Y <= 22);
			if (!bForest && !bFrame && !bGroveTree) { continue; }
			if (X > 27) { UE_LOG(LogSaga, Warning, TEXT("[Saga] tree loop: X=%d Y=%d forest=%d frame=%d grove=%d"), X, Y, bForest, bFrame, bGroveTree); continue; }
			if (bForest && (Y == 11 || Y == 12)) { continue; } // forest road
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
	// SM_WoodenFence log sections from the north to the south log wall placed in Map_Saga (y = -2500 / +2500),
	// broken by the gate. The per-cell actors keep the Ashen stone footing and the path blocking.
	bool bMeshWall = true;
	for (const FVector2D& Run : { FVector2D(-2500.f, CellCorner(GateX, GateY).Y), FVector2D(CellCorner(GateX, GateY + 2).Y, 2500.f) })
	{
		ATSCPrimActor* P = SpawnPrim(FVector(WallX(), 0.f, 0.f));
		bMeshWall &= BuildPalisadeRun(P, Run.X, Run.Y, FMath::RoundToInt(Run.X));
		P->Refresh();
	}
	for (int32 Y = -1; Y <= GH; ++Y)
	{
		if (Y == GateY || Y == GateY + 1) { continue; }
		ATSCPrimActor* P = SpawnPrim(CellCorner(GateX, Y));
		BuildPalisadeCell(P, Y + 20, !bMeshWall);
		P->Refresh();
		SetBlocked(GateX, Y, true);
	}
	Gate = SpawnPrim(CellCorner(GateX, GateY));
	BuildGate(Gate, GateLeaves);
	Gate->Refresh();

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
	auto Place = [&](int32 X, int32 Y, int32 W, int32 H, int32 Region = 0)
	{
		ATSCPrimActor* A = SpawnPrim(CellCorner(X, Y), Region);
		BlockRect(X, Y, W, H);
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
		for (const FVector2D& L : { FVector2D(13.5f, 4.5f), FVector2D(9.5f, 5.5f) })
		{
			A = SpawnPrim(CellF(L.X, L.Y));
			BuildLogPile(A);
			A->Refresh();
			const FIntPoint C = WorldToCell(CellF(L.X, L.Y));
			SetBlocked(C.X, C.Y, true);
		}
	}
	Garden = SpawnPrim(CellCorner(16, 15), TSCRegion::Garden);
	BuildGarden(Garden, 4, 3, CropParts);
	Garden->Refresh();

	DockActor = SpawnPrim(CellCorner(26, 11), TSCRegion::Dock);
	BuildDock(DockActor, 6, 2, CrateParts);
	DockActor->Refresh();
	{
		ATSCPrimActor* Ship = SpawnPrim(CellF(30.6f, 14.4f) + FVector(0.f, 0.f, -6.f), TSCRegion::Dock);
		BuildLongship(Ship);
		Ship->Refresh();
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
	for (const FVector2D& T : { FVector2D(13.5f, 13.5f), FVector2D(20.5f, 13.5f), FVector2D(25.6f, 10.6f), FVector2D(15.6f, 14.6f), FVector2D(13.4f, 8.6f) })
	{
		ATSCPrimActor* A = SpawnPrim(CellF(T.X, T.Y));
		BuildTorch(A);
		A->Refresh();
	}

	// ----- construction plots -----
	Plots.SetNum(2);
	Plots[0].Cell = FIntPoint(10, 8);
	Plots[1].Cell = FIntPoint(21, 18);
	for (FTSCPlot& P : Plots)
	{
		P.Marker = SpawnPrim(CellCorner(P.Cell.X, P.Cell.Y));
		BuildPlotMarker(P.Marker, 3, 2);
		P.Marker->Refresh();
		P.Marker->SetActorHiddenInGame(true);
	}
	Plots[0].bAvailable = true;
	Plots[0].Marker->SetActorHiddenInGame(false);

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
	BuildAtmosphere();
	BuildWorld();
	float W, F;
	ComputeRates(W, F);
	WoodRate = FMath::RoundToInt(W);
	FoodRate = FMath::RoundToInt(F);
	PlayMusic(MenuMusic);
}

// =====================================================================================================================
// Tick
// =====================================================================================================================

void ATSCGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float RealDt = FMath::Min(DeltaSeconds, 0.1f);
	WorldTime += RealDt;

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

	if (!IsFrozen())
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(SpeedMul));
		const float Dt = RealDt * SpeedMul / Steps;
		for (int32 S = 0; S < Steps && !IsFrozen(); ++S)
		{
			if (Phase == ETSCPhase::Day) { SimDay(Dt); }
			else if (Phase == ETSCPhase::Night) { SimNight(Dt); }
			for (int32 I = 0; I < Units.Num(); ++I) { if (Units[I]) { Units[I]->Sim(Dt); } }
		}
	}
	UpdateAtmosphere(RealDt);
	if (bAuto) { AutoPlay(RealDt); }
}

void ATSCGameMode::UpdateAtmosphere(float Dt)
{
	if (!Sun) { return; }
	int32 NumAshen = 0;
	for (const FTSCScene& S : Scenes) { NumAshen += S.bAshen ? 1 : 0; }
	AshAmount = FMath::FInterpTo(AshAmount, NumAshen / 4.f, Dt, 0.6f);

	const bool bNight = Phase == ETSCPhase::Night || (Phase == ETSCPhase::Epilogue);
	const float F = Phase == ETSCPhase::Day ? FMath::Clamp(DayTime / DayLength, 0.f, 1.f) : (Phase == ETSCPhase::Title ? 0.55f : 1.f);
	// cold blue-grey day sliding into an orange dusk; the night is a deep blue lit by fires
	FLinearColor SunCol = FMath::Lerp(FLinearColor(0.62f, 0.74f, 1.f), FLinearColor(1.f, 0.56f, 0.34f), F * F);
	float SunInt = FMath::Lerp(3.6f, 2.2f, F);
	float FillInt = FMath::Lerp(1.25f, 0.9f, F);
	if (bNight)
	{
		SunCol = FLinearColor(0.35f, 0.48f, 1.f);
		SunInt = 1.0f;
		FillInt = 0.5f;
	}
	static float CurSun = 2.f, CurFill = 0.7f;
	static FLinearColor CurCol(0.6f, 0.7f, 1.f);
	const float K = Dt <= 0.f ? 1.f : FMath::Clamp(Dt * 0.8f, 0.f, 1.f);
	CurSun = FMath::Lerp(CurSun, SunInt, K);
	CurFill = FMath::Lerp(CurFill, FillInt, K);
	CurCol = FMath::Lerp(CurCol, SunCol, K);
	Sun->SetIntensity(CurSun);
	Sun->SetLightColor(CurCol);
	Fill->SetIntensity(CurFill);
	Fill->SetLightColor(FLinearColor(0.45f, 0.6f, 0.9f));
	TopFill->SetIntensity(CurFill * 0.9f);
	TopFill->SetLightColor(FMath::Lerp(FLinearColor(0.5f, 0.62f, 0.8f), FLinearColor(0.7f, 0.62f, 0.56f), AshAmount));

	const float Thick = (Phase == ETSCPhase::Day && Day >= 3) || bNight ? 1.7f : 1.f;
	static float CurThick = 1.f;
	CurThick = FMath::Lerp(CurThick, Thick, K);
	Fog->SetFogDensity(0.012f * CurThick);
	Fog->SetFogInscatteringColor(FMath::Lerp(FLinearColor(0.16f, 0.23f, 0.30f), FLinearColor(0.24f, 0.21f, 0.19f), AshAmount) * (bNight ? 0.45f : 1.f));

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
		ShowBanner(TEXT("Day 1 - Settling in.  Assign your people to wood and food with the panel on the left."), 12.f);
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
	int32& N = Job == ETSCJob::Wood ? NumWood : NumFood;
	const int32 Idle = Workers.Num() - NumWood - NumFood;
	if (Delta > 0 && Idle > 0) { ++N; }
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
	if (Phase != ETSCPhase::Day || FreePlot() < 0 || Wood < BuildCost(Type)) { return false; }
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
	Toast(FString::Printf(TEXT("Your people begin raising %s, tile by tile."), Names[(int32)Type]));
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
		S.LivingName = TEXT("THE LIVING SAGA - The wooden fence");
		S.LivingText = TEXT("The boar tramples the crops: -25 food, and the frightened gardeners gather only half as much today. The soil stays rich.");
		S.AshenName = TEXT("THE ASHEN SAGA - The stone wall");
		S.AshenText = TEXT("Here the garden was walled in stone after the fires. The boar finds no way in. But ash-poor soil yields a quarter less food for the rest of the saga.");
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

void ATSCGameMode::PressCut()
{
	if (!CanCut()) { return; }
	bCutOpen = true;
	SelectedTake = 0;
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
	Toast(FString::Printf(TEXT("%d wood braces the gate (%d strength).%s%s"), FMath::RoundToInt(Spent), FMath::RoundToInt(GateMaxHP),
		bHungryNight ? TEXT(" Your defenders are weak with hunger.") : (bFedNight ? TEXT(" Your defenders are well fed.") : TEXT("")),
		bHammerShrine ? TEXT(" The Hammer blesses their axes.") : TEXT("")));
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
		if (U->bNamed && Phase == ETSCPhase::Night) { Toast(FString::Printf(TEXT("%s has fallen."), *U->DisplayName)); }
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
	if (Once(3, 9.f)) { RaiderWave(bDockAshen ? 2 : 3); Toast(bDockAshen ? TEXT("With no dock to land at, a few raiders wade ashore through the surf.") : TEXT("Raiders leap onto the dock!")); }
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
		FString B = BuiltNames[0];
		if (BuiltNames.Num() > 1) { B += TEXT(" and ") + BuiltNames[1]; }
		FString Why;
		if (bHammerShrine) { Why += TEXT(" The Hammer put weight in every axe."); }
		if (bStagShrine) { Why += TEXT(" The Stag called some of the fog back into the trees."); }
		if (ExtraHealers > 0) { Why += TEXT(" The new hut's fires boiled bandages all night."); }
		if (HousesBuilt > 0) { Why += TEXT(" The newcomers took up shields beside the old families."); }
		if (ExtraSawmills > 0) { Why += TEXT(" Its timber went into the gate."); }
		Epilogue.Add(FString::Printf(TEXT("On the empty ground you raised %s.%s"), *B, *Why));
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
	for (const FString& N : Names) { if (!N.IsEmpty()) { Living += (NLiving++ ? TEXT(", ") : TEXT("")) + N; } }
	Epilogue.Add(Became + (NLiving > 0 ? FString::Printf(TEXT(" Of the four you knew by name, %s still sit by the fire."), *Living) : TEXT(" Of the four you knew by name, none are left to hear it.")));
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
