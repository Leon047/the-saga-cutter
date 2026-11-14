// The Saga Cutter - game jam project.

#pragma once

#include "CoreMinimal.h"

class ATSCPrimActor;

/**
 * Recipes that turn an empty ATSCPrimActor into a game object.
 * Constructions (houses, palisade, dock...) are stacks of grid-sized flat tiles; scenery and props are 3D primitives.
 * Local space of a construction: origin at the north-west corner of its footprint, +X east, +Y south, Z up.
 */
namespace TSC
{
	constexpr float CELL = 200.f;

	namespace Pal
	{
		const FLinearColor Timber(0.20f, 0.115f, 0.06f);
		const FLinearColor TimberDark(0.10f, 0.06f, 0.035f);
		const FLinearColor Plank(0.26f, 0.17f, 0.09f);
		const FLinearColor Moss(0.10f, 0.19f, 0.07f);
		const FLinearColor Turf(0.13f, 0.21f, 0.06f);
		const FLinearColor Thatch(0.30f, 0.24f, 0.10f);
		const FLinearColor Stone(0.22f, 0.22f, 0.24f);
		const FLinearColor StoneDark(0.12f, 0.12f, 0.14f);
		const FLinearColor Grass(0.055f, 0.095f, 0.05f);
		const FLinearColor ForestFloor(0.035f, 0.07f, 0.04f);
		const FLinearColor Dirt(0.10f, 0.075f, 0.05f);
		const FLinearColor Soil(0.075f, 0.05f, 0.03f);
		const FLinearColor Shore(0.17f, 0.165f, 0.155f);
		const FLinearColor Water(0.015f, 0.05f, 0.08f);
		const FLinearColor Spruce(0.02f, 0.085f, 0.05f);
		const FLinearColor Char(0.028f, 0.024f, 0.022f);
		const FLinearColor Ash(0.075f, 0.07f, 0.065f);
		const FLinearColor Skin(0.62f, 0.42f, 0.30f);
		const FLinearColor Iron(0.20f, 0.21f, 0.23f);
		const FLinearColor Fire(1.0f, 0.42f, 0.08f);
		const FLinearColor Shroom(0.45f, 1.0f, 0.65f);
		const FLinearColor Red(0.45f, 0.05f, 0.04f);
	}

	enum class EBuilding : uint8 { House, Sawmill, Healer, HammerShrine, StagShrine };

	void BuildGroundTile(ATSCPrimActor* A, const FLinearColor& Living, const FLinearColor& Ashen);
	void BuildHouse(ATSCPrimActor* A, int32 Seed);
	void BuildLonghouse(ATSCPrimActor* A);
	void BuildSawmill(ATSCPrimActor* A);
	void BuildHealer(ATSCPrimActor* A);
	void BuildShrine(ATSCPrimActor* A, bool bHammer);
	void BuildPlotBuilding(ATSCPrimActor* A, EBuilding Type, int32 Seed);
	/** Crops gets the part indices of the plants, so the boar can trample them. */
	void BuildGarden(ATSCPrimActor* A, int32 W, int32 H, TArray<int32>& Crops);
	void BuildDock(ATSCPrimActor* A, int32 W, int32 H, TArray<int32>& Crates);
	/** One straight palisade segment along local Y from 0 to Len, supports facing local +X: an SM_WoodenFence section
	 *  (primitive log tiles without the mesh) and the Ashen stone footing. */
	void BuildPalisadeSegment(ATSCPrimActor* A, float Len, int32 Seed);
	/** Two cells high (in Y). Pivots[0] and Pivots[1] are the door leaves. */
	void BuildGate(ATSCPrimActor* A, TArray<USceneComponent*>& Pivots);
	void BuildTree(ATSCPrimActor* A, int32 Seed, bool bWithStump);
	void BuildStump(ATSCPrimActor* A);
	void BuildRock(ATSCPrimActor* A, int32 Seed);
	void BuildMushrooms(ATSCPrimActor* A, int32 Seed);
	void BuildRuneStone(ATSCPrimActor* A, bool bHammer);
	/** Returns the pivot at the foot of the sail (scale its Z to lower the sail), or null if the ship has none. */
	USceneComponent* BuildLongship(ATSCPrimActor* A);
	void BuildPlotMarker(ATSCPrimActor* A, int32 W, int32 H);
	void BuildLogPile(ATSCPrimActor* A);
	void BuildTorch(ATSCPrimActor* A);
	void BuildRaiderBoat(ATSCPrimActor* A);
	/** A barrel on its side (axis along local X), half in the water. Living only. */
	void BuildBarrel(ATSCPrimActor* A);
	/** Two or three charred planks (long axis local X) with a faint ember glow. Ashen only. */
	void BuildDriftwood(ATSCPrimActor* A, int32 Seed);
}
