// The Saga Cutter - game jam project.

#include "TSCTerrain.h"

#include "TheSagaCutter.h"
#include "TSCBuilders.h"
#include "TSCGameMode.h"
#include "TSCOcean.h"
#include "TSCPrimActor.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"

namespace
{
	namespace TT
	{
		// Flat rectangle (exactly flat land): X >= FlatWestX, |Y| <= FlatHalfY.
		constexpr float FlatWestX = -4800.f;      // west edge of the flat rectangle (no east limit)
		constexpr float FlatHalfY = 3400.f;       // half-height of the flat rectangle in Y
		constexpr float BaseZ = -7.f;             // land level inside the flat rectangle (old ground base top)

		// Rolling land: Roll = ss(0, RollRamp, d) * (RollMin + RollAmp * Fbm(p / RollScale)), clamped to <= RollMax.
		constexpr float RollRamp = 2500.f;        // distance outside the flat rectangle over which rolling land fades in
		constexpr float RollMin = 30.f;           // rolling land base height
		constexpr float RollAmp = 420.f;          // rolling land noise amplitude
		constexpr float RollScale = 5200.f;       // rolling land noise scale (world units per noise cell)
		constexpr float RollMax = 650.f;          // rolling land cap (must stay under ~700, camera side)

		constexpr float SeaFloorZ = -60.f;

		// River centreline: Ry(x) = RiverBaseY + A1 sin(x / P1) + A2 sin(x / P2 + Ph2) + WestDrift * max(-x - WestDriftX, 0).
		constexpr float RiverBaseY = 4600.f;      // mean centreline Y
		constexpr float RiverA1 = 250.f;         // first meander amplitude
		constexpr float RiverP1 = 1700.f;         // first meander period / 2Р В РЎСџР В РІР‚С™
		constexpr float RiverA2 = 150.f;          // second meander amplitude
		constexpr float RiverP2 = 610.f;          // second meander period / 2Р В РЎСџР В РІР‚С™
		constexpr float RiverPh2 = 1.3f;          // second meander phase
		constexpr float RiverWestDrift = 0.10f;   // the river bends south far to the west (per unit of X)
		constexpr float RiverWestDriftX = 5000.f; // ... starting west of X = -this
		constexpr float RiverWestX = -45000.f;
		constexpr float RiverFineX = -12000.f;
		constexpr float RiverVLen = 600.f;
		constexpr float RiverProxRange = 3500.f;
	}

	float Hash(int32 X, int32 Y)
	{
		uint32 H = (uint32)X * 374761393u + (uint32)Y * 668265263u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H ^= H >> 16;
		return (float)(H & 0xFFFFFFu) / 16777215.f;
	}

	/** Smooth value noise 0..1: bilinear between lattice hashes with smoothstep weights. */
	float N(const FVector2D& P)
	{
		const int32 X0 = FMath::FloorToInt(P.X);
		const int32 Y0 = FMath::FloorToInt(P.Y);
		const float FX = (float)(P.X - X0);
		const float FY = (float)(P.Y - Y0);
		const float WX = FX * FX * (3.f - 2.f * FX);
		const float WY = FY * FY * (3.f - 2.f * FY);
		const float A = FMath::Lerp(Hash(X0, Y0), Hash(X0 + 1, Y0), WX);
		const float B = FMath::Lerp(Hash(X0, Y0 + 1), Hash(X0 + 1, Y0 + 1), WX);
		return FMath::Lerp(A, B, WY);
	}

	/** 4 octaves of N, 0..1. */
	float Fbm(const FVector2D& P)
	{
		float Sum = 0.f, Amp = 1.f, Norm = 0.f, Freq = 1.f;
		for (int32 I = 0; I < 4; ++I)
		{
			Sum += Amp * N(P * Freq);
			Norm += Amp;
			Amp *= 0.5f;
			Freq *= 2.f;
		}
		return Sum / Norm;
	}

	/** 4 octaves of (1 - |2N - 1|)^2, 0..1. */
	float Ridged(const FVector2D& P)
	{
		float Sum = 0.f, Amp = 1.f, Norm = 0.f, Freq = 1.f;
		for (int32 I = 0; I < 4; ++I)
		{
			Sum += Amp * FMath::Square(1.f - FMath::Abs(2.f * N(P * Freq) - 1.f));
			Norm += Amp;
			Amp *= 0.5f;
			Freq *= 2.f;
		}
		return Sum / Norm;
	}

	float SS(float A, float B, float X)
	{
		return FMath::SmoothStep(A, B, X);
	}

	struct FRiverSection
	{
		float Center, North, South;

		float Cross(float Y) const
		{
			return (Y - Center) / (Y < Center ? North : South);
		}
	};

	FRiverSection RiverSection(float X)
	{
		const float Center = ATSCTerrain::RiverY(X);
		const float Mouth = SS(1250.f, 2200.f, X);
		const float Width = 310.f + 105.f * N(FVector2D(X / 1100.f, 27.f));
		const float Fan = SS(1240.f, 2440.f, X);
		const float Bend = (ATSCTerrain::RiverY(X - 190.f) + ATSCTerrain::RiverY(X + 190.f) - 2.f * Center) / 16.f;
		const float Bias = FMath::Clamp(Bend, -1.f, 1.f) * (1.f - Mouth) * 55.f;
		const float NorthBreak = (N(FVector2D(X / 230.f, 8.f)) - 0.5f) * 142.f;
		const float SouthBreak = (N(FVector2D(X / 310.f, 13.f)) - 0.5f) * 174.f;
		return {Center, Width - Bias + NorthBreak * (1.f - Mouth) + 245.f * Fan,
			Width + Bias + SouthBreak * (1.f - Mouth) + 335.f * Fan};
	}

	float DeltaBar(const FVector2D& P, const FRiverSection& Channel)
	{
		// An offset gravel bar divides the outlet into two channels before the tide covers its tip.
		const double Along = (P.X - 1830.0) / 440.0;
		const double Across = (Channel.Cross(P.Y) - 0.12f - 0.10f * Along) / 0.37f;
		return 1.f - SS(0.13f, 1.f, Along * Along + Across * Across);
	}

	float WesternTrailY(float X)
	{
		return (1.f - SS(-5800.f, -4600.f, X)) * (85.f * FMath::Sin(X / 850.f) + 37.f * FMath::Sin(X / 360.f));
	}

	float MossCover(const FVector2D& P)
	{
		return SS(0.18f, 0.57f, Fbm(P / 520.f)) * (0.57f + 0.43f * N(P / 190.f));
	}

	FLinearColor ForestGroundColor(const FVector2D& P)
	{
		const float Soil = Fbm(P / 630.f);
		const FLinearColor Earth = FMath::Lerp(FLinearColor(0.034f, 0.024f, 0.016f),
			FLinearColor(0.079f, 0.058f, 0.037f), Soil);
		const FLinearColor Moss = FMath::Lerp(FLinearColor(0.058f, 0.089f, 0.022f),
			FLinearColor(0.144f, 0.205f, 0.041f), N(P / 325.f));
		return FMath::Lerp(Earth, Moss, MossCover(P));
	}

	FLinearColor RockColor() { return TSC::Pal::Stone * 0.72f; }

	UMaterialInterface* LoadFlat()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/M_SagaFlat.M_SagaFlat"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	USoundBase* LoadAmbience(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Game/Vikings/Audio/Ambience/%s.%s"), Name, Name);
		return LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	template <typename T>
	T* PickRandom(const TArray<TObjectPtr<T>>& Arr)
	{
		return Arr.Num() ? Arr[FMath::RandRange(0, Arr.Num() - 1)].Get() : nullptr;
	}

	/** Spawns a 2D loop that fades in; null when the sound is missing. */
	UAudioComponent* StartLoop(UObject* Owner, USoundBase* Loop)
	{
		if (!Loop)
		{
			return nullptr;
		}
		// The mix takes these loops down to zero (river far away, birds at night): keep them alive while silent.
		if (USoundWave* Wave = Cast<USoundWave>(Loop)) { Wave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent; }
		UAudioComponent* C = UGameplayStatics::SpawnSound2D(Owner, Loop, 0.01f, 1.f, 0.f, nullptr, false, false);
		if (C)
		{
			C->bIsUISound = true;
			C->FadeIn(2.f, 1.f);  // the fade level multiplies VolumeMultiplier for the whole life of the sound, so it must end at 1 (as the ocean surf)
		}
		return C;
	}

	TAutoConsoleVariable<int32> CVarTerrainMixLog(TEXT("tsc.TerrainMixLog"), 0, TEXT("1 = log the river / wind / birds volumes once per second (terrain audio mix check)."));

	// Scatter tunables.
	constexpr int32 MaxPines = 1400;

}

ATSCTerrain::ATSCTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Land = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Land"));
	Land->SetupAttachment(RootComponent);
	Trails = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Trails"));
	Trails->SetupAttachment(RootComponent);
	River = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("River"));
	River->SetupAttachment(RootComponent);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		ULocalFogVolumeComponent* Mist = CreateDefaultSubobject<ULocalFogVolumeComponent>(*FString::Printf(TEXT("ValleyMist%d"), Index));
		Mist->SetupAttachment(RootComponent);
		const float X = -2250.f - 3100.f * Index;
		Mist->SetRelativeLocation(FVector(X, RiverY(X), RiverZ(X) - 310.f));
		Mist->SetRelativeScale3D(FVector(18.5f));
		Mist->SetRadialFogExtinction(0.012f);
		Mist->SetHeightFogExtinction(0.17f);
		Mist->SetHeightFogFalloff(1120.f);
		Mist->SetHeightFogOffset(-0.08f);
		Mist->SetFogAlbedo(FLinearColor(0.65f, 0.73f, 0.70f));
		Mist->SetFogPhaseG(0.22f);
	}
}

float ATSCTerrain::RiverY(float X)
{
	return TT::RiverBaseY + TT::RiverA1 * FMath::Sin(X / TT::RiverP1) + TT::RiverA2 * FMath::Sin(X / TT::RiverP2 + TT::RiverPh2)
		+ TT::RiverWestDrift * FMath::Max(-X - TT::RiverWestDriftX, 0.f);
}

float ATSCTerrain::RiverZ(float X)
{
	return 9.f + 0.004f * FMath::Max(1000.f - X, 0.f);
}

float ATSCTerrain::RiverMouthCut(float X, float Y)
{
	const FRiverSection Channel = RiverSection(X);
	return SS(1310.f, 1710.f, X) * (1.f - SS(0.81f, 1.38f, FMath::Abs(Channel.Cross(Y)))) * 34.f;
}

float ATSCTerrain::RiverProximity(const FVector2D& P) const
{
	return FMath::Clamp(FMath::Abs((float)P.Y - RiverY((float)P.X)) / TT::RiverProxRange, 0.f, 1.f);
}

float ATSCTerrain::HeightAt(const FVector2D& P) const
{
	const float X = (float)P.X, Y = (float)P.Y;

	// Distance outside the flat rectangle.
	const float DX = FMath::Max(TT::FlatWestX - X, 0.f);
	const float DY = FMath::Max(FMath::Abs(Y) - TT::FlatHalfY, 0.f);
	const float D = FMath::Sqrt(DX * DX + DY * DY);

	// Rolling land.
	const float Roll = FMath::Min(SS(0.f, TT::RollRamp, D) * (TT::RollMin + TT::RollAmp * Fbm(P / TT::RollScale)), TT::RollMax);

	// A sinuous ridge with separated summits and runoff gullies on the south face.
	const float CrestY = -7800.f + 600.f * FMath::Sin(X / 3200.f) + 260.f * FMath::Sin(X / 1150.f);
	const float North = -Y - 3450.f;
	const float Headland = SS(2850.f, 5200.f, North);
	const float Ridge = 1.f - SS(180.f, 5200.f, FMath::Abs(Y - CrestY));
	const float Summits = 0.70f + 0.20f * FMath::Cos((X + 1700.f) / 1100.f) + 0.10f * FMath::Cos(X / 500.f);
	const float Gully = Ridged(FVector2D((X + (Y - CrestY) * 0.19f) / 940.f, Y / 2650.f));
	const float Erosion = 1.f - 0.43f * Gully;
	const float Coast = FMath::Lerp(1.f - SS(-1000.f, 1780.f, X), 1.f, Headland);
	const float Mount = 4900.f * SS(0.f, 3700.f, North) * Ridge * Summits * Erosion * Coast * SS(0.f, 1500.f, D);
	const float Sea = SS(1800.f, 2100.f, X) * (1.f - Headland);
	const float MeadowSkirt = 3.f * SS(0.f, 140.f, DY) * (1.f - SS(1600.f, 1860.f, FMath::Abs(X)));
	const float LandZ = FMath::Lerp(TT::BaseZ + Roll + Mount + MeadowSkirt + GroundRelief(P), TT::SeaFloorZ, Sea);

	const FRiverSection Channel = RiverSection(X);
	const float Cross = Channel.Cross(Y);
	const float R = FMath::Abs(Cross);
	const bool bInside = Cross * (Channel.South - Channel.North) > 0.f;
	const float BankNoise = N(P / 280.f);
	const float Bank = 1.f - SS(1.02f, (bInside ? 2.13f : 1.65f) + 0.29f * BankNoise, R);
	const float Bed = FMath::Lerp(-68.f, 24.f, SS(0.26f, 1.09f, R))
		+ 17.f * SS(0.35f, 0.62f, R) * (1.f - SS(0.76f, 1.03f, R));
	const float Gravel = (bInside ? 37.f : 25.f) * SS(0.39f, 0.80f, R) * (1.f - SS(0.87f, 1.43f, R))
		* (0.66f * BankNoise + 0.34f * N(P / 132.f));
	const float Cut = Bank * SS(0.f, 300.f, D) * (1.f - SS(2080.f, 2520.f, X));
	const float Fan = SS(1160.f, 2130.f, X);
	const float ChannelZ = RiverZ(X) + Bed + Gravel - 15.f * Fan + 81.f * DeltaBar(P, Channel);
	return FMath::Lerp(LandZ, ChannelZ, Cut);
}

FLinearColor ATSCTerrain::GroundColorAt(const FVector2D& P, float Z, const FVector& Normal, const ATSCOcean* Ocean) const
{
	const float X = P.X;
	const float Y = P.Y;
	const float Variation = Fbm(P / 1400.f);
	const float Rock = FMath::Max(1.f - SS(0.61f, 0.85f, Normal.Z), SS(950.f, 1950.f, Z));
	const float SnowLine = 2220.f + 570.f * N(P / 1700.f);
	const float Snow = SS(SnowLine - 240.f, SnowLine + 250.f, Z) * SS(0.35f, 0.70f, Normal.Z);
	const float GravelBar = N(P / 240.f);
	const FRiverSection Channel = RiverSection(X);
	const float Across = FMath::Abs(Channel.Cross(Y));
	const float Bank = (1.f - SS(0.69f + 0.18f * GravelBar, 1.44f + 0.26f * GravelBar, Across))
		* FMath::Max(1.f - SS(19.f, 87.f, Z - RiverZ(X)), DeltaBar(P, Channel));
	const float Forest = (1.f - SS(720.f, 1550.f, Z));
	FLinearColor Color = FMath::Lerp(TSC::Pal::ForestFloor * 0.8f, ForestGroundColor(P), Forest);
	Color = FMath::Lerp(Color, FMath::Lerp(TSC::Pal::StoneDark * 0.75f, TSC::Pal::Stone * 0.52f, Variation), Rock);
	Color = FMath::Lerp(Color, FLinearColor(0.49f, 0.54f, 0.60f), Snow);
	Color = FMath::Lerp(Color, TSC::Pal::Shore * (0.58f + 0.2f * Variation), Bank);
	const float ShoreDistance = Ocean ? Ocean->ShoreXAt(Y) - X : 2200.f - X;
	const float Breakup = (Fbm(P / 430.f) - 0.5f) * 360.f
		+ (N(P / 150.f) - 0.5f) * 140.f;
	const float Gravel = 1.f - SS(95.f, 1090.f, ShoreDistance + Breakup);
	const float Wet = 1.f - SS(-40.f, 85.f, ShoreDistance);
	const FLinearColor Beach = TSC::Pal::Shore * FMath::Lerp(1.f, 0.6f, Wet);
	Color = FMath::Lerp(Color, Beach, Gravel);
	Color.A = Forest * (1.f - Rock) * (1.f - Bank) * (1.f - Snow) * (1.f - Gravel);
	return Color;
}

void ATSCTerrain::BuildLand(const ATSCOcean* Ocean)
{
	TArray<float> Xs = {-45000.f, -30000.f, -22000.f, -16500.f};
	TArray<float> Ys = Xs;
	for (float X = -15000.f; X <= 6600.f; X += (X >= -6600.f && X < 2640.f ? 60.f : 120.f)) { Xs.Add(X); }
	Xs.Append({7500.f, 9000.f, 12000.f, 16500.f, 21000.f, 29000.f, 37000.f, 45000.f});
	for (float Y = -15000.f; Y <= 3000.f; Y += (Y >= -3300.f ? 60.f : 120.f)) { Ys.Add(Y); }
	for (float Y = 3040.f; Y <= 7200.f; Y += 40.f) { Ys.Add(Y); }
	Ys.Append({8000.f, 9200.f, 11000.f, 14000.f, 18000.f, 24000.f, 34000.f, 45000.f});

	TArray<FVector> Verts, Normals;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Tris;
	const int32 NY = Ys.Num();
	// Shape the beach into the terrain so the two surfaces share normals and vertex colors.
	const auto SurfaceHeight = [this, Ocean](const FVector2D& P)
	{
		const float LandZ = HeightAt(P);
		if (!Ocean || !LandMID) { return LandZ; }
		const float FromWater = P.X - Ocean->ShoreXAt(P.Y);
		const float Apron = SS(-1250.f, -830.f, FromWater) * (1.f - SS(100.f, 310.f, FromWater));
		if (Apron <= 0.f) { return LandZ; }
		const float Lift = Ocean->BeachHeightAt(P) - LandZ;
		return LandZ + Apron * FMath::Max(0.f, Lift) * SS(-4.f, 6.f, Lift);
	};
	for (float X : Xs)
	{
		for (float Y : Ys)
		{
			const FVector2D P(X, Y);
			const float Z = SurfaceHeight(P);
			const float DX = SurfaceHeight(P + FVector2D(45.f, 0.f)) - SurfaceHeight(P - FVector2D(45.f, 0.f));
			const float DY = SurfaceHeight(P + FVector2D(0.f, 45.f)) - SurfaceHeight(P - FVector2D(0.f, 45.f));
			const FVector Normal = FVector(-DX, -DY, 90.f).GetSafeNormal();
			Verts.Add(FVector(P, Z));
			Normals.Add(Normal);
			UV.Add(P / 1000.f);
			Colors.Add(GroundColorAt(P, Z, Normal, Ocean));
			Tangents.Add(FProcMeshTangent(FVector(90.f, 0.f, DX).GetSafeNormal(), false));
		}
	}
	for (int32 Ix = 0; Ix < Xs.Num() - 1; ++Ix)
	{
		for (int32 Iy = 0; Iy < NY - 1; ++Iy)
		{
			const int32 A = Ix * NY + Iy, B = A + 1, C = A + NY, D = C + 1;
			Tris.Append({A, B, C, C, B, D});
		}
	}
	Land->ClearAllMeshSections();
	Land->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UV, Colors, Tangents, false, false);
}

void ATSCTerrain::Build(ATSCOcean* Ocean, const TArray<TArray<FVector2D>>& TrailRoutes)
{
	SprayRenderer = Ocean;
	RiverImpacts.Reset();
	SprayRandom.Reset();
	Mounds.Reset();
	const float ShoreX = Ocean ? Ocean->ShoreXAt(RiverY(2200.f)) : 2200.f;
	UMaterialInterface* Flat = LoadFlat();
	UMaterialInterface* Ground = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/M_SagaEmeraldGround.M_SagaEmeraldGround"));
	LandMID = Ground ? UMaterialInstanceDynamic::Create(Ground, this) : nullptr;
	if (LandMID)
	{
		Land->SetMaterial(0, LandMID);
		if (Ocean) { Ocean->HideBeachApron(); }
	}
	else if (Flat)
	{
		UMaterialInstanceDynamic* Fallback = UMaterialInstanceDynamic::Create(Flat, this);
		Fallback->SetVectorParameterValue(TEXT("Color"), TSC::Pal::ForestFloor);
		Land->SetMaterial(0, Fallback);
	}
	BuildLand(Ocean);
	BuildTrails(TrailRoutes, Ocean);
	Land->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Land->SetCastShadow(false);
	Land->bAffectDistanceFieldLighting = false;
	LastAsh = -1.f;

	// ----- Scatter: pines and boulders -----
	for (UInstancedStaticMeshComponent* Old : Pines)
	{
		if (Old) { Old->DestroyComponent(); }
	}
	Pines.Reset();
	for (UInstancedStaticMeshComponent* RocksComponent : Boulders)
	{
		RocksComponent->DestroyComponent();
	}
	Boulders.Reset();

	auto MakeISM = [this](const TCHAR* Name, UStaticMesh* Mesh) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, MakeUniqueObjectName(this, UInstancedStaticMeshComponent::StaticClass(), Name));
		C->SetMobility(EComponentMobility::Movable);
		C->SetupAttachment(RootComponent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(true);
		C->RegisterComponent();
		C->SetStaticMesh(Mesh);
		return C;
	};

	{
		TArray<UStaticMesh*> PineMeshes;
		for (const TCHAR* Path : { TEXT("/Game/Trees/Pine/SM_PineTree.SM_PineTree"), TEXT("/Game/Trees/Pine2/SM_PineTree2.SM_PineTree2"), TEXT("/Game/Trees/Pine3/SM_PineTree3.SM_PineTree3") })
		{
			if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				PineMeshes.Add(M);
			}
		}
		if (PineMeshes.Num() > 0)
		{
			TArray<TArray<FTransform>> PineXf;
			PineXf.SetNum(PineMeshes.Num());
			FRandomStream Rng(9241);
			int32 Count = 0;
			for (float Gx = -15000.f; Gx < 2000.f && Count < MaxPines; Gx += 260.f)
			{
				for (float Gy = -10500.f; Gy < 8000.f && Count < MaxPines; Gy += 260.f)
				{
					const FVector2D P(Gx + Rng.FRandRange(-130.f, 130.f), Gy + Rng.FRandRange(-130.f, 130.f));
					const float Roll = Rng.FRand();
					const int32 MeshIdx = Rng.RandRange(0, PineMeshes.Num() - 1);
					const float Yaw = Rng.FRandRange(0.f, 360.f);
					const float Sc = 0.4f * Rng.FRandRange(0.85f, 1.3f);
					const float ZStretch = Rng.FRandRange(0.65f, 1.6f);

					if (P.X >= -4700.f && FMath::Abs(P.Y) <= 3300.f) { continue; }                       // runtime forest area
					if (FMath::Abs((float)P.Y - RiverY((float)P.X)) < 520.f) { continue; }                // river
					if (P.X < -4700.f && FMath::Abs(P.Y - WesternTrailY(P.X)) < 235.f) { continue; }                           // forest road
					const float Hz = HeightAt(P);
					if (Hz < 0.f || Hz > 1400.f) { continue; }                                            // below land / above the tree line (S7: 2300, above the new snow line)
					if (FMath::Abs(HeightAt(P + FVector2D(150.f, 0.f)) - Hz) > 110.f || FMath::Abs(HeightAt(P + FVector2D(0.f, 150.f)) - Hz) > 110.f) { continue; }   // too steep
					const float Grove = N(P / 1450.f + FVector2D(5.f, 5.f));
					if (Roll > 0.85f * SS(0.30f, 0.68f, Grove)) { continue; }

					PineXf[MeshIdx].Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P, Hz - 5.f), FVector(Sc, Sc, Sc * ZStretch)));
					++Count;
				}
			}
			for (int32 I = 0; I < PineMeshes.Num(); ++I)
			{
				UInstancedStaticMeshComponent* C = MakeISM(*FString::Printf(TEXT("Pines%d"), I), PineMeshes[I]);
				if (PineXf[I].Num() > 0)
				{
					C->AddInstances(PineXf[I], false, /*bWorldSpace*/ true);
				}
				Pines.Add(C);
			}
			UE_LOG(LogSaga, Display, TEXT("[Saga] Terrain pines: %d instances (%d meshes)"), Count, PineMeshes.Num());
		}
	}

	FRandomStream Rocks(733);
	RockMID = Flat ? UMaterialInstanceDynamic::Create(Flat, this) : nullptr;
	if (RockMID) { RockMID->SetVectorParameterValue(TEXT("Color"), RockColor()); }

	auto MakeRockScatter = [&](const TCHAR* Name, UStaticMesh* Mesh, bool bUseScanMaterial = false)
	{
		UInstancedStaticMeshComponent* Component = MakeISM(Name, Mesh);
		Boulders.Add(Component);
		if (RockMID && !bUseScanMaterial)
		{
			for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				Component->SetMaterial(Slot, RockMID);
			}
		}
		return Component;
	};

	UStaticMesh* LedgeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Fab/Nordic_Forest_Ledge_Rock_Medium/xislecsqx_tier_2/StaticMeshes/xislecsqx_tier_2.xislecsqx_tier_2"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* ClusterMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Fab/Nordic_Forest_Cluster_Rock_Small/xjijdiz_tier_2/StaticMeshes/xjijdiz_tier_2.xjijdiz_tier_2"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UInstancedStaticMeshComponent* Ledges = LedgeMesh ? MakeRockScatter(TEXT("NordicMountainLedges"), LedgeMesh) : nullptr;
	UInstancedStaticMeshComponent* Clusters = ClusterMesh ? MakeRockScatter(TEXT("NordicRiverClusters"), ClusterMesh, true) : nullptr;
	UStaticMesh* Boulder = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vikings/Buildings/VikingHouse/Meshes/SM_VH_FND_Boulder_A.SM_VH_FND_Boulder_A"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UInstancedStaticMeshComponent* Rapids = LedgeMesh ? MakeRockScatter(TEXT("RiverBedrock"), LedgeMesh, true)
		: (Boulder ? MakeRockScatter(TEXT("RiverBedrock"), Boulder) : nullptr);
	for (const TCHAR* Variant : {TEXT("A"), TEXT("B"), TEXT("C")})
	{
		UInstancedStaticMeshComponent* Fallback = nullptr;
		if (!Ledges || !Clusters)
		{
			const FString Path = FString::Printf(TEXT("/Game/Vikings/Buildings/VikingHouse/Meshes/SM_VH_FND_Boulder_%s.SM_VH_FND_Boulder_%s"), Variant, Variant);
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Fallback = MakeRockScatter(*FString::Printf(TEXT("Boulders%s"), Variant), Mesh);
			}
		}
		for (int32 I = 0; I < 110; ++I)
		{
			const bool bRiver = I < 48;
			UInstancedStaticMeshComponent* Component = bRiver ? Clusters : Ledges;
			if (!Component) { Component = Fallback; }
			if (!Component) { continue; }
			const float X = Rocks.FRandRange(-9850.f, 1360.f);
			const FRiverSection Channel = RiverSection(X);
			const float Side = Rocks.FRand() < 0.5f ? -Channel.North : Channel.South;
			const float Y = bRiver ? Channel.Center + Side * Rocks.FRandRange(0.96f, 1.36f) : Rocks.FRandRange(-8600.f, -3860.f);
			const FVector2D P(X, Y);
			const float Z = HeightAt(P);
			if (Z < -12.f || (!bRiver && (Z < 100.f || Z > 2500.f || N(P / 1000.f) < 0.53f))) { continue; }
			const float Radius = Rocks.FRandRange(bRiver ? 14.f : 55.f, bRiver ? 57.f : 210.f);
			const FBoxSphereBounds Bounds = Component->GetStaticMesh()->GetBounds();
			const float Scale = Radius / FMath::Max(1.0, FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y));
			const float XDelta = HeightAt(P + FVector2D(55.f, 0.f)) - HeightAt(P - FVector2D(55.f, 0.f));
			const float YDelta = HeightAt(P + FVector2D(0.f, 55.f)) - HeightAt(P - FVector2D(0.f, 55.f));
			const FVector Normal = FVector(-XDelta, -YDelta, 110.f).GetSafeNormal();
			const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Normal) * FQuat(FVector::UpVector, Rocks.FRandRange(0.f, 2.f * PI));
			const FVector Origin = FVector(P, Z) + Normal * Bounds.BoxExtent.Z * Scale * 0.58f - Rotation.RotateVector(Bounds.Origin * Scale);
			Component->AddInstance(FTransform(Rotation, Origin, FVector(Scale)), true);
		}
	}
	const float RiverDetailEnd = FMath::Min(1440.f, ShoreX - 500.f);
	TArray<TPair<FVector2D, float>> RiverObstacles;
	TArray<float> RapidCenters;
	if (Rapids)
	{
		if (UMaterialInstanceDynamic* Material = Rapids->CreateDynamicMaterialInstance(0))
		{
			Material->SetScalarParameterValue(TEXT("MossAmount"), 0.48f);
		}
		const FBoxSphereBounds Bounds = Rapids->GetStaticMesh()->GetBounds();
		for (float Reach = -10400.f; Reach < RiverDetailEnd - 200.f; Reach += Rocks.FRandRange(460.f, 1080.f))
		{
			RapidCenters.Add(Reach);
			const int32 Count = Rocks.RandRange(3, 7);
			for (int32 I = 0; I < Count; ++I)
			{
				const float X = Reach + Rocks.FRandRange(-200.f, 200.f);
				const FRiverSection Channel = RiverSection(X);
				const float Across = Rocks.FRandRange(-0.85f, 0.85f);
				const float Y = Channel.Center + Across * (Across < 0.f ? Channel.North : Channel.South);
				const float Radius = I == 0 ? Rocks.FRandRange(76.f, 141.f) : Rocks.FRandRange(22.f, 67.f);
				const float Scale = Radius / FMath::Max(1.0, FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y));
				const FRotator Rotation(Rocks.FRandRange(-10.f, 10.f), Rocks.FRandRange(0.f, 360.f), Rocks.FRandRange(-12.f, 12.f));
				const float Z = RiverZ(X) - FMath::Min(20.f, (float)Bounds.BoxExtent.Z * Scale * 0.34f);
				RiverObstacles.Emplace(FVector2D(X, Y), Radius);
				if (I < 2 && Radius >= 44.f)
				{
					const FVector2D Downstream = FVector2D(1.f, (RiverY(X + 12.f) - RiverY(X - 12.f)) / 24.f).GetSafeNormal();
					const FVector2D Impact = FVector2D(X, Y) - Downstream * Radius * 0.78f;
					RiverImpacts.Add({FVector(Impact, RiverZ(Impact.X) + 7.f), Downstream, static_cast<float>(SprayRandom.FRandRange(0.2f, 1.1f))});
				}
				Rapids->AddInstance(FTransform(Rotation, FVector(X, Y, Z) - Rotation.RotateVector(Bounds.Origin * Scale), FVector(Scale)), true);
			}
		}
	}

	GroundPlantMIDs.Reset();
	auto ScatterUnderstory = [&](const TCHAR* Name, const TCHAR* Path, bool bFern)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mesh) { return; }
		UInstancedStaticMeshComponent* Plants = MakeISM(Name, Mesh);
		Boulders.Add(Plants);
		Plants->SetCastShadow(false);
		if (UMaterialInstanceDynamic* Material = Plants->CreateDynamicMaterialInstance(0))
		{
			GroundPlantMIDs.Add(Material);
		}
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const FVector Foot(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z);
		const float Step = bFern ? 161.f : 91.f;
		TArray<FTransform> Instances;
		for (float X = -11200.f; X < 1600.f; X += Step)
		{
			for (float Y = -7750.f; Y < 7730.f; Y += Step)
			{
				const FVector2D P(X + Rocks.FRandRange(-0.48f, 0.48f) * Step, Y + Rocks.FRandRange(-0.48f, 0.48f) * Step);
				if (P.X >= TT::FlatWestX - 120.f && FMath::Abs(P.Y) <= TT::FlatHalfY + 100.f) { continue; }
				if (P.X < TT::FlatWestX && FMath::Abs(P.Y - WesternTrailY(P.X)) < 245.f) { continue; }
				const float Bank = FMath::Abs(RiverSection(P.X).Cross(P.Y));
				const float PlantEdge = 1.04f + 0.28f * N(P / 250.f);
				if (Bank < PlantEdge || Rocks.FRand() > MossCover(P) * (bFern ? 0.77f : 1.f)) { continue; }
				const float Z = HeightAt(P);
				if (Z < 0.f || Z > 1350.f) { continue; }
				if (FMath::Abs(HeightAt(P + FVector2D(60, 0)) - Z) > 33.f
					|| FMath::Abs(HeightAt(P + FVector2D(0, 60)) - Z) > 33.f) { continue; }
				const float Scale = Rocks.FRandRange(bFern ? 0.29f : 0.37f, bFern ? 0.65f : 0.84f);
				const FRotator Rotation(0.f, Rocks.FRandRange(0.f, 360.f), 0.f);
				Instances.Emplace(Rotation, FVector(P, Z - 5.f) - Rotation.RotateVector(Foot * Scale), FVector(Scale));
			}
		}
		Plants->AddInstances(Instances, false, true);
		UE_LOG(LogSaga, Display, TEXT("[Saga] %s: %d instances"), Name, Instances.Num());
	};
	ScatterUnderstory(TEXT("ForestGrass"), TEXT("/Game/Vikings/Environment/ShorePlants/SM_Grass05.SM_Grass05"), false);
	ScatterUnderstory(TEXT("ForestFerns"), TEXT("/Game/Vikings/Environment/ShorePlants/SM_Fern02.SM_Fern02"), true);

	if (Clusters)
	{
		const FBoxSphereBounds Bounds = ClusterMesh->GetBounds();
		const double Radius = FMath::Max(1.0, FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y));
		UInstancedStaticMeshComponent* Gravel = MakeRockScatter(TEXT("RiverGravelBars"), ClusterMesh, true);
		Gravel->SetCastShadow(false);
		if (UMaterialInstanceDynamic* Material = Gravel->CreateDynamicMaterialInstance(0))
		{
			Material->SetScalarParameterValue(TEXT("MossAmount"), 0.17f);
		}
		FRandomStream Pebbles(87411);
		TArray<FTransform> Bars;
		for (const float Side : {-1.f, 1.f})
		{
			for (float Reach = -11200.f; Reach < RiverDetailEnd - 130.f; Reach += Pebbles.FRandRange(120.f, 410.f))
			{
				const int32 Count = Pebbles.RandRange(13, 29);
				for (int32 I = 0; I < Count; ++I)
				{
					const float X = Reach + Pebbles.FRandRange(-125.f, 125.f);
					const FRiverSection Channel = RiverSection(X);
					const float Across = Pebbles.FRandRange(0.72f, 1.53f);
					const FVector2D P(X, Channel.Center + Side * Across * (Side < 0.f ? Channel.North : Channel.South));
					const float Z = HeightAt(P);
					if (Z < RiverZ(X) - 34.f || Z > RiverZ(X) + 64.f) { continue; }
					const double Scale = Pebbles.FRandRange(8.f, I % 6 == 0 ? 39.f : 24.f) / Radius;
					const FRotator Rotation(0.f, Pebbles.FRandRange(0.f, 360.f), 0.f);
					Bars.Emplace(Rotation, FVector(P, Z + Bounds.BoxExtent.Z * Scale * 0.18) - Rotation.RotateVector(Bounds.Origin * Scale), FVector(Scale));
				}
			}
		}
		for (int32 I = 0; I < 310; ++I)
		{
			const float X = Pebbles.FRandRange(1330.f, ShoreX + 175.f);
			const FRiverSection Channel = RiverSection(X);
			const float Across = Pebbles.FRandRange(-1.f, 1.f);
			const FVector2D P(X, Channel.Center + Across * (Across < 0.f ? Channel.North : Channel.South));
			const float Z = HeightAt(P);
			if (Z < -33.f || Z > 29.f) { continue; }
			const double Scale = Pebbles.FRandRange(8.f, 33.f) / Radius;
			const FRotator Rotation(0.f, Pebbles.FRandRange(0.f, 360.f), 0.f);
			Bars.Emplace(Rotation, FVector(P, Z + Bounds.BoxExtent.Z * Scale * 0.22) - Rotation.RotateVector(Bounds.Origin * Scale), FVector(Scale));
		}
		Gravel->AddInstances(Bars, false, true);
		UE_LOG(LogSaga, Display, TEXT("[Saga] River gravel clusters: %d"), Bars.Num());

		for (int32 I = 0; I < 600; ++I)
		{
			const FVector2D P(Rocks.FRandRange(-11200.f, 1480.f), Rocks.FRandRange(-7100.f, 7250.f));
			if (P.X >= TT::FlatWestX - 180.f && FMath::Abs(P.Y) <= TT::FlatHalfY + 170.f) { continue; }
			if (P.X < TT::FlatWestX && FMath::Abs(P.Y - WesternTrailY(P.X)) < 285.f) { continue; }
			if (FMath::Abs(RiverSection(P.X).Cross(P.Y)) < 1.3f || Rocks.FRand() > MossCover(P)) { continue; }
			const float Z = HeightAt(P);
			if (Z < 0.f || Z > 1350.f) { continue; }
			const float Scale = Rocks.FRandRange(30.f, 110.f) / FMath::Max(1.0, FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y));
			const FRotator Rotation(0.f, Rocks.FRandRange(0.f, 360.f), 0.f);
			const float CenterZ = Z + Bounds.BoxExtent.Z * Scale * 0.27f;
			Clusters->AddInstance(FTransform(Rotation, FVector(P, CenterZ) - Rotation.RotateVector(Bounds.Origin * Scale), FVector(Scale)), true);
		}
	}
	UE_LOG(LogSaga, Display, TEXT("[Saga] Nordic Megascans scatter: %d mountain ledges, %d bank and forest clusters, %d river rocks"),
		Ledges ? Ledges->GetInstanceCount() : 0, Clusters ? Clusters->GetInstanceCount() : 0, Rapids ? Rapids->GetInstanceCount() : 0);

	// ----- River ribbon -----
	const float EndX = ShoreX + 145.f;
	TArray<float> Sx;
	for (float X = TT::RiverWestX; X < EndX; X += (X < TT::RiverFineX ? 340.f : 34.f))
	{
		Sx.Add(X);
	}
	if (Sx.Num() == 0 || Sx.Last() < EndX - 1.f)
	{
		Sx.Add(EndX);
	}

	TArray<FVector> RVerts;
	TArray<FVector> RNormals;
	TArray<FVector2D> RUV0;
	TArray<FLinearColor> RColors;
	TArray<FProcMeshTangent> RTangents;
	TArray<int32> RTris;
	const int32 NS = Sx.Num();
	constexpr int32 RiverColumns = 25;
	// Advance the material in travel-time coordinates, so wider sections move
	// more slowly without accumulating phase drift between neighbouring rows.
	constexpr float ReferenceWidth = 720.f;
	const float DeltaStart = ShoreX - 4500.f;
	const float DeltaEnd = ShoreX + 220.f;
	const FRiverSection Outlet = RiverSection(DeltaEnd);
	const float OutletWidth = Outlet.North + Outlet.South;
	float FlowDistance = 0.f;
	float PrevWidth = 0.f;
	FVector2D PrevC = FVector2D::ZeroVector;
	for (int32 I = 0; I < NS; ++I)
	{
		const float X = Sx[I];
		const FVector2D C(X, RiverY(X));
		const FRiverSection Channel = RiverSection(X);
		// The current starts spreading upstream of the visibly widened banks.
		const float DeltaBlend = SS(DeltaStart, DeltaEnd, X);
		const float Width = FMath::Lerp(Channel.North + Channel.South, OutletWidth, DeltaBlend);
		if (I > 0)
		{
			const float MeanWidth = 0.5f * (PrevWidth + Width);
			FlowDistance += (float)FVector2D::Distance(C, PrevC) * MeanWidth / ReferenceWidth;
		}
		PrevC = C;
		PrevWidth = Width;
		const float Slope = (RiverY(X + 1.f) - RiverY(X - 1.f)) * 0.5f;
		const FVector2D T = FVector2D(1.f, Slope).GetSafeNormal();
		const float Mouth = SS(ShoreX - 280.f, EndX, X);
		const float Z = FMath::Lerp(RiverZ(X), -8.f, SS(ShoreX - 345.f, EndX, X));
		float Rapid = 0.f;
		for (const float Reach : RapidCenters)
		{
			Rapid = FMath::Max(Rapid, 1.f - SS(54.f, 225.f, FMath::Abs(X - Reach)));
		}
		const float DeltaCalm = 1.f - DeltaBlend;
		for (int32 Column = 0; Column < RiverColumns; ++Column)
		{
			const float Across = static_cast<float>(Column) / (RiverColumns - 1);
			const FVector2D V(X, C.Y + FMath::Lerp(-Channel.North, Channel.South, Across));
			float Wake = 0.f;
			for (const TPair<FVector2D, float>& Obstacle : RiverObstacles)
			{
				const FVector2D Delta = V - Obstacle.Key;
				const float Radius = Obstacle.Value;
				const float Along = FVector2D::DotProduct(Delta, T) / Radius;
				if (Along < -1.6f || Along > 6.f) { continue; }
				const float Lateral = FMath::Abs(FVector2D::CrossProduct(Delta, T)) / Radius;
				const float Bow = (1.f - SS(0.20f, 0.83f, FMath::Abs(FMath::Sqrt(Along * Along + Lateral * Lateral) - 0.91f)))
					* (1.f - SS(-0.22f, 1.25f, Along));
				const float Tail = 0.62f * SS(-0.31f, 0.62f, Along) * (1.f - SS(0.95f, 3.55f, Along))
					* (1.f - SS(0.15f, 0.76f, FMath::Abs(Lateral - 0.42f - Along * 0.11f)));
				Wake = FMath::Max(Wake, FMath::Max(Bow, Tail));
			}
			RVerts.Add(FVector(V, Z));
			RNormals.Add(FVector::UpVector);
			RUV0.Add(FVector2D(Across, FlowDistance / TT::RiverVLen));
			const float Depth = FMath::Clamp((Z - HeightAt(V)) / 65.f, 0.f, 1.f);
			RColors.Add(FLinearColor(Mouth, Wake * DeltaCalm, Rapid * DeltaCalm, Depth));
			RTangents.Add(FProcMeshTangent(FVector(T, 0.f), false));
		}
	}
	for (int32 I = 0; I < NS - 1; ++I)
	{
		for (int32 Column = 0; Column < RiverColumns - 1; ++Column)
		{
			const int32 A = I * RiverColumns + Column;
			const int32 B = A + RiverColumns;
			RTris.Append({A, A + 1, B, B, A + 1, B + 1});
		}
	}
	River->ClearAllMeshSections();
	River->CreateMeshSection_LinearColor(0, RVerts, RTris, RNormals, RUV0, RColors, RTangents, false, false);
	River->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	River->SetCastShadow(false);
	River->bAffectDistanceFieldLighting = false;
	River->SetBoundsScale(1.08f);

	UMaterialInterface* RMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/River/M_SagaRiverClear.M_SagaRiverClear"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!RMat)
	{
		RMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/River/M_SagaRiver.M_SagaRiver"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	bool bFallback = false;
	if (!RMat)
	{
		UE_LOG(LogSaga, Warning, TEXT("[Saga] M_SagaRiver missing, the river uses the flat fallback material"));
		RMat = Flat;
		bFallback = true;
	}
	RiverMID = RMat ? UMaterialInstanceDynamic::Create(RMat, this) : nullptr;
	if (RiverMID)
	{
		River->SetMaterial(0, RiverMID);
		if (bFallback)
		{
			RiverMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.037f, 0.081f, 0.073f));
		}
	}

	// ----- Sounds (all optional) -----
	Ravens.Reset();
	Owls.Reset();
	for (const TCHAR* Name : { TEXT("SW_Raven_1"), TEXT("SW_Raven_2") })
	{
		if (USoundBase* S = LoadAmbience(Name)) { Ravens.Add(S); }
	}
	for (const TCHAR* Name : { TEXT("SW_Owl_1"), TEXT("SW_Owl_2") })
	{
		if (USoundBase* S = LoadAmbience(Name)) { Owls.Add(S); }
	}
	RiverLoopComp = StartLoop(this, LoadAmbience(TEXT("SW_RiverLoop")));
	WindLoopComp = StartLoop(this, LoadAmbience(TEXT("SW_WindLoop")));
	BirdsLoopComp = StartLoop(this, LoadAmbience(TEXT("SW_BirdsLoop")));
	for (float& V : MixSmooth) { V = 0.01f; }
}

float ATSCTerrain::GroundRelief(const FVector2D& P) const
{
	if (P.X < -6500.f || P.X > 1250.f || FMath::Abs(P.Y) > 3200.f) { return 0.f; }
	float Lift = 0.f;
	for (const FGroundMound& Mound : Mounds)
	{
		const FVector2D Offset = P - Mound.Center;
		if (Offset.SizeSquared() >= FMath::Square(Mound.Radius)) { continue; }
		const double Along = FVector2D::DotProduct(Offset, Mound.Axis);
		const double Across = FVector2D::CrossProduct(Offset, Mound.Axis);
		const float Distance = (Along * Along + 2.8 * Across * Across) / FMath::Square(Mound.Radius);
		const float Profile = FMath::Square(FMath::Max(0.f, 1.f - Distance));
		const float Shape = 0.62f + 0.38f * Fbm(P / 105.f);
		Lift = FMath::Max(Lift, Mound.Height * Profile * Shape);
	}
	return Lift;
}

bool ATSCTerrain::OverlapsTrail(const FVector2D& P, float Radius) const
{
	return TrailSegments.ContainsByPredicate([&](const FTrailSegment& Trail)
	{
		const FVector2D Delta = Trail.End - Trail.Start;
		const double T = FMath::Clamp(FVector2D::DotProduct(P - Trail.Start, Delta)
			/ FMath::Max(Delta.SizeSquared(), 1.0), 0.0, 1.0);
		return FVector2D::DistSquared(P, Trail.Start + Delta * T)
			< FMath::Square(Trail.HalfWidth + Radius + 18.f);
	});
}

void ATSCTerrain::BuildForestFloor(const ATSCOcean* Ocean, TFunctionRef<bool(const FVector2D&, float)> HasClearance,
	TFunctionRef<bool(const FVector2D&)> IsClearing)
{
	for (UInstancedStaticMeshComponent* Component : ForestFloor) { Component->DestroyComponent(); }
	ForestFloor.Reset();
	ClearingStumps = nullptr;

	Mounds.Reset();
	FRandomStream ReliefRandom(59403);
	for (float X = -5900.f; X < 1030.f; X += 365.f)
	{
		for (float Y = -2750.f; Y < 2750.f; Y += 345.f)
		{
			const FVector2D P(X + ReliefRandom.FRandRange(-120.f, 120.f),
				Y + ReliefRandom.FRandRange(-115.f, 115.f));
			const bool bClearing = IsClearing(P);
			const float Radius = bClearing ? ReliefRandom.FRandRange(145.f, 215.f)
				: ReliefRandom.FRandRange(180.f, 365.f);
			if (P.X + Radius > 990.f || FMath::Abs(P.Y) + Radius > 3200.f) { continue; }
			if (!HasClearance(P, bClearing ? Radius + 40.f : 22.f) || OverlapsTrail(P, Radius)) { continue; }
			if (ReliefRandom.FRand() > (bClearing ? 0.72f : 0.86f)) { continue; }
			const float Angle = ReliefRandom.FRandRange(0.f, 2.f * PI);
			const float Height = bClearing ? ReliefRandom.FRandRange(29.f, 57.f)
				: ReliefRandom.FRandRange(49.f, 108.f);
			Mounds.Add({P, FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)), Radius, Height});
		}
	}
	BuildLand(Ocean);
	UE_LOG(LogSaga, Display, TEXT("[Saga] %d blended forest knolls"), Mounds.Num());

	struct FScatter
	{
		const TCHAR* Name;
		const TCHAR* Path;
		float Step, MinRadius, MaxRadius, WoodlandDensity, ClearingDensity, Burial;
		bool bStump;
	};
	const FScatter ScatterLayers[] = {
		{TEXT("MossGrass"), TEXT("/Game/Vikings/Environment/ShorePlants/SM_Grass05.SM_Grass05"),
			92.f, 16.f, 42.f, 0.96f, 0.18f, 3.f, false},
		{TEXT("WoodlandFerns"), TEXT("/Game/Vikings/Environment/ShorePlants/SM_Fern02.SM_Fern02"),
			145.f, 28.f, 65.f, 0.75f, 0.07f, 2.f, false},
		{TEXT("MossyStones"), TEXT("/Game/Fab/Nordic_Forest_Cluster_Rock_Small/xjijdiz_tier_2/StaticMeshes/xjijdiz_tier_2.xjijdiz_tier_2"),
			138.f, 25.f, 83.f, 0.79f, 0.32f, 16.f, false},
		{TEXT("MossyLedges"), TEXT("/Game/Fab/Nordic_Forest_Ledge_Rock_Medium/xislecsqx_tier_2/StaticMeshes/xislecsqx_tier_2.xislecsqx_tier_2"),
			270.f, 65.f, 143.f, 0.72f, 0.12f, 24.f, false},
		{TEXT("ExposedRoots"), TEXT("/Game/Fab/Nordic_Forest_Tree_Stump_Birch_Medium/th5hcjjfa_tier_2/StaticMeshes/th5hcjjfa_tier_2.th5hcjjfa_tier_2"),
			255.f, 27.f, 53.f, 0.64f, 0.f, 29.f, false},
		{TEXT("ClearingStumps"), TEXT("/Game/Fab/Nordic_Forest_Tree_Stump_Birch_Medium/th5hcjjfa_tier_2/StaticMeshes/th5hcjjfa_tier_2.th5hcjjfa_tier_2"),
			390.f, 26.f, 41.f, 0.f, 0.82f, 5.f, true},
	};
	for (int32 Layer = 0; Layer < UE_ARRAY_COUNT(ScatterLayers); ++Layer)
	{
		const FScatter& Spec = ScatterLayers[Layer];
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Spec.Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mesh) { continue; }

		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, FName(Spec.Name));
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetupAttachment(RootComponent);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(Spec.bStump || Spec.Burial >= 16.f);
		Component->RegisterComponent();
		ForestFloor.Add(Component);
		if (Spec.bStump) { ClearingStumps = Component; }
		if (Spec.Burial == 16.f || Spec.Burial == 24.f)
		{
			if (UMaterialInstanceDynamic* Material = Component->CreateDynamicMaterialInstance(0))
			{
				Material->SetScalarParameterValue(TEXT("MossAmount"), 0.92f);
			}
		}

		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const FVector Foot = Bounds.Origin - FVector(0.f, 0.f, Bounds.BoxExtent.Z);
		const double MeshRadius = FMath::Max(1.0, FVector2D(Bounds.BoxExtent).Size());
		FRandomStream Random(62491 + Layer * 127);
		TArray<FTransform> Instances;
		for (float X = -6100.f; X < 1710.f; X += Spec.Step)
		{
			for (float Y = -3100.f; Y < 3100.f; Y += Spec.Step)
			{
				const FVector2D P(X + Random.FRandRange(-0.43f, 0.43f) * Spec.Step,
					Y + Random.FRandRange(-0.43f, 0.43f) * Spec.Step);
				const bool bClearing = IsClearing(P);
				const float Density = bClearing ? Spec.ClearingDensity : Spec.WoodlandDensity;
				if (Random.FRand() >= Density * (0.32f + 0.68f * MossCover(P))) { continue; }
				const float Radius = Random.FRandRange(Spec.MinRadius, Spec.MaxRadius) * (bClearing ? 0.78f : 1.f);
				if (!HasClearance(P, Radius)) { continue; }

				if (OverlapsTrail(P, Radius)) { continue; }

				const float Z = HeightAt(P);
				if (Z < TT::BaseZ - 1.f || Z > 600.f) { continue; }
				const double Scale = Radius / MeshRadius;
				const float Yaw = Random.FRandRange(0.f, 360.f);
				const FRotator Rotation(0.f, Yaw, 0.f);
				const double Burial = FMath::Min(double(Spec.Burial), Bounds.BoxExtent.Z * Scale * 1.15);
				Instances.Emplace(Rotation, FVector(P, Z - Burial) - Rotation.RotateVector(Foot * Scale), FVector(Scale));
			}
		}
		Component->AddInstances(Instances, false, true);
		UE_LOG(LogSaga, Display, TEXT("[Saga] %s: %d forest floor instances"), Spec.Name, Instances.Num());
	}
}

void ATSCTerrain::SetClearingStumpsVisible(bool bVisible)
{
	if (ClearingStumps) { ClearingStumps->SetVisibility(bVisible); }
}

void ATSCTerrain::BuildTrails(const TArray<TArray<FVector2D>>& Routes, const ATSCOcean* Ocean)
{
	TrailSegments.Reset();
	Trails->ClearAllMeshSections();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/M_SagaForestTrail.M_SagaForestTrail"));
	if (!Material || Routes.IsEmpty()) { return; }

	TArray<FVector> Verts, Normals;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Tris;
	for (int32 RouteIndex = 0; RouteIndex < Routes.Num(); ++RouteIndex)
	{
		TArray<FVector2D> Line;
		const TArray<FVector2D>& Route = Routes[RouteIndex];
		if (Route.Num() < 2) { continue; }
		if (RouteIndex == 0)
		{
			for (float X = -14300.f; X < Route[0].X; X += 120.f) { Line.Emplace(X, WesternTrailY(X)); }
		}
		Line.Append(Route);
		TArray<FVector2D> Points;
		for (int32 I = 0; I < Line.Num() - 1; ++I)
		{
			const FVector2D A = Line[I], B = Line[I + 1];
			const FVector2D Prev = I > 0 ? Line[I - 1] : A;
			const FVector2D Next = I + 2 < Line.Num() ? Line[I + 2] : B;
			const double Length = FVector2D::Distance(A, B);
			const FVector2D TA = (B - Prev).GetSafeNormal() * Length;
			const FVector2D TB = (Next - A).GetSafeNormal() * Length;
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length / 32.0));
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				Points.Add(FMath::CubicInterp(A, TA, B, TB, double(Step) / Steps));
			}
		}
		Points.Add(Line.Last());

		const int32 Start = Verts.Num();
		constexpr int32 Columns = 9;
		for (int32 I = 0; I < Points.Num(); ++I)
		{
			const FVector2D P = Points[I];
			const FVector2D T = (Points[FMath::Min(I + 1, Points.Num() - 1)] - Points[FMath::Max(0, I - 1)]).GetSafeNormal();
			const FVector2D Across(-T.Y, T.X);
			const float Width = (RouteIndex == 0 ? 126.f : 75.f) * (0.79f + 0.34f * N(P / 390.f));
			if (I > 0)
			{
				TrailSegments.Add({Points[I - 1], P, (RouteIndex == 0 ? 126.f : 75.f) * 1.13f});
			}
			const float Fade = FMath::Min(SS(0.f, 7.f, I), SS(0.f, 7.f, Points.Num() - 1 - I));
			for (int32 J = 0; J < Columns; ++J)
			{
				const float U = 2.f * J / (Columns - 1) - 1.f;
				const FVector2D V = P + Across * (U * Width);
				const float LandZ = HeightAt(V);
				const float Z = Ocean ? FMath::Max(LandZ, Ocean->BeachHeightAt(V)) : LandZ;
				Verts.Add(FVector(V, Z + 1.1f + RouteIndex * 0.45f));
				Normals.Add(FVector::UpVector);
				UV.Add(V / 1000.f);
				Colors.Add(FLinearColor(1.f, 1.f, 1.f, Fade * (1.f - SS(0.47f, 1.f, FMath::Abs(U)))));
				Tangents.Add(FProcMeshTangent(FVector(T, 0.f), false));
				if (I > 0 && J > 0)
				{
					const int32 D = Start + I * Columns + J;
					Tris.Append({D - Columns - 1, D - Columns, D - 1, D - 1, D - Columns, D});
				}
			}
		}
	}
	Trails->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UV, Colors, Tangents, false, false);
	Trails->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trails->SetCastShadow(false);
	Trails->bAffectDistanceFieldLighting = false;
	TrailMID = UMaterialInstanceDynamic::Create(Material, this);
	TrailMID->SetVectorParameterValue(TEXT("Color"), TSC::Pal::Dirt * 0.83f);
	Trails->SetMaterial(0, TrailMID);
}

void ATSCTerrain::SetTrailsVisible(bool bVisible)
{
	Trails->SetVisibility(bVisible);
}

void ATSCTerrain::StartGroundTransition(int32 Region, const FBox2D& Bounds, const FVector2D& Origin)
{
	const int32 Index = Region - 1;
	if (Index < 0 || Index >= int32(UE_ARRAY_COUNT(GroundTransitions))) { return; }

	GroundTransitions[Index] = {Origin, 0.f};
	const FName BoundsName(*FString::Printf(TEXT("AshBounds%d"), Region));
	const FLinearColor Rectangle(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y);
	if (LandMID) { LandMID->SetVectorParameterValue(BoundsName, Rectangle); }
	if (TrailMID) { TrailMID->SetVectorParameterValue(BoundsName, Rectangle); }
}

void ATSCTerrain::Sim(float Dt, const FTSCOceanInput& In, float Ash)
{
	if (ATSCOcean* Ocean = SprayRenderer.Get())
	{
		for (FRiverImpact& Impact : RiverImpacts)
		{
			if (FVector2D::DistSquared(FVector2D(In.Focus), FVector2D(Impact.Position)) > FMath::Square(12500.f)) { continue; }
			Impact.Cooldown -= Dt;
			if (Impact.Cooldown > 0.f) { continue; }
			Ocean->RiverSplash(Impact.Position, Impact.Downstream, SprayRandom);
			Impact.Cooldown = SprayRandom.FRandRange(0.22f, 0.68f);
		}
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(GroundTransitions); ++Index)
	{
		FGroundTransition& Transition = GroundTransitions[Index];
		if (Transition.Elapsed < 0.f || Transition.Elapsed > 3.52f) { continue; }

		Transition.Elapsed += Dt;
		const FName WaveName(*FString::Printf(TEXT("AshWave%d"), Index + 1));
		const FLinearColor Wave(Transition.Origin.X, Transition.Origin.Y, Transition.Elapsed, 1.f);
		if (LandMID) { LandMID->SetVectorParameterValue(WaveName, Wave); }
		if (TrailMID) { TrailMID->SetVectorParameterValue(WaveName, Wave); }
	}
	if (RiverMID)
	{
		RiverMID->SetScalarParameterValue(TEXT("RiverTime"), In.Time);
		RiverMID->SetScalarParameterValue(TEXT("Ash"), Ash);
	}
	if (FMath::Abs(Ash - LastAsh) > 0.01f)
	{
		if (LandMID) { LandMID->SetScalarParameterValue(TEXT("Ash"), Ash * 0.7f); }
		if (TrailMID) { TrailMID->SetScalarParameterValue(TEXT("Ash"), Ash); }
		if (RockMID) { RockMID->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(RockColor(), ATSCPrimActor::Ashify(RockColor()), Ash * 0.7f)); }
		const FLinearColor Understory(0.083f, 0.162f, 0.036f);
		for (UMaterialInstanceDynamic* Material : GroundPlantMIDs)
		{
			Material->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Understory, ATSCPrimActor::Ashify(Understory), Ash * 0.7f));
		}
		LastAsh = Ash;
	}

	// ----- Ambience, mixed by camera focus -----
	const float Riv = 1.f - RiverProximity(FVector2D(In.Focus));
	const float North = FMath::Clamp((-In.Focus.Y - 800.f) / 2000.f, 0.f, 1.f);
	const float West = FMath::Clamp((1200.f - In.Focus.X) / 3000.f, 0.f, 1.f);
	const float Targets[3] =
	{
		RiverVolume * Riv * Riv * FMath::Lerp(1.f, 0.55f, In.Zoom01),
		WindVolume * FMath::Lerp(0.2f, 1.f, FMath::Max(North, In.Zoom01)) * FMath::Lerp(1.f, 1.3f, Ash),
		BirdsVolume * (In.bDaylight ? 1.f : 0.f) * FMath::Lerp(0.35f, 1.f, West) * (1.f - Ash),
	};
	UAudioComponent* Comps[3] = { RiverLoopComp, WindLoopComp, BirdsLoopComp };
	for (int32 I = 0; I < 3; ++I)
	{
		MixSmooth[I] = FMath::FInterpTo(MixSmooth[I], Targets[I], Dt, 2.5f);
		if (Comps[I] && !Comps[I]->IsPlaying() && MixSmooth[I] > 0.02f) { Comps[I]->Play(); }  // in case the engine culled it while silent
		if (Comps[I] && FMath::Abs(MixSmooth[I] - Comps[I]->VolumeMultiplier) > 0.01f)
		{
			Comps[I]->SetVolumeMultiplier(MixSmooth[I]);
		}
	}
	if (CVarTerrainMixLog.GetValueOnGameThread() != 0)
	{
		MixLogTimer -= Dt;
		if (MixLogTimer <= 0.f)
		{
			MixLogTimer = 1.f;
			UE_LOG(LogSaga, Display, TEXT("[Saga] Terrain mix river=%.3f wind=%.3f birds=%.3f (targets %.3f %.3f %.3f)"), MixSmooth[0], MixSmooth[1], MixSmooth[2], Targets[0], Targets[1], Targets[2]);
		}
	}

	CallTimer -= Dt;
	if (CallTimer <= 0.f)
	{
		CallTimer = FMath::FRandRange(14.f, 34.f);
		if (Ash < 0.6f)
		{
			USoundBase* Call = In.bDaylight ? PickRandom(Ravens) : PickRandom(Owls);
			if (Call)
			{
				UGameplayStatics::PlaySound2D(this, Call, SfxVolume * FMath::FRandRange(0.4f, 0.8f), FMath::FRandRange(0.93f, 1.07f));
			}
		}
	}
}
