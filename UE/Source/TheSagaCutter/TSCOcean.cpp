// The Saga Cutter - game jam project.

#include "TSCOcean.h"
#include "TSCTerrain.h"
#include "TSCPrimActor.h"

#include "TheSagaCutter.h"
#include "TSCBuilders.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundBase.h"

namespace
{
	/** One directional sine wave. The material reads the same table (Wave0..3 = Dir.X, Dir.Y, K, A; WaveSpeed = W0..W3). */
	struct FTSCWave
	{
		FVector2D Dir;
		float K;  // 2ПЂ / wavelength
		float A;  // amplitude
		float W;  // angular speed, 0.6 * sqrt(980 * K)
	};

	FTSCWave MakeWave(float DirX, float DirY, float Wavelength, float Amplitude)
	{
		FTSCWave Wv;
		Wv.Dir = FVector2D(DirX, DirY).GetSafeNormal();
		Wv.K = 2.f * PI / Wavelength;
		Wv.A = Amplitude;
		Wv.W = 0.6f * FMath::Sqrt(980.f * Wv.K);
		return Wv;
	}

	const FTSCWave GWaves[4] =
	{
		MakeWave(-1.0f, 0.25f, 1900.f, 9.0f),
		MakeWave(-0.8f, -0.6f, 1100.f, 5.0f),
		MakeWave(-0.3f, 1.0f, 620.f, 2.5f),
		MakeWave(-1.0f, -0.1f, 330.f, 1.2f),
	};

	/** Calm at the shore so the water never pokes through the beach or the dock. D = distance out to sea from the waterline. */
	float Atten(float D, const FVector2D& P)
	{
		const float RiverOpening = ATSCTerrain::RiverMouthCut(P.X, P.Y) / 34.f;
		const float Beach = 1.f - FMath::SmoothStep(0.06f, 0.97f, RiverOpening);
		return FMath::Lerp(0.15f, 1.f, FMath::SmoothStep(0.f, 1600.f, D))
			* FMath::Lerp(1.f, FMath::SmoothStep(-12.f, 65.f, D), Beach);
	}

	/**
	 * Fractal shoreline: waterline X = ShoreX + S(y), S = soft clamp of Bias + sum A_i sin(F_i y + P_i) to [ShoreMin, ShoreMax].
	 * The material reads the same table (Shore0..4 = A, F, P; ShoreBias, ShoreMin, ShoreMax). Positive S = land reaches further out.
	 * Seaward bias on purpose: level props and rocks (X <= ~2130) stay on land, the moored longship (west end X ~2420) stays afloat.
	 */
	struct FTSCShoreOctave
	{
		float A;  // amplitude
		float F;  // angular frequency along Y
		float P;  // phase
	};

	FTSCShoreOctave MakeOctave(int32 I, float Phase)
	{
		return { 78.f * FMath::Pow(0.58f, (float)I), 2.f * PI / 2900.f * FMath::Pow(2.17f, (float)I), Phase };
	}

	// Phases picked so the shore stays at or seaward of the old line along the dock (|Y| <= 650) and the log-wall ends (|Y| ~2500),
	// and the land stays clear of the moored longship (Y 380..580: S <= 110).
	const FTSCShoreOctave GShore[5] = { MakeOctave(0, 1.31f), MakeOctave(1, 2.85f), MakeOctave(2, 0.07f), MakeOctave(3, 4.12f), MakeOctave(4, 5.00f) };
	constexpr float ShoreBias = 60.f;
	constexpr float ShoreMin = -60.f;
	constexpr float ShoreMax = 180.f;

	float ShoreOffset(float Y)
	{
		float S = ShoreBias;
		for (const FTSCShoreOctave& O : GShore)
		{
			S += O.A * FMath::Sin(O.F * Y + O.P);
		}
		// Soft clamp: smoothstep over a range a quarter wider on each side keeps slope ~1 in the middle and flattens into the limits.
		const float Pad = (ShoreMax - ShoreMin) * 0.25f;
		return ShoreMin + (ShoreMax - ShoreMin) * FMath::SmoothStep(ShoreMin - Pad, ShoreMax + Pad, S);
	}

	/** Beach heights: flat top, then a wet slope that crosses the water rest level exactly at the waterline. */
	constexpr float BeachTopZ = 4.f;
	constexpr float BeachTopEnd = -20.f;   // top ends this far before the waterline
	constexpr float BeachToeX = 100.f;     // slope toe this far past the waterline
	constexpr float BeachToeZ = -20.f;

	float BeachCrest(float Y)
	{
		return BeachTopZ + 2.7f * FMath::Sin(Y / 293.f) + 1.6f * FMath::Sin(Y / 96.f + 0.8f);
	}

	float BeachBack(float Y)
	{
		return 1010.f + 72.f * FMath::Sin(Y / 430.f) + 48.f * FMath::Sin(Y / 159.f);
	}

	USoundBase* LoadAmbience(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Game/Vikings/Audio/Ambience/%s.%s"), Name, Name);
		return LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	constexpr int32 MaxDrops = 200;

	/** S8 mix: the surf loop is ~11 dB quieter than the music masters, so it gets this extra gain on top of SurfVolume (S13: 1.8 -> 1.1, quieter). */
	constexpr float SurfGain = 1.1f;

	/**
	 * Shore lapping bands, shared with the material (scalars LapRate, LapLength): band = smoothstep(0.86, 0.92, frac(dn / LapLength + t * LapRate)),
	 * travelling toward the shore. A band's leading edge touches the waterline when frac(t * LapRate) = LapArrive.
	 * The surf swells with a cos^2 envelope that peaks LapWashDelay (in phase) after that: the wash.
	 */
	constexpr float LapRate = 0.11f;
	constexpr float LapLength = 460.f;
	constexpr float LapArrive = 0.89f;
	constexpr float LapWashDelay = 0.03f;

	TAutoConsoleVariable<int32> CVarSurfLog(TEXT("tsc.OceanSurfLog"), 0, TEXT("1 = log the surf phase, envelope and volume 4x per second (ocean audio timing check)."));

	template <typename T>
	T* PickRandom(const TArray<TObjectPtr<T>>& Arr)
	{
		return Arr.Num() ? Arr[FMath::RandRange(0, Arr.Num() - 1)].Get() : nullptr;
	}
}

ATSCOcean::ATSCOcean()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Beach = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Beach"));
	Beach->SetupAttachment(Mesh);
}

float ATSCOcean::HeightAt(const FVector2D& P) const
{
	float Sum = 0.f;
	for (const FTSCWave& Wv : GWaves)
	{
		Sum += Wv.A * FMath::Sin(Wv.K * FVector2D::DotProduct(Wv.Dir, P) - Wv.W * Time);
	}
	return SurfaceZ + Atten(P.X - ShoreXAt(P.Y), P) * Sum;
}

float ATSCOcean::ShoreXAt(float Y) const
{
	return ShoreX + ShoreOffset(Y);
}

FVector2D ATSCOcean::SlopeAt(const FVector2D& P) const
{
	FVector2D G = FVector2D::ZeroVector;
	for (const FTSCWave& Wv : GWaves)
	{
		G += Wv.Dir * (Wv.A * Wv.K * FMath::Cos(Wv.K * FVector2D::DotProduct(Wv.Dir, P) - Wv.W * Time));
	}
	return G * Atten(P.X - ShoreXAt(P.Y), P);
}

void ATSCOcean::Build(float InShoreX, float InSurfaceZ)
{
	ShoreX = InShoreX;
	SurfaceZ = InSurfaceZ;

	// Grid lines: dense where the camera can look, stretched skirts beyond. Weight = vertex colour R.
	TArray<float> Xs, XW, Ys, YW;
	for (float X = ShoreX - 300.f; X <= ShoreX + 7200.f + 1.f; X += 100.f) { Xs.Add(X); XW.Add(1.f); }
	const float XSkirts[5] = { 9000.f, 12000.f, 18000.f, 30000.f, 50000.f };
	for (int32 S = 0; S < 5; ++S) { Xs.Add(ShoreX + XSkirts[S]); XW.Add(1.f - (S + 1) / 5.f); }

	const float YSkirts[5] = { 7500.f, 10000.f, 15000.f, 25000.f, 45000.f };
	for (int32 S = 4; S >= 0; --S) { Ys.Add(-YSkirts[S]); YW.Add(1.f - (S + 1) / 5.f); }
	for (float Y = -6000.f; Y <= 6000.f + 1.f; Y += 100.f) { Ys.Add(Y); YW.Add(1.f); }
	for (int32 S = 0; S < 5; ++S) { Ys.Add(YSkirts[S]); YW.Add(1.f - (S + 1) / 5.f); }

	const int32 NX = Xs.Num(), NY = Ys.Num();
	TArray<FVector> Verts;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Tris;
	Verts.Reserve(NX * NY);
	Normals.Reserve(NX * NY);
	UV0.Reserve(NX * NY);
	Colors.Reserve(NX * NY);
	Tangents.Reserve(NX * NY);
	Tris.Reserve((NX - 1) * (NY - 1) * 6);

	// Index = Ix * NY + Iy.
	for (int32 Ix = 0; Ix < NX; ++Ix)
	{
		for (int32 Iy = 0; Iy < NY; ++Iy)
		{
			const float Y = Ys[Iy];
			const float Mouth = ATSCTerrain::RiverMouthCut(ShoreXAt(Y), Y) / 34.f;
			// Give the two surfaces room to overlap around the outlet and gravel bar.
			const float Inland = 1.f - FMath::SmoothStep(ShoreX - 300.f, ShoreX + 100.f, Xs[Ix]);
			const float X = Xs[Ix] - 420.f * Mouth * Inland;
			const float Feather = FMath::SmoothStep(0.02f, 0.24f, Mouth)
				* (1.f - FMath::SmoothStep(ShoreX - 300.f, ShoreX - 100.f, Xs[Ix]));
			Verts.Add(FVector(X, Y, SurfaceZ));
			Normals.Add(FVector::UpVector);
			UV0.Add(FVector2D(X, Y) / 1000.f);
			const float RiverOpening = ATSCTerrain::RiverMouthCut(X, Y) / 34.f;
			Colors.Add(FLinearColor(FMath::Min(XW[Ix], YW[Iy]), RiverOpening, Feather, 1.f));
			Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
		}
	}
	// Same winding as UKismetProceduralMeshLibrary::CreateGridMeshWelded (faces +Z): (x,y) (x,y+1) (x+1,y), (x+1,y) (x,y+1) (x+1,y+1).
	for (int32 Ix = 0; Ix < NX - 1; ++Ix)
	{
		for (int32 Iy = 0; Iy < NY - 1; ++Iy)
		{
			const int32 I00 = Ix * NY + Iy;
			const int32 I01 = I00 + 1;
			const int32 I10 = I00 + NY;
			const int32 I11 = I10 + 1;
			Tris.Add(I00); Tris.Add(I01); Tris.Add(I10);
			Tris.Add(I10); Tris.Add(I01); Tris.Add(I11);
		}
	}
	Mesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UV0, Colors, Tangents, false, false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bAffectDistanceFieldLighting = false;
	Mesh->SetBoundsScale(1.2f);

	BuildBeach();
	BuildShorePlants();

	// Material: the ocean material, or the flat one in the old water colour.
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/Ocean/M_SagaOcean.M_SagaOcean"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	bool bFallback = false;
	if (!Mat)
	{
		UE_LOG(LogSaga, Warning, TEXT("[Saga] M_SagaOcean missing, the ocean uses the flat fallback material"));
		Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/M_SagaFlat.M_SagaFlat"));
		bFallback = true;
	}
	MID = Mat ? UMaterialInstanceDynamic::Create(Mat, this) : nullptr;
	if (MID)
	{
		Mesh->SetMaterial(0, MID);
		if (bFallback)
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.015f, 0.05f, 0.08f));
		}
		MID->SetScalarParameterValue(TEXT("ShoreX"), ShoreX);
		MID->SetScalarParameterValue(TEXT("SurfaceZ"), SurfaceZ);
		static const FName WaveNames[4] = { TEXT("Wave0"), TEXT("Wave1"), TEXT("Wave2"), TEXT("Wave3") };
		for (int32 I = 0; I < 4; ++I)
		{
			MID->SetVectorParameterValue(WaveNames[I], FLinearColor(GWaves[I].Dir.X, GWaves[I].Dir.Y, GWaves[I].K, GWaves[I].A));
		}
		MID->SetVectorParameterValue(TEXT("WaveSpeed"), FLinearColor(GWaves[0].W, GWaves[1].W, GWaves[2].W, GWaves[3].W));
		static const FName ShoreNames[5] = { TEXT("Shore0"), TEXT("Shore1"), TEXT("Shore2"), TEXT("Shore3"), TEXT("Shore4") };
		for (int32 I = 0; I < 5; ++I)
		{
			MID->SetVectorParameterValue(ShoreNames[I], FLinearColor(GShore[I].A, GShore[I].F, GShore[I].P, 0.f));
		}
		MID->SetScalarParameterValue(TEXT("ShoreBias"), ShoreBias);
		MID->SetScalarParameterValue(TEXT("ShoreMin"), ShoreMin);
		MID->SetScalarParameterValue(TEXT("ShoreMax"), ShoreMax);
		MID->SetScalarParameterValue(TEXT("LapRate"), LapRate);
		MID->SetScalarParameterValue(TEXT("LapLength"), LapLength);
	}

	// Sounds (all optional).
	SurfLoop = LoadAmbience(TEXT("SW_OceanLoop"));
	Gulls.Reset();
	Creaks.Reset();
	for (const TCHAR* Name : { TEXT("SW_Gull_1"), TEXT("SW_Gull_2"), TEXT("SW_Gull_3") })
	{
		if (USoundBase* S = LoadAmbience(Name)) { Gulls.Add(S); }
	}
	for (const TCHAR* Name : { TEXT("SW_BoatCreak_1"), TEXT("SW_BoatCreak_2") })
	{
		if (USoundBase* S = LoadAmbience(Name)) { Creaks.Add(S); }
	}
	Landing = LoadAmbience(TEXT("SW_BoatLand"));
	Wades.Reset();
	for (const TCHAR* Name : { TEXT("SW_Wade_1"), TEXT("SW_Wade_2") })
	{
		if (USoundBase* S = LoadAmbience(Name)) { Wades.Add(S); }
	}

	// Splash droplets: one instanced engine sphere in a foam-coloured flat material.
	if (!Drops)
	{
		Drops = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Drops"));
		Drops->SetMobility(EComponentMobility::Movable);
		Drops->SetupAttachment(RootComponent);
		Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Drops->SetCastShadow(false);
		Drops->RegisterComponent();
		if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
		{
			Drops->SetStaticMesh(Sphere);
		}
		if (UMaterialInterface* Flat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/M_SagaFlat.M_SagaFlat"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UMaterialInstanceDynamic* DropMID = UMaterialInstanceDynamic::Create(Flat, this);
			DropMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.80f, 0.90f, 0.92f));
			DropMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.20f, 0.24f, 0.25f));  // stays foam-white under the night lights
			Drops->SetMaterial(0, DropMID);
		}
	}

	if (SurfLoop)
	{
		Surf = UGameplayStatics::SpawnSound2D(this, SurfLoop, 0.01f, 1.f, 0.f, nullptr, false, false);
		if (Surf)
		{
			Surf->bIsUISound = true;
			Surf->FadeIn(2.f, 1.f);  // the fade level multiplies VolumeMultiplier for the whole life of the sound, so it must end at 1
		}
		UE_LOG(LogSaga, Display, TEXT("[Saga] Ocean surf loop %s: component %s, playing %d"), *SurfLoop->GetName(), Surf ? TEXT("created") : TEXT("NOT created"), Surf && Surf->IsPlaying() ? 1 : 0);
	}
}

float ATSCOcean::BeachHeightAt(const FVector2D& P) const
{
	const float Waterline = ShoreXAt(P.Y);
	const float DryEnd = Waterline + BeachTopEnd;
	const float Back = BeachBack(P.Y);
	const float Span = FMath::Max(1.f, DryEnd - Back);
	const float DryT = FMath::Clamp((float(P.X) - Back) / Span, 0.f, 1.f);
	const float WetT = FMath::Clamp((float(P.X) - DryEnd) / (BeachToeX - BeachTopEnd), 0.f, 1.f);
	const float Z = P.X <= DryEnd ? FMath::Lerp(-13.f, BeachCrest(P.Y), FMath::SmoothStep(0.f, 0.42f, DryT))
		: FMath::Lerp(BeachCrest(P.Y), BeachToeZ, WetT);
	return Z - ATSCTerrain::RiverMouthCut(P.X, P.Y);
}

void ATSCOcean::HideBeachApron()
{
	Beach->SetVisibility(false);
}

void ATSCOcean::BuildBeach()
{
	// Columns along Y: dense where the camera can look, coarse beyond.
	TArray<float> Ys;
	for (float Y = -45000.f; Y < -7000.f - 1.f; Y += 500.f) { Ys.Add(Y); }
	for (float Y = -7000.f; Y < 7000.f - 1.f; Y += 25.f) { Ys.Add(Y); }
	for (float Y = 7000.f; Y <= 45000.f + 1.f; Y += 500.f) { Ys.Add(Y); }
	const int32 NY = Ys.Num();

	// The back of the beach starts below the meadow and rises gradually through it.
	// Vertex red supplies wetness, and green matches the grass to the ground beneath the apron.
	TArray<FVector> Verts, Normals;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Tris;
	constexpr int32 DryRows = 12;
	constexpr int32 WetRows = 6;
	for (int32 Row = 0; Row <= DryRows + WetRows; ++Row)
	{
		for (float Y : Ys)
		{
			const float Waterline = ShoreXAt(Y);
			const float DryEnd = Waterline + BeachTopEnd;
			const float WetT = FMath::Clamp(float(Row - DryRows) / WetRows, 0.f, 1.f);
			const float DryT = FMath::Min(float(Row) / DryRows, 1.f);
			const float Back = BeachBack(Y);
			const float X = Row <= DryRows ? FMath::Lerp(Back, DryEnd, DryT) : Waterline + FMath::Lerp(BeachTopEnd, BeachToeX, WetT);
			const FVector2D P(X, Y);
			Verts.Emplace(P, BeachHeightAt(P));
			Normals.Add(FVector::UpVector);
			UV.Add(P / 1000.f);
			const float Wetness = FMath::SmoothStep(-84.f, 40.f, X - Waterline);
			const float Meadow = 1.f - FMath::SmoothStep(3090.f, 3600.f, FMath::Abs(Y));
			Colors.Emplace(Wetness, Meadow, 0.f, 1.f);
			Tangents.Emplace(1.f, 0.f, 0.f);
		}
	}
	for (int32 Row = 0; Row < DryRows + WetRows; ++Row)
	{
		for (int32 Iy = 0; Iy < NY - 1; ++Iy)
		{
			const int32 A = Row * NY + Iy, B = A + 1, C = A + NY, D = C + 1;
			Tris.Append({A, B, C, C, B, D});
		}
	}
	Beach->ClearAllMeshSections();
	Beach->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UV, Colors, Tangents, false, false);
	Beach->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beach->SetCastShadow(false);
	Beach->bAffectDistanceFieldLighting = false;

	UMaterialInterface* Shore = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/M_SagaShore.M_SagaShore"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Shore)
	{
		Shore = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/M_SagaFlat.M_SagaFlat"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!Shore) { return; }
	BeachMID = UMaterialInstanceDynamic::Create(Shore, this);
	BeachMID->SetVectorParameterValue(TEXT("Color"), TSC::Pal::Shore);
	BeachMID->SetVectorParameterValue(TEXT("Meadow"), TSC::Pal::Grass);
	BeachMID->SetVectorParameterValue(TEXT("ForestFloor"), TSC::Pal::ForestFloor * 0.8f);
	Beach->SetMaterial(0, BeachMID);

	if (BeachCobbles) { BeachCobbles->DestroyComponent(); }
	BeachCobbles = nullptr;
	UStaticMesh* Cobbles = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Fab/Nordic_Forest_Cluster_Rock_Small/xjijdiz_tier_2/StaticMeshes/xjijdiz_tier_2.xjijdiz_tier_2"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Cobbles) { return; }
	BeachCobbles = NewObject<UInstancedStaticMeshComponent>(this);
	BeachCobbles->SetupAttachment(RootComponent);
	BeachCobbles->SetStaticMesh(Cobbles);
	BeachCobbles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeachCobbles->SetCanEverAffectNavigation(false);
	BeachCobbles->SetCastShadow(false);
	BeachCobbles->RegisterComponent();
	if (UMaterialInstanceDynamic* Material = BeachCobbles->CreateDynamicMaterialInstance(0))
	{
		Material->SetScalarParameterValue(TEXT("MossAmount"), 0.03f);
	}

	const FBoxSphereBounds Bounds = Cobbles->GetBounds();
	const double MeshRadius = FMath::Max(1.0, FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y));
	FRandomStream Random(13793);
	TArray<FTransform> Instances;
	for (float Y = -5800.f; Y <= 6800.f; Y += 74.f)
	{
		if (FMath::Abs(Y) < 490.f) { continue; } // dock and landing route
		const float Shoal = 0.52f + 0.27f * FMath::Sin(Y / 239.f) + 0.17f * FMath::Sin(Y / 713.f);
		if (Random.FRand() > Shoal) { continue; }
		const int32 Count = Random.RandRange(3, 9);
		for (int32 I = 0; I < Count; ++I)
		{
			const float SampleY = Y + Random.FRandRange(-58.f, 58.f);
			const float Offset = Random.FRandRange(-330.f, 45.f);
			const float X = ShoreXAt(SampleY) + Offset;
			if (ATSCTerrain::RiverMouthCut(X + 260.f, SampleY) > 2.f) { continue; }
			const float Wet = FMath::Clamp((Offset - BeachTopEnd) / (BeachToeX - BeachTopEnd), 0.f, 1.f);
			const float Z = FMath::Lerp(BeachCrest(SampleY), BeachToeZ, Wet);
			const double Scale = Random.FRandRange(8.f, I == 0 ? 49.f : 24.f) / MeshRadius;
			const FRotator Rotation(0.f, Random.FRandRange(0.f, 360.f), 0.f);
			Instances.Emplace(Rotation, FVector(X, SampleY, Z + Bounds.BoxExtent.Z * Scale * 0.20) - Rotation.RotateVector(Bounds.Origin * Scale), FVector(Scale));
		}
	}
	BeachCobbles->AddInstances(Instances, false, true);
	UE_LOG(LogSaga, Display, TEXT("[Saga] Beach cobble clusters: %d"), Instances.Num());
}

void ATSCOcean::BuildShorePlants()
{
	FRandomStream Random(71539);
	auto Scatter = [&](const TCHAR* Path, const TCHAR* Name, bool bFern) -> UMaterialInstanceDynamic*
	{
		UStaticMesh* Plant = LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Plant) { return nullptr; }
		UInstancedStaticMeshComponent* Plants = NewObject<UInstancedStaticMeshComponent>(this, Name);
		Plants->SetupAttachment(RootComponent);
		Plants->SetStaticMesh(Plant);
		Plants->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Plants->SetCanEverAffectNavigation(false);
		Plants->RegisterComponent();
		UMaterialInstanceDynamic* Material = Plants->CreateDynamicMaterialInstance(0);
		const FBoxSphereBounds Bounds = Plant->GetBounds();
		const FVector Foot(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z);
		for (float Y = -5700.f; Y <= 6600.f; Y += (bFern ? 365.f : 119.f))
		{
			const float JitterY = Y + Random.FRandRange(-45.f, 45.f);
			const float Edge = 1650.f + 115.f * FMath::Sin(JitterY / 418.f) + 70.f * FMath::Sin(JitterY / 148.f + 0.4f);
			const float Patch = 0.50f + 0.29f * FMath::Sin(Y / 325.f) + 0.17f * FMath::Sin(Y / 179.f + 1.1f);
			if (FMath::Abs(JitterY) < 460.f || Random.FRand() > Patch) { continue; }
			const float X = Edge + Random.FRandRange(bFern ? -190.f : -115.f, bFern ? 0.f : 125.f);
			if (ATSCTerrain::RiverMouthCut(X + 270.f, JitterY) > 2.f) { continue; }
			const float Scale = Random.FRandRange(bFern ? 0.21f : 0.37f, bFern ? 0.33f : 0.62f);
			const FRotator Rotation(0.f, Random.FRandRange(0.f, 360.f), 0.f);
			const FVector Location = FVector(X, JitterY, -8.f) - Rotation.RotateVector(Foot * Scale);
			Plants->AddInstance(FTransform(Rotation, Location, FVector(Scale)));
		}
		return Material;
	};

	ShoreGrassMID = Scatter(TEXT("/Game/Vikings/Environment/ShorePlants/SM_Grass05.SM_Grass05"), TEXT("ShoreGrass"), false);
	ShoreFernMID = Scatter(TEXT("/Game/Vikings/Environment/ShorePlants/SM_Fern02.SM_Fern02"), TEXT("ShoreFerns"), true);
}

int32 ATSCOcean::AddFloater(AActor* Actor, float HalfLen, float HalfWidth, float RestZ, float FoamRadius, bool bFree)
{
	FTSCFloater F;
	F.Actor = Actor;
	F.HalfLen = HalfLen;
	F.HalfWidth = HalfWidth;
	F.RestZ = RestZ;
	F.FoamRadius = FoamRadius;
	F.bFree = bFree;
	if (Actor)
	{
		F.Home = FVector2D(Actor->GetActorLocation());
		F.LastXY = F.Home;
	}
	return Floaters.Add(F);
}

void ATSCOcean::Sim(float Dt, const FTSCOceanInput& In)
{
	Time = In.Time;
	if (BeachMID)
	{
		BeachMID->SetVectorParameterValue(TEXT("Meadow"), In.MeadowColor);
		BeachMID->SetScalarParameterValue(TEXT("Ash"), In.Ash);
	}
	const FLinearColor LeafColor = TSC::Pal::Grass * 1.32f;
	const FLinearColor ShoreFoliage = FMath::Lerp(LeafColor, ATSCPrimActor::Ashify(LeafColor), In.Ash);
	if (ShoreGrassMID) { ShoreGrassMID->SetVectorParameterValue(TEXT("Color"), ShoreFoliage); }
	if (ShoreFernMID) { ShoreFernMID->SetVectorParameterValue(TEXT("Color"), ShoreFoliage * 0.85f); }

	// --- Material parameters ---
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("OceanTime"), Time);
		MID->SetScalarParameterValue(TEXT("Ash"), In.Ash);
		MID->SetVectorParameterValue(TEXT("SunGlint"), In.SunColor * In.SunIntensity);
		FLinearColor Foam[2] = { FLinearColor(0.f, 0.f, 0.f, 0.f), FLinearColor(0.f, 0.f, 0.f, 0.f) };
		int32 NumFoam = 0;
		for (const FTSCFloater& F : Floaters)
		{
			if (NumFoam >= 2) { break; }
			const AActor* A = F.Actor.Get();
			if (!A || A->IsHidden() || F.FoamRadius <= 0.f) { continue; }
			const FVector L = A->GetActorLocation();
			Foam[NumFoam++] = FLinearColor(L.X, L.Y, F.FoamRadius, 1.f);
		}
		MID->SetVectorParameterValue(TEXT("Foam0"), Foam[0]);
		MID->SetVectorParameterValue(TEXT("Foam1"), Foam[1]);
	}

	// --- Floaters: spring-damper buoyancy ---
	bool bCreaky = false;
	for (FTSCFloater& F : Floaters)
	{
		AActor* A = F.Actor.Get();
		if (!A || A->IsHidden())
		{
			F.bInit = false;
			continue;
		}
		FVector2D P(A->GetActorLocation());
		float Yaw = A->GetActorRotation().Yaw;

		auto Targets = [&](const FVector2D& At, float InYaw, float& OutZ, float& OutPitch, float& OutRoll)
		{
			const float YawRad = FMath::DegreesToRadians(InYaw);
			const FVector2D Fwd(FMath::Cos(YawRad), FMath::Sin(YawRad));
			const FVector2D Right(-Fwd.Y, Fwd.X);  // local +Y (starboard)
			const float HBow = HeightAt(At + Fwd * F.HalfLen);
			const float HStern = HeightAt(At - Fwd * F.HalfLen);
			const float HPort = HeightAt(At - Right * F.HalfWidth);
			const float HStar = HeightAt(At + Right * F.HalfWidth);
			OutZ = (HBow + HStern + HPort + HStar) * 0.25f + F.RestZ;
			OutPitch = FMath::RadiansToDegrees(FMath::Atan2(HBow - HStern, 2.f * F.HalfLen));
			OutRoll = FMath::RadiansToDegrees(FMath::Atan2(HPort - HStar, 2.f * F.HalfWidth));
		};

		const bool bSnap = !F.bInit || FVector2D::Distance(P, F.LastXY) > 400.f;
		if (bSnap)
		{
			float TZ, TP, TR;
			Targets(P, Yaw, TZ, TP, TR);
			F.Z = TZ;
			F.Pitch = FMath::Clamp(TP, -12.f, 12.f);
			F.Roll = FMath::Clamp(TR, -16.f, 16.f);
			F.VelZ = F.VelPitch = F.VelRoll = 0.f;
			F.Vel = FVector2D::ZeroVector;
			F.bInit = true;
		}
		else
		{
			float Left = Dt;
			while (Left > KINDA_SMALL_NUMBER)
			{
				const float H = FMath::Min(Left, 1.f / 60.f);
				Left -= H;
				if (F.bFree)
				{
					F.Vel += (-SlopeAt(P) * 900.f + (F.Home - P) * 0.35f - F.Vel * 0.8f) * H;
					P += F.Vel * H;
					Yaw += F.Vel.X * 0.02f;
				}
				float TZ, TP, TR;
				Targets(P, Yaw, TZ, TP, TR);
				F.VelZ += (22.f * (TZ - F.Z) - 3.6f * F.VelZ) * H;
				F.Z += F.VelZ * H;
				F.VelPitch += (16.f * (TP - F.Pitch) - 2.6f * F.VelPitch) * H;
				F.Pitch = FMath::Clamp(F.Pitch + F.VelPitch * H, -12.f, 12.f);
				F.VelRoll += (16.f * (TR - F.Roll) - 2.6f * F.VelRoll) * H;
				F.Roll = FMath::Clamp(F.Roll + F.VelRoll * H, -16.f, 16.f);
			}
		}
		A->SetActorLocationAndRotation(FVector(P, F.Z), FRotator(F.Pitch, Yaw, F.Roll));

		// Bow spray: boats moving faster than 60 u/s throw droplets off the bow, alternating sides.
		F.SprayTimer -= Dt;
		if (!F.bFree && !bSnap && Dt > KINDA_SMALL_NUMBER)
		{
			const FVector2D Move = P - F.LastXY;
			if (Move.Size() / Dt > 60.f && F.SprayTimer <= 0.f)
			{
				const FVector2D TravelDir = Move.GetSafeNormal();
				const FVector2D Side(-TravelDir.Y, TravelDir.X);
				Splash(P + TravelDir * F.HalfLen, 0.5f, F.bSprayLeft ? -Side : Side);
				F.bSprayLeft = !F.bSprayLeft;
				F.SprayTimer = 0.08f;
			}
		}
		F.LastXY = P;
		if (!F.bFree && FMath::Abs(F.VelPitch) + FMath::Abs(F.VelRoll) > 3.f)
		{
			bCreaky = true;
		}
	}

	// --- Audio, mixed by camera position ---
	Proximity = 1.f - FMath::Clamp((ShoreX - In.Focus.X - 400.f) / 5200.f, 0.f, 1.f);
	// Surf in time with the lapping bands: swells as a band reaches the waterline, recedes between bands.
	const float LapPh = FMath::Frac(Time * LapRate);
	const float LapEnv = FMath::Square(0.5f + 0.5f * FMath::Cos(2.f * PI * (LapPh - LapArrive - LapWashDelay)));
	const float SurfTarget = SurfGain * SurfVolume * FMath::Lerp(0.15f, 1.f, Proximity) * FMath::Lerp(1.f, 0.6f, In.Zoom01) * FMath::Lerp(1.f, 0.75f, In.Ash)
		* FMath::Lerp(0.45f, 1.f, LapEnv);
	float SurfApplied = SurfTarget;
	if (Surf)
	{
		// The component's multiplier holds the smoothed value, so the swell eases in and out instead of stepping.
		SurfApplied = FMath::FInterpTo(Surf->VolumeMultiplier, SurfTarget, Dt, 3.f);
		if (SurfApplied != Surf->VolumeMultiplier)  // no dead band: per-frame steps are tiny at high frame rates
		{
			Surf->SetVolumeMultiplier(SurfApplied);
		}
	}
	if (CVarSurfLog.GetValueOnGameThread() != 0 && FMath::FloorToInt((Time - Dt) * 4.f) != FMath::FloorToInt(Time * 4.f))
	{
		UE_LOG(LogSaga, Display, TEXT("[Saga] Surf t=%.2f Ph=%.3f Env=%.3f target=%.3f applied=%.3f surf=%d"), Time, LapPh, LapEnv, SurfTarget, SurfApplied, Surf ? 1 : 0);
	}
	GullTimer -= Dt;
	if (GullTimer <= 0.f)
	{
		if (In.bDaylight && In.Ash < 0.5f && Gulls.Num())
		{
			UGameplayStatics::PlaySound2D(this, PickRandom(Gulls), SfxVolume * FMath::FRandRange(0.35f, 0.8f) * FMath::Lerp(0.4f, 1.f, Proximity), FMath::FRandRange(0.92f, 1.1f));
		}
		GullTimer = FMath::FRandRange(9.f, 24.f);
	}
	CreakCooldown -= Dt;
	if (bCreaky && CreakCooldown <= 0.f && Proximity > 0.35f && Creaks.Num())
	{
		UGameplayStatics::PlaySound2D(this, PickRandom(Creaks), SfxVolume * 0.6f * Proximity, FMath::FRandRange(0.9f, 1.1f));
		CreakCooldown = FMath::FRandRange(4.f, 9.f);
	}
	WadeSoundCooldown -= Dt;

	// --- Shore break spray near the camera focus ---
	ShoreSprayTimer -= Dt;
	if (ShoreSprayTimer <= 0.f)
	{
		ShoreSprayTimer = FMath::Lerp(0.5f, 0.12f, LapEnv);  // shore break bursts with the wash
		const float SprayY = In.Focus.Y + FMath::FRandRange(-2600.f, 2600.f);
		Splash(FVector2D(ShoreXAt(SprayY) + FMath::FRandRange(40.f, 100.f), SprayY), 0.3f, FVector2D(-1.f, 0.f));
	}

	// --- Droplets: ballistic, die on re-entering the water ---
	for (int32 I = DropSim.Num() - 1; I >= 0; --I)
	{
		FDrop& D = DropSim[I];
		D.Vel.Z -= 620.f * Dt;
		D.Pos += D.Vel * Dt;
		D.Life -= Dt;
		const double Surface = D.RiverSurface == -BIG_NUMBER ? HeightAt(FVector2D(D.Pos)) : D.RiverSurface;
		if (D.Life <= 0.f || (D.Vel.Z < 0.f && D.Pos.Z < Surface))
		{
			DropSim.RemoveAtSwap(I, 1, EAllowShrinking::No);
		}
	}
	if (Drops && (DropSim.Num() > 0 || Drops->GetInstanceCount() > 0))
	{
		TArray<FTransform> Transforms;
		Transforms.Reserve(DropSim.Num());
		for (const FDrop& D : DropSim)
		{
			Transforms.Add(FTransform(FQuat::Identity, D.Pos, FVector(D.Size * FMath::Clamp(D.Life * 4.f, 0.f, 1.f))));
		}
		Drops->ClearInstances();
		if (Transforms.Num() > 0)
		{
			Drops->AddInstances(Transforms, false, /*bWorldSpace*/ true);
		}
	}
}

void ATSCOcean::PlayLanding(const FVector2D& At, float Volume)
{
	if (Landing)
	{
		UGameplayStatics::PlaySound2D(this, Landing, SfxVolume * Volume * FMath::Lerp(0.4f, 1.f, Proximity));
	}
	Splash(At + FVector2D(-40.f, 30.f), 2.f);
	Splash(At + FVector2D(40.f, -30.f), 2.f);
}

void ATSCOcean::Splash(const FVector2D& At, float Strength, const FVector2D& Dir)
{
	const int32 Count = FMath::Min(FMath::RoundToInt(6.f * Strength), MaxDrops - DropSim.Num());
	if (Count <= 0)
	{
		return;
	}
	const float Root = FMath::Sqrt(FMath::Max(Strength, 0.f));
	const FVector Start(At, HeightAt(At) + 4.f);
	const FVector2D Bias = Dir.GetSafeNormal() * 90.f * Root;
	for (int32 I = 0; I < Count; ++I)
	{
		const float Ang = FMath::FRandRange(0.f, 2.f * PI);
		const FVector2D Horiz = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * FMath::FRandRange(50.f, 150.f) * Root + Bias;
		FDrop D;
		D.Pos = Start;
		D.Vel = FVector(Horiz, FMath::FRandRange(170.f, 340.f) * Root);
		D.Life = FMath::FRandRange(0.5f, 0.9f);
		D.Size = FMath::FRandRange(0.09f, 0.16f) * (0.8f + 0.4f * Strength);
		DropSim.Add(D);
	}
}

void ATSCOcean::RiverSplash(const FVector& At, const FVector2D& Downstream, FRandomStream& Random)
{
	const FVector2D Side(-Downstream.Y, Downstream.X);
	const int32 Count = FMath::Min(Random.RandRange(5, 9), MaxDrops - DropSim.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FDrop& Drop = DropSim.AddDefaulted_GetRef();
		Drop.RiverSurface = At.Z;
		Drop.Pos = At + FVector(Side * Random.FRandRange(-14.f, 14.f), 3.f);
		Drop.Vel = FVector(Side * Random.FRandRange(-83.f, 83.f) - Downstream * Random.FRandRange(18.f, 63.f),
			Random.FRandRange(88.f, 205.f));
		Drop.Life = Random.FRandRange(0.28f, 0.65f);
		Drop.Size = Random.FRandRange(0.031f, 0.072f);
	}
}

void ATSCOcean::WadeStep(const FVector2D& At, const FVector2D& MoveDir)
{
	Splash(At, 0.35f, MoveDir);
	if (WadeSoundCooldown <= 0.f && Proximity > 0.3f && Wades.Num())
	{
		UGameplayStatics::PlaySound2D(this, PickRandom(Wades), SfxVolume * 0.45f * Proximity, FMath::FRandRange(0.9f, 1.15f));
		WadeSoundCooldown = FMath::FRandRange(0.3f, 0.5f);
	}
}
