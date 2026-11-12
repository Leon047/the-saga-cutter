// The Saga Cutter - game jam project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TSCBuilders.h"
#include "TSCUnit.h"
#include "TSCGameMode.generated.h"

class ATSCPrimActor;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class USkyLightComponent;

enum class ETSCPhase : uint8 { Title, Day, Night, Epilogue };

/** Editable regions of the map; every object in a region has a Living and an Ashen version. */
namespace TSCRegion
{
	enum Type { None = 0, Forest = 1, Garden = 2, Grove = 3, Dock = 4 };
}

/** One editable scene: a scripted key event with two takes. */
struct FTSCScene
{
	FString Title;
	FString Alert;        // banner while the scene plays
	FString LivingName;
	FString LivingText;   // what happens if the scene is left as it is
	FString AshenName;
	FString AshenText;    // the alternate take: what it solves and what it costs
	int32 Region = 0;
	bool bStarted = false;
	bool bResolved = false;
	bool bAshen = false;
	bool bCutOpenable = false;
	float Window = 0.f;   // seconds left to press CUT!
	float Timer = 0.f;
	int32 Step = 0;
};

struct FTSCTracer
{
	FVector From;
	FVector To;
	FLinearColor Color;
	float Time;
};

struct FTSCToast
{
	FString Text;
	float Time;
};

struct FTSCEmber
{
	TObjectPtr<ATSCPrimActor> Actor;
	FVector Vel;
	float Life;
};

struct FTSCPlot
{
	FIntPoint Cell;
	bool bAvailable = false;
	bool bUsed = false;
	TSC::EBuilding Type = TSC::EBuilding::House;
	float Progress = 0.f;
	TObjectPtr<ATSCPrimActor> Marker;
	TObjectPtr<ATSCPrimActor> Building;
};

/**
 * The whole game: builds the clearing out of primitives, runs the three days and the final night, the two-resource
 * economy, the scripted scenes with the CUT! mechanic, and assembles the skald's epilogue.
 */
UCLASS()
class THESAGACUTTER_API ATSCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATSCGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ----- tunables ---------------------------------------------------------------------------------------------------
	/** Edits available for the whole game. */
	UPROPERTY(EditAnywhere, Category = "Saga")
	int32 MaxEdits = 3;
	UPROPERTY(EditAnywhere, Category = "Saga")
	float DayLength = 135.f;
	UPROPERTY(EditAnywhere, Category = "Saga")
	float NightLength = 85.f;
	/** Seconds between economy ticks. */
	UPROPERTY(EditAnywhere, Category = "Saga")
	float EconomyInterval = 5.f;

	// ----- state read by the HUD --------------------------------------------------------------------------------------
	ETSCPhase Phase = ETSCPhase::Title;
	int32 Day = 1;
	float DayTime = 0.f;
	float NightTime = 0.f;
	float Wood = 70.f;
	float Food = 70.f;
	int32 WoodRate = 0;
	int32 FoodRate = 0;
	int32 Pop = 0;
	int32 PopCap = 18;
	int32 EditsLeft = 3;
	int32 NumWood = 0;
	int32 NumFood = 0;
	int32 NumIdle = 0;
	float SpeedMul = 1.f;

	TArray<FTSCScene> Scenes;
	int32 ActiveScene = -1;
	bool bCutOpen = false;
	int32 SelectedTake = 0;
	/** > 0 while the world is changing saga after an edit. */
	float TransitionTimer = 0.f;
	FString TransitionTitle;

	FString Banner;
	float BannerTimer = 0.f;
	TArray<FTSCToast> Toasts;
	TArray<FTSCTracer> Tracers;
	TArray<FString> Epilogue;
	FString EpilogueTitle;

	float GateHP = 0.f;
	float GateMaxHP = 1.f;
	float HallHP = 0.f;
	float HallMaxHP = 1.f;
	bool bGateClosed = false;
	bool bGateBroken = false;
	bool bHammerShrine = false;
	bool bStagShrine = false;

	UPROPERTY()
	TArray<TObjectPtr<ATSCUnit>> Units;

	TArray<FTSCPlot> Plots;

	// ----- actions from the HUD ---------------------------------------------------------------------------------------
	void StartGame();
	void RestartGame();
	void AdjustJob(ETSCJob Job, int32 Delta);
	int32 BuildCost(TSC::EBuilding Type) const;
	bool CanBuild(TSC::EBuilding Type) const;
	int32 FreePlot() const;
	void StartBuild(TSC::EBuilding Type);
	bool CanCut() const;
	void PressCut();
	void SelectTake(int32 Take) { SelectedTake = Take; }
	void ConfirmCut();
	void ToggleSpeed();
	bool IsFrozen() const { return bCutOpen || TransitionTimer > 0.f || Phase == ETSCPhase::Title || Phase == ETSCPhase::Epilogue; }
	FString PhaseLabel() const;
	float PhaseFrac() const;

	// ----- services for units -----------------------------------------------------------------------------------------
	bool FindPath(const FVector& From, const FVector& To, TArray<FVector>& Out) const;
	int32 GridVersion = 0;
	bool CanReach(const ATSCUnit* A, const ATSCUnit* B) const;
	void GetAssaultGoal(const ATSCUnit* U, FVector& OutPos, ETSCStructure& OutStruct) const;
	void DamageStructure(ETSCStructure S, float Amount);
	void AddTracer(const FVector& From, const FVector& To, const FLinearColor& Col);
	void OnUnitDied(ATSCUnit* U);
	void Toast(const FString& Text);

	/** Test helper: plays the game by itself. Take pattern is a string of L/A per scene, e.g. "LALA". */
	UFUNCTION(Exec)
	void TSCAuto(const FString& Takes, const FString& Build, float Speed);
	UFUNCTION(Exec)
	void TSCSkip();
	/** Test helper: press CUT!, select a take (0/1) and optionally confirm. */
	UFUNCTION(Exec)
	void TSCCut(int32 Take, int32 bConfirm);

	static constexpr int32 GW = 34;
	static constexpr int32 GH = 24;

private:
	// world
	void BuildAtmosphere();
	void BuildWorld();
	ATSCPrimActor* SpawnPrim(const FVector& Loc, int32 Region = 0);
	ATSCUnit* SpawnUnit(ETSCRole InRole, const FString& Name, const FVector& Loc);
	FVector CellCenter(int32 Cx, int32 Cy) const;
	FVector CellCorner(int32 Cx, int32 Cy) const;
	FVector CellF(float Cx, float Cy) const;
	bool InGrid(int32 Cx, int32 Cy) const { return Cx >= 0 && Cy >= 0 && Cx < GW && Cy < GH; }
	bool IsBlocked(int32 Cx, int32 Cy) const { return !InGrid(Cx, Cy) || Blocked[Cy * GW + Cx] != 0; }
	void SetBlocked(int32 Cx, int32 Cy, bool bBlock);
	void BlockRect(int32 Cx, int32 Cy, int32 W, int32 H, bool bBlock = true);
	FIntPoint WorldToCell(const FVector& P) const;
	FIntPoint NearestFree(FIntPoint C) const;
	bool LineFree(const FVector& A, const FVector& B) const;
	int32 RegionOfCell(int32 Cx, int32 Cy) const;

	// flow
	void SimDay(float Dt);
	void SimNight(float Dt);
	void StartDay(int32 InDay);
	void StartNight();
	void EndNight(bool bHallFell);
	void BuildEpilogue(bool bHallFell);
	void EconomyTick();
	void ComputeRates(float& OutWood, float& OutFood) const;
	void UpdateJobs();
	void SimConstruction(float Dt);
	void UpdateAtmosphere(float Dt);
	void SimEmbers(float Dt);
	void SpawnEmbers(int32 Region, int32 Count);
	void SetGate(bool bClosed);
	void ShowBanner(const FString& Text, float Time);
	void AutoPlay(float Dt);

	// scenes
	void DefineScenes();
	void StartScene(int32 Idx);
	void SimScene(int32 Idx, float Dt);
	void ResolveLiving(int32 Idx);
	void ApplyAshen(int32 Idx);
	void FinishScene(int32 Idx);
	void TransitionRegion(int32 Region, const FVector& Origin);

	TArray<uint8> Blocked;
	UPROPERTY()
	TArray<TObjectPtr<ATSCPrimActor>> Prims;
	TArray<FTSCEmber> Embers;

	// special objects
	UPROPERTY() TObjectPtr<ATSCPrimActor> Gate;
	TArray<USceneComponent*> GateLeaves;
	float GateOpenAlpha = 1.f;
	UPROPERTY() TObjectPtr<ATSCPrimActor> EventTree;
	UPROPERTY() TObjectPtr<ATSCPrimActor> Garden;
	TArray<int32> CropParts;
	UPROPERTY() TObjectPtr<ATSCPrimActor> DockActor;
	TArray<int32> CrateParts;
	UPROPERTY() TObjectPtr<ATSCPrimActor> RaiderBoat;
	UPROPERTY() TObjectPtr<ATSCPrimActor> WaterActor;
	UPROPERTY() TObjectPtr<ATSCPrimActor> HammerStone;
	UPROPERTY() TObjectPtr<ATSCPrimActor> StagStone;

	// named villagers and event actors
	UPROPERTY() TObjectPtr<ATSCUnit> Hunter;
	UPROPERTY() TObjectPtr<ATSCUnit> Woodcutter;
	UPROPERTY() TObjectPtr<ATSCUnit> Healer;
	UPROPERTY() TObjectPtr<ATSCUnit> Shield;
	UPROPERTY() TObjectPtr<ATSCUnit> Boar;
	UPROPERTY() TArray<TObjectPtr<ATSCUnit>> EventUnits;
	UPROPERTY() TArray<TObjectPtr<ATSCUnit>> Workers;

	// atmosphere
	UPROPERTY() TObjectPtr<AActor> AtmoActor;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Fill;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> TopFill;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<UPostProcessComponent> PostFx;

	// economy modifiers set by the scenes
	float ForestMul = 1.f;
	float GardenMul = 1.f;
	float GroveBonus = 2.f;
	float DockBonus = 2.f;
	float DayFoodMul = 1.f;
	int32 ExtraSawmills = 0;
	int32 ExtraHealers = 0;
	int32 HousesBuilt = 0;
	bool bPendingTimber = false;
	bool bShieldWounded = false;
	bool bHungryNight = false;
	bool bFedNight = false;
	float StartNightWood = 0.f;
	int32 NightDefendersLost = 0;
	int32 NightVillagersAtStart = 0;
	bool bWestBreached = false;
	bool bEastBreached = false;
	int32 EnemiesSlain = 0;
	TArray<FString> BuiltNames;

	float EconomyTimer = 0.f;
	float WorldTime = 0.f;
	float AshAmount = 0.f;      // 0..1, how much of the world is Ashen (drives the colour drain)
	float FlashTimer = 0.f;
	int32 DayFlags = 0;
	bool bCameraSet = false;

	// autoplay (testing)
	bool bAuto = false;
	FString AutoTakes;
	FString AutoBuild;

public:
	float GetFlash() const { return FlashTimer; }
	float GetWorldTime() const { return WorldTime; }
	FVector HallDoor() const { return CellF(17.f, 13.6f); }
	float WallX() const { return CellCorner(8, 0).X; }
};
