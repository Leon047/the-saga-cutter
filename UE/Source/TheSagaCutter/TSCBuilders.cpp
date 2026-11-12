// The Saga Cutter - game jam project.

#include "TSCBuilders.h"

#include "TSCPrimActor.h"

namespace TSC
{
	namespace
	{
		const float T = 0.07f; // tile thickness (in cube scale units)
		const FLinearColor WindowGlow(9.f, 4.2f, 0.9f);
		const FLinearColor WindowDead(0.05f, 0.015f, 0.f);

		// ----- tile pieces -----------------------------------------------------------------------------------------

		int32 Floor(ATSCPrimActor* A, int32 Cx, int32 Cy, float Z, const FLinearColor& Col)
		{
			return A->AddPart(ETSCShape::Cube, FVector((Cx + 0.5f) * CELL, (Cy + 0.5f) * CELL, Z), FRotator::ZeroRotator,
				FVector(1.97f, 1.97f, 0.05f), Col);
		}

		/** Upright tile on a cell edge. Edge: 0 = north, 1 = east, 2 = south, 3 = west. */
		int32 Wall(ATSCPrimActor* A, int32 Cx, int32 Cy, int32 Edge, float Z0, const FLinearColor& Col, float WidthFrac = 1.f,
			float HeightFrac = 1.f, float Thick = T)
		{
			const float H = CELL * HeightFrac;
			if (Edge == 0 || Edge == 2)
			{
				const float Y = (Edge == 0 ? Cy : Cy + 1) * CELL;
				return A->AddPart(ETSCShape::Cube, FVector((Cx + 0.5f) * CELL, Y, Z0 + H * 0.5f), FRotator::ZeroRotator,
					FVector(2.f * WidthFrac, Thick, 2.f * HeightFrac), Col);
			}
			const float X = (Edge == 3 ? Cx : Cx + 1) * CELL;
			return A->AddPart(ETSCShape::Cube, FVector(X, (Cy + 0.5f) * CELL, Z0 + H * 0.5f), FRotator::ZeroRotator,
				FVector(Thick, 2.f * WidthFrac, 2.f * HeightFrac), Col);
		}

		/** Small tile laid on a wall tile (window, door, sign). */
		int32 Inset(ATSCPrimActor* A, int32 Cx, int32 Cy, int32 Edge, float ZCenter, float W, float H, const FLinearColor& Col)
		{
			if (Edge == 0 || Edge == 2)
			{
				const float Y = (Edge == 0 ? Cy : Cy + 1) * CELL;
				return A->AddPart(ETSCShape::Cube, FVector((Cx + 0.5f) * CELL, Y, ZCenter), FRotator::ZeroRotator,
					FVector(W / 100.f, T * 2.f, H / 100.f), Col);
			}
			const float X = (Edge == 3 ? Cx : Cx + 1) * CELL;
			return A->AddPart(ETSCShape::Cube, FVector(X, (Cy + 0.5f) * CELL, ZCenter), FRotator::ZeroRotator,
				FVector(T * 2.f, W / 100.f, H / 100.f), Col);
		}

		void WallWindow(ATSCPrimActor* A, int32 Cx, int32 Cy, int32 Edge, float Z0, const FLinearColor& Col)
		{
			Wall(A, Cx, Cy, Edge, Z0, Col);
			const int32 W = Inset(A, Cx, Cy, Edge, Z0 + CELL * 0.55f, 62.f, 58.f, FLinearColor(0.9f, 0.5f, 0.15f));
			A->SetAshen(W, Pal::Char);
			A->SetGlow(W, WindowGlow, WindowDead);
		}

		void WallDoor(ATSCPrimActor* A, int32 Cx, int32 Cy, int32 Edge, float Z0, const FLinearColor& Col)
		{
			Wall(A, Cx, Cy, Edge, Z0, Col);
			Inset(A, Cx, Cy, Edge, Z0 + 72.f, 80.f, 144.f, Pal::TimberDark);
		}

		/** Gable roof of tilted tiles, ridge along X, covering W x H cells. Some tiles are missing in the Ashen Saga. */
		void GableRoof(ATSCPrimActor* A, int32 W, int32 H, float ZBase, const FLinearColor& Col, int32 Seed, float AngleDeg = 36.f)
		{
			const float Over = 24.f;
			const float HalfSpan = H * CELL * 0.5f + Over;
			const float Ang = FMath::DegreesToRadians(AngleDeg);
			const float SlopeLen = HalfSpan / FMath::Cos(Ang);
			const int32 Pieces = FMath::Max(1, FMath::RoundToInt(SlopeLen / CELL));
			const float PieceLen = SlopeLen / Pieces;
			const float MidY = H * CELL * 0.5f;
			const FLinearColor Burnt = Pal::Char;

			for (int32 Cx = 0; Cx < W; ++Cx)
			{
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const float Dir = Side == 0 ? 1.f : -1.f; // side 0 = north slope, rises toward +Y
					const FVector Up(0.f, Dir * FMath::Cos(Ang), FMath::Sin(Ang));
					const FVector Start(0.f, MidY - Dir * HalfSpan, ZBase);
					const FQuat Rot = FRotationMatrix::MakeFromXY(FVector::XAxisVector, Up).ToQuat();
					for (int32 P = 0; P < Pieces; ++P)
					{
						FVector C = Start + Up * (PieceLen * (P + 0.5f));
						C.X = (Cx + 0.5f) * CELL;
						const int32 Idx = A->AddPartQ(ETSCShape::Cube, C, Rot, FVector(1.98f, PieceLen / 100.f * 0.985f, 0.06f),
							Col * (0.9f + 0.2f * (((Cx * 7 + P * 3 + Side * 5 + Seed) % 5) / 4.f)));
						A->SetAshen(Idx, Burnt);
						if (((Cx * 13 + P * 7 + Side * 3 + Seed * 5) % 7) < 2)
						{
							A->SetVis(Idx, ETSCVis::LivingOnly); // burnt away
						}
					}
				}
				const float Rise = HalfSpan * FMath::Tan(Ang);
				const int32 Ridge = A->AddPart(ETSCShape::Cube, FVector((Cx + 0.5f) * CELL, MidY, ZBase + Rise + 2.f),
					FRotator::ZeroRotator, FVector(2.f, 0.14f, 0.14f), Pal::TimberDark);
				A->SetAshen(Ridge, Burnt);
			}
		}

		/** Gable-end tiles under the roof on the west and east ends. */
		void GableEnds(ATSCPrimActor* A, int32 W, int32 H, float ZBase, const FLinearColor& Col, float AngleDeg = 36.f)
		{
			const float Rise = H * CELL * 0.5f * FMath::Tan(FMath::DegreesToRadians(AngleDeg));
			for (int32 End = 0; End < 2; ++End)
			{
				const float X = End == 0 ? 0.f : W * CELL;
				A->AddPart(ETSCShape::Cube, FVector(X, H * CELL * 0.5f, ZBase + Rise * 0.24f), FRotator::ZeroRotator,
					FVector(T, H * 2.f * 0.72f, Rise * 0.48f / 100.f), Col);
				A->AddPart(ETSCShape::Cube, FVector(X, H * CELL * 0.5f, ZBase + Rise * 0.66f), FRotator::ZeroRotator,
					FVector(T, H * 2.f * 0.34f, Rise * 0.36f / 100.f), Col);
			}
		}

		void Post(ATSCPrimActor* A, float X, float Y, float Z0, float H, const FLinearColor& Col, float Thick = 0.13f)
		{
			A->AddPart(ETSCShape::Cube, FVector(X, Y, Z0 + H * 0.5f), FRotator::ZeroRotator, FVector(Thick, Thick, H / 100.f), Col);
		}

		void ShellWalls(ATSCPrimActor* A, int32 W, int32 H, float Z0, const FLinearColor& Col, int32 DoorCx, int32 Seed)
		{
			for (int32 Cx = 0; Cx < W; ++Cx)
			{
				Wall(A, Cx, 0, 0, Z0, Col * 0.8f);
				if (Cx == DoorCx) { WallDoor(A, Cx, H - 1, 2, Z0, Col); }
				else if ((Cx + Seed) % 2 == 0) { WallWindow(A, Cx, H - 1, 2, Z0, Col); }
				else { Wall(A, Cx, H - 1, 2, Z0, Col); }
			}
			for (int32 Cy = 0; Cy < H; ++Cy)
			{
				Wall(A, W - 1, Cy, 1, Z0, Col * 0.8f);
				if ((Cy + Seed) % 2 == 1) { WallWindow(A, 0, Cy, 3, Z0, Col * 0.9f); }
				else { Wall(A, 0, Cy, 3, Z0, Col * 0.9f); }
			}
		}
	}

	// ----- ground ------------------------------------------------------------------------------------------------------

	void BuildGroundTile(ATSCPrimActor* A, const FLinearColor& Living, const FLinearColor& Ashen)
	{
		const int32 I = A->AddPart(ETSCShape::Cube, FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(2.f, 2.f, 0.03f), Living);
		A->SetAshen(I, Ashen);
	}

	// ----- constructions (tile stacks) ---------------------------------------------------------------------------------

	void BuildHouse(ATSCPrimActor* A, int32 Seed)
	{
		const int32 W = 2, H = 2;
		for (int32 X = 0; X < W; ++X) { for (int32 Y = 0; Y < H; ++Y) { Floor(A, X, Y, 4.f, Pal::Stone); } }
		ShellWalls(A, W, H, 4.f, Pal::Timber, Seed % 2, Seed);
		GableEnds(A, W, H, 4.f + CELL, Pal::Timber * 0.9f);
		GableRoof(A, W, H, 4.f + CELL, Pal::Turf, Seed);
		A->AddLight(FVector(CELL * (0.5f + Seed % 2), CELL * 2.f + 70.f, 110.f), Pal::Fire, 2.2f, 0.3f, 420.f);
	}

	void BuildLonghouse(ATSCPrimActor* A)
	{
		const int32 W = 6, H = 3;
		for (int32 X = 0; X < W; ++X) { for (int32 Y = 0; Y < H; ++Y) { Floor(A, X, Y, 6.f, Pal::Stone); } }
		// Walls: door in the middle of the south side, lit windows on the south and west.
		for (int32 X = 0; X < W; ++X)
		{
			Wall(A, X, 0, 0, 6.f, Pal::Timber * 0.8f);
			if (X == 2 || X == 3) { WallDoor(A, X, H - 1, 2, 6.f, Pal::Timber); }
			else { WallWindow(A, X, H - 1, 2, 6.f, Pal::Timber); }
		}
		for (int32 Y = 0; Y < H; ++Y)
		{
			Wall(A, W - 1, Y, 1, 6.f, Pal::Timber * 0.8f);
			if (Y == 1) { WallWindow(A, 0, Y, 3, 6.f, Pal::Timber * 0.9f); }
			else { Wall(A, 0, Y, 3, 6.f, Pal::Timber * 0.9f); }
		}
		GableEnds(A, W, H, 6.f + CELL, Pal::Timber * 0.9f, 40.f);
		GableRoof(A, W, H, 6.f + CELL, Pal::Moss, 3, 40.f);

		// Shields on the south wall.
		const FLinearColor ShieldCols[4] = { Pal::Red, FLinearColor(0.08f, 0.16f, 0.35f), FLinearColor(0.5f, 0.38f, 0.06f), Pal::Red };
		for (int32 I = 0; I < 4; ++I)
		{
			const float X = (I < 2 ? 0.5f + I : 3.5f + I) * CELL;
			A->AddPart(ETSCShape::Cylinder, FVector(X, H * CELL + 12.f, 168.f), FRotator(0.f, 0.f, 90.f), FVector(0.42f, 0.42f, 0.05f), ShieldCols[I]);
		}
		// Dragon-head gables: neck + snout at both ridge ends.
		const float Rise = (H * CELL * 0.5f + 24.f) * FMath::Tan(FMath::DegreesToRadians(40.f));
		const float RidgeZ = 6.f + CELL + Rise;
		for (int32 End = 0; End < 2; ++End)
		{
			const float Dir = End == 0 ? -1.f : 1.f;
			const float X = End == 0 ? 0.f : W * CELL;
			A->AddPart(ETSCShape::Cube, FVector(X + Dir * 26.f, H * CELL * 0.5f, RidgeZ + 44.f), FRotator(Dir * -28.f, 0.f, 0.f),
				FVector(0.16f, 0.14f, 1.05f), Pal::TimberDark);
			A->AddPart(ETSCShape::Cone, FVector(X + Dir * 84.f, H * CELL * 0.5f, RidgeZ + 96.f), FRotator(Dir * -78.f, 0.f, 0.f),
				FVector(0.3f, 0.26f, 0.7f), Pal::TimberDark);
			const int32 Eye = A->AddPart(ETSCShape::Sphere, FVector(X + Dir * 62.f, H * CELL * 0.5f, RidgeZ + 104.f), FRotator::ZeroRotator,
				FVector(0.09f, 0.34f, 0.09f), Pal::Fire);
			A->SetGlow(Eye, FLinearColor(6.f, 2.f, 0.3f), FLinearColor(0.6f, 0.1f, 0.f));
		}
		// Open hearth glow through the smoke hole and the door.
		const int32 Smoke = A->AddPart(ETSCShape::Cube, FVector(W * CELL * 0.5f, H * CELL * 0.5f, RidgeZ + 6.f), FRotator::ZeroRotator,
			FVector(0.9f, 0.3f, 0.12f), Pal::Fire);
		A->SetGlow(Smoke, FLinearColor(14.f, 5.f, 0.8f), FLinearColor(2.f, 0.5f, 0.05f));
		const int32 Hearth = A->AddPart(ETSCShape::Cube, FVector(W * CELL * 0.5f, H * CELL + 4.f, 60.f), FRotator::ZeroRotator,
			FVector(1.2f, 0.2f, 0.9f), Pal::Fire);
		A->SetGlow(Hearth, FLinearColor(10.f, 3.6f, 0.6f), FLinearColor(2.f, 0.5f, 0.05f));
		A->AddLight(FVector(W * CELL * 0.5f, H * CELL + 130.f, 120.f), Pal::Fire, 6.f, 2.5f, 900.f);
		A->AddLight(FVector(-120.f, H * CELL * 0.5f, 140.f), Pal::Fire, 3.f, 1.f, 600.f);
		// Table and benches outside the door.
		A->AddPart(ETSCShape::Cube, FVector(4.8f * CELL, H * CELL + 110.f, 38.f), FRotator::ZeroRotator, FVector(1.5f, 0.5f, 0.06f), Pal::Plank);
		A->AddPart(ETSCShape::Cube, FVector(4.8f * CELL, H * CELL + 60.f, 20.f), FRotator::ZeroRotator, FVector(1.4f, 0.16f, 0.05f), Pal::TimberDark);
		A->AddPart(ETSCShape::Cube, FVector(4.8f * CELL, H * CELL + 160.f, 20.f), FRotator::ZeroRotator, FVector(1.4f, 0.16f, 0.05f), Pal::TimberDark);
	}

	void BuildSawmill(ATSCPrimActor* A)
	{
		const int32 W = 3, H = 2;
		for (int32 X = 0; X < W; ++X) { for (int32 Y = 0; Y < H; ++Y) { Floor(A, X, Y, 4.f, Pal::Plank * (0.85f + 0.1f * ((X + Y) % 2))); } }
		for (int32 X = 0; X < W; ++X) { Wall(A, X, 0, 0, 4.f, Pal::Timber * 0.8f); }
		Wall(A, W - 1, 0, 1, 4.f, Pal::Timber * 0.8f);
		for (int32 X = 0; X <= W; ++X) { Post(A, X * CELL, H * CELL, 4.f, CELL, Pal::TimberDark); }
		Post(A, 0.f, 0.f, 4.f, CELL, Pal::TimberDark);
		GableRoof(A, W, H, 4.f + CELL, Pal::Plank * 0.7f, 5, 30.f);
		// Saw bench with an upright blade tile.
		A->AddPart(ETSCShape::Cube, FVector(1.5f * CELL, 1.2f * CELL, 46.f), FRotator::ZeroRotator, FVector(2.6f, 0.5f, 0.08f), Pal::Plank);
		A->AddPart(ETSCShape::Cylinder, FVector(1.5f * CELL, 1.2f * CELL, 62.f), FRotator(0.f, 0.f, 90.f), FVector(0.8f, 0.8f, 0.025f), Pal::Iron * 2.f);
		A->AddPart(ETSCShape::Cylinder, FVector(1.1f * CELL, 1.2f * CELL, 70.f), FRotator(90.f, 0.f, 0.f), FVector(0.28f, 0.28f, 1.3f), Pal::Timber * 1.3f);
		A->AddLight(FVector(1.5f * CELL, H * CELL + 40.f, 150.f), Pal::Fire, 2.2f, 0.4f, 480.f);
	}

	void BuildHealer(ATSCPrimActor* A)
	{
		const int32 W = 2, H = 2;
		for (int32 X = 0; X < W; ++X) { for (int32 Y = 0; Y < H; ++Y) { Floor(A, X, Y, 4.f, Pal::Stone); } }
		ShellWalls(A, W, H, 4.f, Pal::Timber * 1.1f, 0, 1);
		GableEnds(A, W, H, 4.f + CELL, Pal::Timber);
		GableRoof(A, W, H, 4.f + CELL, Pal::Thatch, 2);
		// Drying herbs hanging from a line under the south eave.
		A->AddPart(ETSCShape::Cube, FVector(CELL, H * CELL + 30.f, 190.f), FRotator::ZeroRotator, FVector(4.2f, 0.03f, 0.03f), Pal::TimberDark);
		for (int32 I = 0; I < 7; ++I)
		{
			const int32 Herb = A->AddPart(ETSCShape::Cone, FVector(30.f + I * 57.f, H * CELL + 30.f, 168.f), FRotator(180.f, 0.f, 0.f),
				FVector(0.16f, 0.16f, 0.4f), I % 2 ? FLinearColor(0.16f, 0.36f, 0.08f) : FLinearColor(0.34f, 0.3f, 0.06f));
			A->SetVis(Herb, I % 3 == 0 ? ETSCVis::Both : ETSCVis::LivingOnly);
		}
		// Green cross sign.
		const FLinearColor CrossGlow(0.5f, 3.f, 0.6f);
		int32 C = Inset(A, 1, H - 1, 2, 150.f, 50.f, 16.f, FLinearColor(0.2f, 0.8f, 0.25f));
		A->SetGlow(C, CrossGlow, CrossGlow * 0.3f);
		C = Inset(A, 1, H - 1, 2, 150.f, 16.f, 50.f, FLinearColor(0.2f, 0.8f, 0.25f));
		A->SetGlow(C, CrossGlow, CrossGlow * 0.3f);
		A->AddLight(FVector(CELL * 0.5f, CELL * 2.f + 70.f, 110.f), Pal::Fire, 2.2f, 0.3f, 420.f);
	}

	void BuildShrine(ATSCPrimActor* A, bool bHammer)
	{
		const int32 W = 2, H = 2;
		const FLinearColor Glow = bHammer ? FLinearColor(10.f, 4.f, 0.5f) : FLinearColor(1.5f, 8.f, 3.5f);
		for (int32 X = 0; X < W; ++X) { for (int32 Y = 0; Y < H; ++Y) { Floor(A, X, Y, 8.f, Pal::Stone * (0.9f + 0.15f * ((X + Y) % 2))); } }
		Post(A, 14.f, 14.f, 8.f, CELL * 1.1f, Pal::TimberDark);
		Post(A, W * CELL - 14.f, 14.f, 8.f, CELL * 1.1f, Pal::TimberDark);
		Post(A, 14.f, H * CELL - 14.f, 8.f, CELL * 1.1f, Pal::TimberDark);
		Post(A, W * CELL - 14.f, H * CELL - 14.f, 8.f, CELL * 1.1f, Pal::TimberDark);
		Wall(A, 0, 0, 0, 8.f, Pal::Timber * 0.8f);
		Wall(A, 1, 0, 0, 8.f, Pal::Timber * 0.8f);
		GableRoof(A, W, H, 8.f + CELL * 1.1f, bHammer ? Pal::Red * 0.8f : Pal::Moss, bHammer ? 1 : 4, 42.f);
		// Altar stone with a glowing sign.
		A->AddPart(ETSCShape::Cube, FVector(CELL, CELL, 60.f), FRotator::ZeroRotator, FVector(0.7f, 0.5f, 1.1f), Pal::StoneDark);
		int32 S = A->AddPart(ETSCShape::Cube, FVector(CELL, CELL + 27.f, 78.f), FRotator::ZeroRotator, FVector(0.1f, 0.04f, 0.5f), Pal::Fire);
		A->SetGlow(S, Glow, Glow * 0.5f);
		S = A->AddPart(ETSCShape::Cube, FVector(CELL, CELL + 27.f, bHammer ? 96.f : 100.f), FRotator(0.f, 0.f, 0.f),
			FVector(bHammer ? 0.4f : 0.5f, 0.04f, bHammer ? 0.16f : 0.07f), Pal::Fire);
		A->SetGlow(S, Glow, Glow * 0.5f);
		A->AddLight(FVector(CELL, CELL + 90.f, 110.f), bHammer ? Pal::Fire : Pal::Shroom, 4.f, 2.f, 600.f);
	}

	void BuildPlotBuilding(ATSCPrimActor* A, EBuilding Type, int32 Seed)
	{
		A->bStepParts = true;
		switch (Type)
		{
		case EBuilding::House: BuildHouse(A, Seed); break;
		case EBuilding::Sawmill: BuildSawmill(A); break;
		case EBuilding::Healer: BuildHealer(A); break;
		case EBuilding::HammerShrine: BuildShrine(A, true); break;
		case EBuilding::StagShrine: BuildShrine(A, false); break;
		}
		A->bStepParts = false;
	}

	void BuildGarden(ATSCPrimActor* A, int32 W, int32 H, TArray<int32>& Crops)
	{
		for (int32 X = 0; X < W; ++X)
		{
			for (int32 Y = 0; Y < H; ++Y)
			{
				const int32 S = Floor(A, X, Y, 3.f, Pal::Soil * (0.9f + 0.2f * ((X + Y) % 2)));
				A->SetAshen(S, Pal::Ash * 0.8f);
			}
		}
		auto Fence = [A, W](int32 Cx, int32 Cy, int32 Edge)
		{
			if (Edge == 0 && Cx == W / 2) { return; } // entrance
			int32 F = Wall(A, Cx, Cy, Edge, 0.f, Pal::Timber, 1.f, 0.3f);
			A->SetVis(F, ETSCVis::LivingOnly);
			F = Wall(A, Cx, Cy, Edge, 0.f, Pal::Stone, 1.f, 0.5f, 0.2f);
			A->SetAshen(F, Pal::StoneDark);
			A->SetVis(F, ETSCVis::AshenOnly);
		};
		for (int32 X = 0; X < W; ++X) { Fence(X, 0, 0); Fence(X, H - 1, 2); }
		for (int32 Y = 0; Y < H; ++Y) { Fence(0, Y, 3); Fence(W - 1, Y, 1); }
		// Crop rows.
		for (int32 X = 0; X < W; ++X)
		{
			for (int32 Y = 0; Y < H; ++Y)
			{
				for (int32 I = 0; I < 4; ++I)
				{
					const FVector P((X + 0.25f + 0.5f * (I % 2)) * CELL, (Y + 0.25f + 0.5f * (I / 2)) * CELL, 22.f);
					const bool bCabbage = (X + Y) % 2 == 0;
					const int32 C = A->AddPart(bCabbage ? ETSCShape::Sphere : ETSCShape::Cone, P, FRotator::ZeroRotator,
						bCabbage ? FVector(0.34f, 0.34f, 0.26f) : FVector(0.2f, 0.2f, 0.5f),
						bCabbage ? FLinearColor(0.18f, 0.42f, 0.1f) : FLinearColor(0.3f, 0.45f, 0.05f));
					A->SetAshen(C, FLinearColor(0.16f, 0.15f, 0.08f));
					if ((X * 3 + Y * 5 + I) % 3 == 0) { A->SetVis(C, ETSCVis::LivingOnly); }
					Crops.Add(C);
				}
			}
		}
	}

	void BuildDock(ATSCPrimActor* A, int32 W, int32 H, TArray<int32>& Crates)
	{
		for (int32 X = 0; X < W; ++X)
		{
			for (int32 Y = 0; Y < H; ++Y)
			{
				const int32 F = Floor(A, X, Y, 34.f, Pal::Plank * (0.8f + 0.15f * ((X * 2 + Y) % 3)));
				A->SetAshen(F, Pal::Char);
				if ((X * 5 + Y * 3) % 4 != 1 && X > 0) { A->SetVis(F, ETSCVis::LivingOnly); }
			}
		}
		for (int32 X = 0; X <= W; ++X)
		{
			for (int32 Y = 0; Y <= H; Y += H)
			{
				const int32 P = A->AddPart(ETSCShape::Cylinder, FVector(X * CELL, Y * CELL, 20.f), FRotator::ZeroRotator,
					FVector(0.18f, 0.18f, (X % 2 ? 0.7f : 1.1f)), Pal::TimberDark);
				A->SetAshen(P, Pal::Char);
			}
		}
		const FVector CratePos[4] = { FVector(1.4f * CELL, 0.4f * CELL, 66.f), FVector(1.9f * CELL, 0.45f * CELL, 60.f),
			FVector(1.6f * CELL, 0.42f * CELL, 118.f), FVector(3.3f * CELL, 1.6f * CELL, 62.f) };
		for (int32 I = 0; I < 4; ++I)
		{
			const int32 C = A->AddPart(ETSCShape::Cube, CratePos[I], FRotator(0.f, I * 17.f, 0.f), FVector(I == 1 ? 0.45f : 0.55f), Pal::Plank * 1.25f);
			A->SetVis(C, ETSCVis::LivingOnly);
			Crates.Add(C);
		}
		int32 L = A->AddPart(ETSCShape::Sphere, FVector(W * CELL, 0.f, 150.f), FRotator::ZeroRotator, FVector(0.2f), Pal::Fire);
		A->SetGlow(L, FLinearColor(12.f, 5.f, 0.8f), FLinearColor(0.f, 0.f, 0.f));
		A->SetVis(L, ETSCVis::LivingOnly);
		A->AddLight(FVector(W * CELL, 0.f, 170.f), Pal::Fire, 3.f, 0.f, 600.f);
	}

	void BuildPalisadeCell(ATSCPrimActor* A, int32 Seed)
	{
		// A row of sharpened log tiles along the west edge of the cell.
		for (int32 I = 0; I < 3; ++I)
		{
			const float Y = (I + 0.5f) * CELL / 3.f;
			const float H = 215.f + 26.f * ((Seed * 3 + I * 5) % 4);
			const FLinearColor Col = Pal::Timber * (0.75f + 0.12f * ((Seed + I * 2) % 4));
			A->AddPart(ETSCShape::Cube, FVector(0.f, Y, H * 0.5f), FRotator::ZeroRotator, FVector(0.1f, 0.64f, H / 100.f), Col);
			A->AddPart(ETSCShape::Cube, FVector(0.f, Y, H), FRotator(0.f, 0.f, 45.f), FVector(0.1f, 0.45f, 0.45f), Col * 0.8f);
		}
		// The Ashen Saga rebuilt its wall on a stone footing: stronger fortifications.
		const int32 Base = A->AddPart(ETSCShape::Cube, FVector(-6.f, CELL * 0.5f, 45.f), FRotator::ZeroRotator, FVector(0.3f, 2.f, 0.9f), Pal::Stone);
		A->SetAshen(Base, Pal::StoneDark * 1.3f);
		A->SetVis(Base, ETSCVis::AshenOnly);
	}

	void BuildGate(ATSCPrimActor* A, TArray<USceneComponent*>& Pivots)
	{
		// Posts, lintel, two door leaves hinged on the posts.
		Post(A, 0.f, 0.f, 0.f, 330.f, Pal::TimberDark, 0.3f);
		Post(A, 0.f, 2.f * CELL, 0.f, 330.f, Pal::TimberDark, 0.3f);
		A->AddPart(ETSCShape::Cube, FVector(0.f, CELL, 318.f), FRotator::ZeroRotator, FVector(0.24f, 4.4f, 0.26f), Pal::TimberDark);
		for (int32 I = 0; I < 2; ++I)
		{
			USceneComponent* Pivot = A->AddPivot(FVector(0.f, I == 0 ? 0.f : 2.f * CELL, 0.f));
			const float Dir = I == 0 ? 1.f : -1.f;
			A->AddPart(ETSCShape::Cube, FVector(0.f, Dir * CELL * 0.5f, 130.f), FRotator::ZeroRotator, FVector(0.1f, 1.94f, 2.5f), Pal::Timber * 1.15f, Pivot);
			A->AddPart(ETSCShape::Cube, FVector(-7.f, Dir * CELL * 0.5f, 70.f), FRotator::ZeroRotator, FVector(0.06f, 1.94f, 0.16f), Pal::Iron, Pivot);
			A->AddPart(ETSCShape::Cube, FVector(-7.f, Dir * CELL * 0.5f, 190.f), FRotator::ZeroRotator, FVector(0.06f, 1.94f, 0.16f), Pal::Iron, Pivot);
			Pivots.Add(Pivot);
			// Torch on each post.
			const int32 Flame = A->AddPart(ETSCShape::Sphere, FVector(30.f, I * 2.f * CELL, 350.f), FRotator::ZeroRotator, FVector(0.26f, 0.26f, 0.36f), Pal::Fire);
			A->SetGlow(Flame, FLinearColor(16.f, 6.f, 0.8f), FLinearColor(10.f, 3.f, 0.3f));
			A->AddLight(FVector(60.f, I * 2.f * CELL, 340.f), Pal::Fire, 5.f, 4.f, 800.f);
		}
	}

	// ----- scenery and props (3D) --------------------------------------------------------------------------------------

	void BuildTree(ATSCPrimActor* A, int32 Seed, bool bWithStump)
	{
		FRandomStream R(Seed * 7919 + 13);
		const FLinearColor Green = Pal::Spruce * R.FRandRange(0.75f, 1.35f);
		const int32 Trunk = A->AddPart(ETSCShape::Cylinder, FVector(0.f, 0.f, 70.f), FRotator::ZeroRotator, FVector(0.26f, 0.26f, 1.4f), Pal::TimberDark * 1.2f);
		A->SetVis(Trunk, ETSCVis::LivingOnly);
		const float Z[3] = { 170.f, 262.f, 340.f };
		const float S[3] = { 1.55f, 1.2f, 0.82f };
		for (int32 I = 0; I < 3; ++I)
		{
			const int32 C = A->AddPart(ETSCShape::Cone, FVector(0.f, 0.f, Z[I]), FRotator::ZeroRotator, FVector(S[I], S[I], 1.5f - I * 0.15f), Green * (1.f + I * 0.18f));
			A->SetAshen(C, Pal::Char);
			A->SetVis(C, ETSCVis::LivingOnly);
		}
		if (bWithStump)
		{
			BuildStump(A);
		}
		const float Sc = R.FRandRange(0.85f, 1.3f);
		A->SetActorScale3D(FVector(Sc, Sc, Sc * R.FRandRange(0.9f, 1.15f)));
		A->SetActorRotation(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f));
	}

	void BuildStump(ATSCPrimActor* A)
	{
		const int32 Stump = A->AddPart(ETSCShape::Cylinder, FVector(0.f, 0.f, 22.f), FRotator::ZeroRotator, FVector(0.3f, 0.3f, 0.44f), Pal::Char);
		A->SetAshen(Stump, Pal::Char);
		A->SetVis(Stump, ETSCVis::AshenOnly);
		const int32 Ember = A->AddPart(ETSCShape::Cylinder, FVector(0.f, 0.f, 44.f), FRotator::ZeroRotator, FVector(0.2f, 0.2f, 0.02f), Pal::Char);
		A->SetGlow(Ember, FLinearColor::Black, FLinearColor(1.6f, 0.35f, 0.03f));
		A->SetVis(Ember, ETSCVis::AshenOnly);
	}

	void BuildRock(ATSCPrimActor* A, int32 Seed)
	{
		FRandomStream R(Seed * 331 + 5);
		const int32 N = 2 + Seed % 2;
		for (int32 I = 0; I < N; ++I)
		{
			const float S = R.FRandRange(0.5f, 1.1f);
			A->AddPart(I % 2 ? ETSCShape::Cube : ETSCShape::Sphere, FVector(R.FRandRange(-50.f, 50.f), R.FRandRange(-50.f, 50.f), S * 22.f),
				FRotator(R.FRandRange(-20.f, 20.f), R.FRandRange(0.f, 90.f), R.FRandRange(-20.f, 20.f)), FVector(S, S * R.FRandRange(0.7f, 1.2f), S * 0.7f),
				Pal::Stone * R.FRandRange(0.6f, 1.1f));
		}
	}

	void BuildMushrooms(ATSCPrimActor* A, int32 Seed)
	{
		FRandomStream R(Seed * 97 + 3);
		const int32 N = 3 + Seed % 3;
		for (int32 I = 0; I < N; ++I)
		{
			const FVector P(R.FRandRange(-70.f, 70.f), R.FRandRange(-70.f, 70.f), 0.f);
			const float S = R.FRandRange(0.6f, 1.2f);
			const int32 Stem = A->AddPart(ETSCShape::Cylinder, P + FVector(0.f, 0.f, 14.f * S), FRotator::ZeroRotator, FVector(0.08f * S, 0.08f * S, 0.28f * S), FLinearColor(0.6f, 0.65f, 0.55f));
			A->SetAshen(Stem, Pal::Char);
			const int32 Cap = A->AddPart(ETSCShape::Sphere, P + FVector(0.f, 0.f, 30.f * S), FRotator::ZeroRotator, FVector(0.3f * S, 0.3f * S, 0.16f * S), Pal::Shroom);
			A->SetAshen(Cap, Pal::Char * 1.5f);
			A->SetGlow(Cap, FLinearColor(1.2f, 4.5f, 2.4f), FLinearColor::Black); // mushrooms go dark in the Ashen Saga
		}
	}

	void BuildRuneStone(ATSCPrimActor* A, bool bHammer)
	{
		const FLinearColor Glow = bHammer ? FLinearColor(10.f, 4.f, 0.5f) : FLinearColor(1.5f, 8.f, 3.5f);
		const FLinearColor Dim = Glow * 0.25f;
		A->AddPart(ETSCShape::Cube, FVector(0.f, 0.f, 12.f), FRotator::ZeroRotator, FVector(1.9f, 1.5f, 0.24f), Pal::StoneDark);
		A->AddPart(ETSCShape::Cube, FVector(0.f, 0.f, 150.f), FRotator(0.f, 0.f, 4.f), FVector(1.05f, 0.3f, 2.7f), Pal::Stone);
		A->AddPart(ETSCShape::Cube, FVector(0.f, 0.f, 290.f), FRotator(0.f, 0.f, 4.f), FVector(0.8f, 0.3f, 0.5f), Pal::Stone * 0.9f);
		const float Y = 20.f;
		if (bHammer)
		{
			int32 S = A->AddPart(ETSCShape::Cube, FVector(0.f, Y, 150.f), FRotator::ZeroRotator, FVector(0.14f, 0.05f, 1.2f), Pal::Fire);
			A->SetGlow(S, Glow, Dim);
			S = A->AddPart(ETSCShape::Cube, FVector(0.f, Y + 4.f, 216.f), FRotator::ZeroRotator, FVector(0.62f, 0.05f, 0.3f), Pal::Fire);
			A->SetGlow(S, Glow, Dim);
			// Banners and candles.
			for (int32 I = 0; I < 2; ++I)
			{
				const float X = I == 0 ? -150.f : 150.f;
				Post(A, X, 0.f, 0.f, 340.f, Pal::TimberDark, 0.08f);
				const int32 B = A->AddPart(ETSCShape::Cube, FVector(X, 8.f, 250.f), FRotator::ZeroRotator, FVector(0.5f, 0.03f, 1.6f), Pal::Red);
				A->SetVis(B, I == 0 ? ETSCVis::Both : ETSCVis::LivingOnly);
			}
			for (int32 I = 0; I < 5; ++I)
			{
				const FVector P(-90.f + I * 45.f, 100.f + (I % 2) * 26.f, 0.f);
				A->AddPart(ETSCShape::Cylinder, P + FVector(0.f, 0.f, 12.f + I % 3 * 4.f), FRotator::ZeroRotator, FVector(0.07f, 0.07f, 0.24f + I % 3 * 0.08f), FLinearColor(0.7f, 0.65f, 0.5f));
				const int32 F = A->AddPart(ETSCShape::Sphere, P + FVector(0.f, 0.f, 30.f + I % 3 * 8.f), FRotator::ZeroRotator, FVector(0.07f, 0.07f, 0.11f), Pal::Fire);
				A->SetGlow(F, FLinearColor(14.f, 6.f, 1.f), FLinearColor(3.f, 1.f, 0.1f));
			}
			A->AddLight(FVector(0.f, 140.f, 80.f), Pal::Fire, 4.f, 1.2f, 700.f);
		}
		else
		{
			// Stag sign: a spine and two antlers.
			int32 S = A->AddPart(ETSCShape::Cube, FVector(0.f, Y, 140.f), FRotator::ZeroRotator, FVector(0.1f, 0.05f, 0.8f), Pal::Shroom);
			A->SetGlow(S, Glow, Dim);
			S = A->AddPart(ETSCShape::Cube, FVector(-28.f, Y + 4.f, 205.f), FRotator(-35.f, 0.f, 0.f), FVector(0.09f, 0.05f, 0.75f), Pal::Shroom);
			A->SetGlow(S, Glow, Dim);
			S = A->AddPart(ETSCShape::Cube, FVector(28.f, Y + 4.f, 205.f), FRotator(35.f, 0.f, 0.f), FVector(0.09f, 0.05f, 0.75f), Pal::Shroom);
			A->SetGlow(S, Glow, Dim);
			// Roots wrapped around the stone.
			for (int32 I = 0; I < 6; ++I)
			{
				const float Ang = I * 60.f + 15.f;
				const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
				const int32 Rt = A->AddPart(ETSCShape::Cylinder, Dir * 62.f + FVector(0.f, 0.f, 70.f), FRotator(I % 2 ? 28.f : -24.f, Ang, I % 2 ? -22.f : 30.f),
					FVector(0.14f, 0.14f, 1.7f), Pal::TimberDark * 1.4f);
				A->SetAshen(Rt, Pal::Char);
			}
			A->AddLight(FVector(0.f, 120.f, 100.f), Pal::Shroom, 3.5f, 0.6f, 700.f);
		}
	}

	void BuildLongship(ATSCPrimActor* A)
	{
		// Hull along X, bow to the east.
		const int32 Hull = A->AddPart(ETSCShape::Cube, FVector(0.f, 0.f, 26.f), FRotator::ZeroRotator, FVector(5.2f, 1.1f, 0.5f), Pal::Timber * 1.1f);
		A->SetAshen(Hull, Pal::Char);
		A->AddPart(ETSCShape::Cube, FVector(0.f, 0.f, 50.f), FRotator::ZeroRotator, FVector(5.0f, 0.9f, 0.06f), Pal::Plank);
		for (int32 End = 0; End < 2; ++End)
		{
			const float Dir = End == 0 ? -1.f : 1.f;
			const int32 Prow = A->AddPart(ETSCShape::Cone, FVector(Dir * 300.f, 0.f, 70.f), FRotator(Dir * -55.f, 0.f, 0.f), FVector(0.8f, 0.6f, 1.9f), Pal::Timber);
			A->SetAshen(Prow, Pal::Char);
		}
		const int32 Mast = A->AddPart(ETSCShape::Cylinder, FVector(0.f, 0.f, 230.f), FRotator::ZeroRotator, FVector(0.14f, 0.14f, 3.7f), Pal::TimberDark);
		A->SetVis(Mast, ETSCVis::LivingOnly);
		for (int32 I = 0; I < 5; ++I)
		{
			const int32 Sail = A->AddPart(ETSCShape::Cube, FVector(-120.f + I * 60.f, 0.f, 270.f), FRotator::ZeroRotator, FVector(0.6f, 0.04f, 2.2f),
				I % 2 ? FLinearColor(0.6f, 0.56f, 0.48f) : Pal::Red);
			A->SetVis(Sail, ETSCVis::LivingOnly);
		}
		for (int32 I = 0; I < 6; ++I)
		{
			const int32 Sh = A->AddPart(ETSCShape::Cylinder, FVector(-200.f + I * 80.f, 58.f, 50.f), FRotator(0.f, 0.f, 90.f), FVector(0.36f, 0.36f, 0.05f),
				I % 2 ? Pal::Red : FLinearColor(0.5f, 0.38f, 0.06f));
			A->SetVis(Sh, I % 3 == 0 ? ETSCVis::Both : ETSCVis::LivingOnly);
			A->SetAshen(Sh, Pal::Char);
		}
	}

	void BuildRaiderBoat(ATSCPrimActor* A)
	{
		A->AddPart(ETSCShape::Cube, FVector(0.f, 0.f, 22.f), FRotator::ZeroRotator, FVector(3.4f, 0.9f, 0.44f), Pal::Char * 2.f);
		A->AddPart(ETSCShape::Cone, FVector(-200.f, 0.f, 60.f), FRotator(55.f, 0.f, 0.f), FVector(0.7f, 0.5f, 1.5f), Pal::Char * 2.f);
		A->AddPart(ETSCShape::Cylinder, FVector(0.f, 0.f, 170.f), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 2.7f), Pal::TimberDark);
		A->AddPart(ETSCShape::Cube, FVector(0.f, 0.f, 210.f), FRotator::ZeroRotator, FVector(2.2f, 0.04f, 1.6f), FLinearColor(0.3f, 0.03f, 0.03f));
	}

	void BuildPlotMarker(ATSCPrimActor* A, int32 W, int32 H)
	{
		const FLinearColor Col(0.9f, 0.8f, 0.4f);
		const FLinearColor Glow(3.f, 2.4f, 0.8f);
		auto Dash = [&](const FVector& P, bool bAlongX)
		{
			const int32 D = A->AddPart(ETSCShape::Cube, P, FRotator::ZeroRotator, bAlongX ? FVector(0.5f, 0.07f, 0.03f) : FVector(0.07f, 0.5f, 0.03f), Col);
			A->SetGlow(D, Glow, Glow);
			A->SetAshen(D, Col);
		};
		for (float X = 50.f; X < W * CELL; X += 100.f) { Dash(FVector(X, 0.f, 6.f), true); Dash(FVector(X, H * CELL, 6.f), true); }
		for (float Y = 50.f; Y < H * CELL; Y += 100.f) { Dash(FVector(0.f, Y, 6.f), false); Dash(FVector(W * CELL, Y, 6.f), false); }
		// Hammer icon floating over the plot.
		const FVector C(W * CELL * 0.5f, H * CELL * 0.5f, 150.f);
		int32 I = A->AddPart(ETSCShape::Cube, C, FRotator(0.f, 45.f, 35.f), FVector(0.1f, 0.1f, 1.3f), Pal::Plank * 1.5f);
		A->SetGlow(I, FLinearColor(0.5f, 0.3f, 0.1f), FLinearColor(0.5f, 0.3f, 0.1f));
		const FVector Top = C + FRotator(0.f, 45.f, 35.f).RotateVector(FVector(0.f, 0.f, 60.f));
		I = A->AddPart(ETSCShape::Cube, Top, FRotator(0.f, 45.f, 35.f), FVector(0.3f, 0.6f, 0.3f), Pal::Iron * 2.f);
		A->SetGlow(I, Glow * 0.6f, Glow * 0.6f);
	}

	void BuildLogPile(ATSCPrimActor* A)
	{
		int32 N = 0;
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 I = 0; I < 3 - Row; ++I)
			{
				const int32 L = A->AddPart(ETSCShape::Cylinder, FVector(0.f, -44.f + I * 44.f + Row * 22.f, 22.f + Row * 38.f), FRotator(90.f, 0.f, 0.f),
					FVector(0.42f, 0.42f, 1.7f), Pal::Timber * (1.1f + 0.15f * (N % 3)));
				A->SetAshen(L, Pal::Char);
				if (Row > 0) { A->SetVis(L, ETSCVis::LivingOnly); }
				++N;
			}
		}
	}

	void BuildTorch(ATSCPrimActor* A)
	{
		Post(A, 0.f, 0.f, 0.f, 150.f, Pal::TimberDark, 0.08f);
		const int32 F = A->AddPart(ETSCShape::Sphere, FVector(0.f, 0.f, 162.f), FRotator::ZeroRotator, FVector(0.2f, 0.2f, 0.3f), Pal::Fire);
		A->SetGlow(F, FLinearColor(16.f, 6.f, 0.8f), FLinearColor(10.f, 3.f, 0.3f));
		A->AddLight(FVector(0.f, 0.f, 175.f), Pal::Fire, 3.5f, 3.f, 650.f);
	}
}
