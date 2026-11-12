// The Saga Cutter - game jam project.

#include "TSCHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "TSCGameMode.h"
#include "TSCUnit.h"

namespace
{
	const FLinearColor WoodDark(0.02f, 0.012f, 0.007f, 0.96f);
	const FLinearColor WoodMid(0.05f, 0.03f, 0.016f, 1.f);
	const FLinearColor WoodLight(0.15f, 0.09f, 0.045f, 1.f);
	const FLinearColor IronCol(0.30f, 0.31f, 0.34f, 1.f);
	const FLinearColor Gold(0.95f, 0.72f, 0.28f, 1.f);
	const FLinearColor Parchment(0.92f, 0.86f, 0.72f, 1.f);
	const FLinearColor Dim(0.55f, 0.5f, 0.42f, 1.f);
	const FLinearColor CutRed(0.75f, 0.10f, 0.06f, 1.f);
	const FLinearColor LivingGreen(0.25f, 0.62f, 0.32f, 1.f);
	const FLinearColor AshGrey(0.62f, 0.56f, 0.5f, 1.f);

	FSlateFontInfo MakeFont(float Px, bool bBold)
	{
		// Canvas text needs a UFont behind the Slate font info; the engine's Roboto has Regular and Bold faces.
		static TWeakObjectPtr<UFont> Roboto;
		if (!Roboto.IsValid()) { Roboto = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")); }
		return FSlateFontInfo(Roboto.Get(), FMath::Max(6, FMath::RoundToInt(Px)), bBold ? FName("Bold") : FName("Regular"));
	}

	enum { IdStart = 1, IdRestart, IdCut, IdTakeLiving, IdTakeAshen, IdConfirm, IdSpeed, IdWoodMinus = 10, IdWoodPlus, IdFoodMinus, IdFoodPlus, IdBuild = 20 };
}

// ----- primitives ---------------------------------------------------------------------------------------------------

void ATSCHUD::Box(float X, float Y, float W, float H, const FLinearColor& C)
{
	DrawRect(C, X * U, Y * U, W * U, H * U);
}

void ATSCHUD::Frame(float X, float Y, float W, float H, const FLinearColor& C, float Th)
{
	Box(X, Y, W, Th, C);
	Box(X, Y + H - Th, W, Th, C);
	Box(X, Y, Th, H, C);
	Box(X + W - Th, Y, Th, H, C);
}

void ATSCHUD::Seg(float X1, float Y1, float X2, float Y2, const FLinearColor& C, float Th)
{
	DrawLine(X1 * U, Y1 * U, X2 * U, Y2 * U, C, Th * U);
}

FVector2D ATSCHUD::Measure(const FString& S, float Size, bool bBold) const
{
	if (!FSlateApplication::IsInitialized()) { return FVector2D(S.Len() * Size * 0.55f, Size * 1.3f); }
	const FSlateFontInfo Font = MakeFont(Size * U * 0.75f, bBold);
	const FVector2D Px = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Font);
	return Px / U;
}

void ATSCHUD::Txt(const FString& S, float X, float Y, float Size, const FLinearColor& C, int32 Align, bool bBold)
{
	if (!Canvas || S.IsEmpty()) { return; }
	const FSlateFontInfo Font = MakeFont(Size * U * 0.75f, bBold);
	float Px = X;
	if (Align != 0)
	{
		const FVector2D M = Measure(S, Size, bBold);
		Px = Align == 1 ? X - M.X * 0.5f : X - M.X;
	}
	FCanvasTextItem Shadow(FVector2D((Px + 1.5f) * U, (Y + 1.5f) * U), FText::FromString(S), Font, FLinearColor(0.f, 0.f, 0.f, C.A * 0.8f));
	Canvas->DrawItem(Shadow);
	FCanvasTextItem Item(FVector2D(Px * U, Y * U), FText::FromString(S), Font, C);
	Canvas->DrawItem(Item);
}

float ATSCHUD::Wrapped(const FString& S, float X, float Y, float MaxW, float Size, const FLinearColor& C, bool bBold, int32 Align)
{
	TArray<FString> Words;
	S.ParseIntoArray(Words, TEXT(" "), true);
	FString Line;
	float Cy = Y;
	const float LineH = Size * 1.38f;
	for (const FString& W : Words)
	{
		const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
		if (Measure(Try, Size, bBold).X > MaxW && !Line.IsEmpty())
		{
			Txt(Line, X, Cy, Size, C, Align, bBold);
			Cy += LineH;
			Line = W;
		}
		else
		{
			Line = Try;
		}
	}
	if (!Line.IsEmpty())
	{
		Txt(Line, X, Cy, Size, C, Align, bBold);
		Cy += LineH;
	}
	return Cy;
}

void ATSCHUD::Knot(float Cx, float Cy, float R, const FLinearColor& C)
{
	// A small interlaced knot: three overlapping diamonds.
	for (int32 I = -1; I <= 1; ++I)
	{
		const float X = Cx + I * R * 0.8f;
		Seg(X - R, Cy, X, Cy - R, C, 1.6f);
		Seg(X, Cy - R, X + R, Cy, C, 1.6f);
		Seg(X + R, Cy, X, Cy + R, C, 1.6f);
		Seg(X, Cy + R, X - R, Cy, C, 1.6f);
	}
}

void ATSCHUD::WoodPanel(float X, float Y, float W, float H)
{
	// Dark carved wood, a lighter bevel, plank grain, iron corner brackets.
	Box(X, Y, W, H, WoodDark);
	Box(X + 3.f, Y + 3.f, W - 6.f, H - 6.f, FLinearColor(WoodMid.R, WoodMid.G, WoodMid.B, 0.92f));
	for (float Gy = Y + 14.f; Gy < Y + H - 8.f; Gy += 17.f)
	{
		Box(X + 8.f, Gy, W - 16.f, 1.f, FLinearColor(0.f, 0.f, 0.f, 0.22f));
	}
	Frame(X, Y, W, H, WoodLight, 2.f);
	Frame(X + 5.f, Y + 5.f, W - 10.f, H - 10.f, FLinearColor(0.f, 0.f, 0.f, 0.5f), 1.f);
	const float B = FMath::Min(18.f, H * 0.35f);
	for (int32 Cx = 0; Cx < 2; ++Cx)
	{
		for (int32 Cy = 0; Cy < 2; ++Cy)
		{
			const float Bx = Cx == 0 ? X - 2.f : X + W - B + 2.f;
			const float By = Cy == 0 ? Y - 2.f : Y + H - B + 2.f;
			Box(Bx, Cy == 0 ? By : By + B - 5.f, B, 5.f, IronCol);
			Box(Cx == 0 ? Bx : Bx + B - 5.f, By, 5.f, B, IronCol);
			Box(Cx == 0 ? Bx + 1.f : Bx + B - 4.f, Cy == 0 ? By + 1.f : By + B - 4.f, 3.f, 3.f, FLinearColor(0.6f, 0.6f, 0.62f, 1.f)); // rivet
		}
	}
}

bool ATSCHUD::Hover(float X, float Y, float W, float H) const
{
	return Mouse.X >= X * U && Mouse.X <= (X + W) * U && Mouse.Y >= Y * U && Mouse.Y <= (Y + H) * U;
}

bool ATSCHUD::Button(int32 Id, float X, float Y, float W, float H, const FString& Label, bool bEnabled, float Size, const FLinearColor* Tint)
{
	const bool bHover = bEnabled && Hover(X, Y, W, H);
	FLinearColor Fill = Tint ? *Tint : WoodMid;
	if (!bEnabled) { Fill = FLinearColor(Fill.R * 0.4f, Fill.G * 0.4f, Fill.B * 0.4f, 0.85f); }
	else if (bHover) { Fill = FLinearColor(Fill.R * 1.5f + 0.04f, Fill.G * 1.5f + 0.03f, Fill.B * 1.5f + 0.02f, 1.f); }
	Box(X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, 0.9f));
	Box(X + 3.f, Y + 3.f, W - 6.f, H - 6.f, Fill);
	Frame(X, Y, W, H, bHover ? Gold : IronCol, 3.f);
	if (!Label.IsEmpty())
	{
		Txt(Label, X + W * 0.5f, Y + H * 0.5f - Size * 0.62f, Size, bEnabled ? Parchment : Dim, 1);
	}
	if (bEnabled)
	{
		FHit Hit;
		Hit.Box = FBox2D(FVector2D(X * U, Y * U), FVector2D((X + W) * U, (Y + H) * U));
		Hit.Id = Id;
		Hits.Add(Hit);
	}
	return bHover;
}

// ----- input --------------------------------------------------------------------------------------------------------

bool ATSCHUD::HandleClick(const FVector2D& Pos)
{
	for (int32 I = Hits.Num() - 1; I >= 0; --I)
	{
		if (Hits[I].Box.IsInside(Pos))
		{
			Act(Hits[I].Id);
			return true;
		}
	}
	return false;
}

void ATSCHUD::HandleCutKey()
{
	ATSCGameMode* GM = GetWorld()->GetAuthGameMode<ATSCGameMode>();
	if (!GM) { return; }
	if (GM->Phase == ETSCPhase::Title) { GM->StartGame(); }
	else if (GM->bCutOpen) { GM->ConfirmCut(); }
	else { GM->PressCut(); }
}

void ATSCHUD::Act(int32 Id)
{
	ATSCGameMode* GM = GetWorld()->GetAuthGameMode<ATSCGameMode>();
	if (!GM) { return; }
	switch (Id)
	{
	case IdStart: GM->StartGame(); break;
	case IdRestart: GM->RestartGame(); break;
	case IdCut: GM->PressCut(); break;
	case IdTakeLiving: GM->SelectTake(0); break;
	case IdTakeAshen: GM->SelectTake(1); break;
	case IdConfirm: GM->ConfirmCut(); break;
	case IdSpeed: GM->ToggleSpeed(); break;
	case IdWoodMinus: GM->AdjustJob(ETSCJob::Wood, -1); break;
	case IdWoodPlus: GM->AdjustJob(ETSCJob::Wood, 1); break;
	case IdFoodMinus: GM->AdjustJob(ETSCJob::Food, -1); break;
	case IdFoodPlus: GM->AdjustJob(ETSCJob::Food, 1); break;
	default:
		if (Id >= IdBuild && Id < IdBuild + 5) { GM->StartBuild((TSC::EBuilding)(Id - IdBuild)); }
		break;
	}
}

// ----- frame --------------------------------------------------------------------------------------------------------

void ATSCHUD::DrawHUD()
{
	Super::DrawHUD();
	ATSCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATSCGameMode>() : nullptr;
	if (!GM || !Canvas) { return; }

	U = FMath::Max(0.4f, Canvas->ClipY / 1080.f);
	SW = Canvas->ClipX / U;
	SH = Canvas->ClipY / U;
	Hits.Reset();
	Mouse = FVector2D(-1.f, -1.f);
	if (APlayerController* PC = GetOwningPlayerController())
	{
		float Mx, My;
		if (PC->GetMousePosition(Mx, My)) { Mouse = FVector2D(Mx, My); }
	}

	if (GM->Phase == ETSCPhase::Title)
	{
		DrawTitle(GM);
		return;
	}
	DrawWorld(GM);
	if (GM->Phase == ETSCPhase::Epilogue)
	{
		DrawEpilogue(GM);
		return;
	}
	DrawTop(GM);
	if (GM->Phase == ETSCPhase::Day)
	{
		DrawJobs(GM);
		DrawBuildBar(GM);
	}
	DrawToasts(GM);
	DrawBanner(GM);

	// letterbox + title while the world changes saga
	if (GM->TransitionTimer > 0.f && !GM->bCutOpen)
	{
		const float A = FMath::Clamp(FMath::Min(GM->TransitionTimer, 3.4f - GM->TransitionTimer) * 2.f, 0.f, 1.f);
		Box(0.f, 0.f, SW, 90.f * A, FLinearColor(0.f, 0.f, 0.f, 0.92f));
		Box(0.f, SH - 90.f * A, SW, 90.f * A, FLinearColor(0.f, 0.f, 0.f, 0.92f));
		Txt(GM->TransitionTitle, SW * 0.5f, SH - 200.f, 40.f, FLinearColor(1.f, 0.82f, 0.6f, A), 1);
		Txt(TEXT("the scene is recut"), SW * 0.5f, SH - 146.f, 22.f, FLinearColor(0.85f, 0.8f, 0.7f, A), 1, false);
	}
	if (GM->GetFlash() > 0.f)
	{
		Box(0.f, 0.f, SW, SH, FLinearColor(1.f, 0.9f, 0.75f, GM->GetFlash() * 1.4f));
	}
	if (GM->bCutOpen)
	{
		DrawCut(GM);
	}
}

// ----- world-space overlays -----------------------------------------------------------------------------------------

void ATSCHUD::DrawWorld(ATSCGameMode* GM)
{
	for (const FTSCTracer& T : GM->Tracers)
	{
		const FVector A = Canvas->Project(T.From);
		const FVector B = Canvas->Project(T.To);
		if (A.Z > 0.f && B.Z > 0.f)
		{
			DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(T.Color.R, T.Color.G, T.Color.B, FMath::Clamp(T.Time * 5.f, 0.f, 1.f)), 2.f * U);
		}
	}
	if (GM->Phase == ETSCPhase::Epilogue) { return; }

	const bool bNight = GM->Phase == ETSCPhase::Night;
	for (const ATSCUnit* Unit : GM->Units)
	{
		if (!Unit || !Unit->IsTargetable() || Unit->IsHidden()) { continue; }
		const FVector P = Canvas->Project(Unit->GetActorLocation() + FVector(0.f, 0.f, Unit->Height + 28.f));
		if (P.Z <= 0.f) { continue; }
		const float X = P.X / U, Y = P.Y / U;
		const bool bEnemy = Unit->Team == ETSCTeam::Creature || Unit->Team == ETSCTeam::Raider;
		float Top = Y;

		// health bar: green for yours, red for enemies
		const bool bBar = bEnemy || Unit->BarTimer > 0.f || (bNight && Unit->bDefender);
		if (bBar && Unit->Team != ETSCTeam::Animal)
		{
			const float W = Unit->bNamed ? 54.f : 40.f;
			const float Frac = FMath::Clamp(Unit->HP / FMath::Max(1.f, Unit->MaxHP), 0.f, 1.f);
			Box(X - W * 0.5f - 1.5f, Y - 8.5f, W + 3.f, 9.f, FLinearColor(0.f, 0.f, 0.f, 0.85f));
			Box(X - W * 0.5f, Y - 7.f, W * Frac, 6.f, bEnemy ? FLinearColor(0.9f, 0.12f, 0.08f, 1.f) : FLinearColor(0.2f, 0.85f, 0.25f, 1.f));
			Top = Y - 12.f;
		}
		if (Unit->bNamed)
		{
			FString First;
			Unit->DisplayName.Split(TEXT(" "), &First, nullptr);
			Txt(First.IsEmpty() ? Unit->DisplayName : First, X, Top - 22.f, 15.f, Gold, 1);
			Top -= 22.f;
		}
		// need bubble
		if (Unit->Need != ETSCNeed::None && !bNight)
		{
			const float Bx = X + 14.f, By = Top - 30.f;
			Box(Bx - 13.f, By - 13.f, 26.f, 24.f, FLinearColor(0.f, 0.f, 0.f, 0.75f));
			Box(Bx - 11.f, By - 11.f, 22.f, 20.f, FLinearColor(0.93f, 0.9f, 0.82f, 0.95f));
			Box(Bx - 9.f, By + 9.f, 6.f, 5.f, FLinearColor(0.93f, 0.9f, 0.82f, 0.95f));
			if (Unit->Need == ETSCNeed::Wood)
			{
				Box(Bx - 8.f, By - 5.f, 16.f, 8.f, FLinearColor(0.42f, 0.24f, 0.1f, 1.f));
				Box(Bx + 5.f, By - 5.f, 3.f, 8.f, FLinearColor(0.8f, 0.62f, 0.36f, 1.f));
			}
			else if (Unit->Need == ETSCNeed::Leaf)
			{
				Box(Bx - 5.f, By - 7.f, 10.f, 10.f, FLinearColor(0.2f, 0.6f, 0.15f, 1.f));
				Box(Bx - 1.f, By - 9.f, 2.f, 16.f, FLinearColor(0.1f, 0.35f, 0.08f, 1.f));
			}
			else
			{
				Box(Bx - 2.5f, By - 8.f, 5.f, 15.f, FLinearColor(0.1f, 0.7f, 0.2f, 1.f));
				Box(Bx - 7.5f, By - 3.f, 15.f, 5.f, FLinearColor(0.1f, 0.7f, 0.2f, 1.f));
			}
		}
	}

	// construction progress over plots
	for (const FTSCPlot& Plot : GM->Plots)
	{
		if (Plot.bUsed && Plot.Progress < 1.f && Plot.Building)
		{
			const FVector P = Canvas->Project(Plot.Building->GetActorLocation() + FVector(250.f, 200.f, 420.f));
			if (P.Z > 0.f)
			{
				const float X = P.X / U, Y = P.Y / U;
				Box(X - 52.f, Y - 2.f, 104.f, 14.f, FLinearColor(0.f, 0.f, 0.f, 0.85f));
				Box(X - 50.f, Y, 100.f * Plot.Progress, 10.f, Gold);
				Txt(TEXT("BUILDING"), X, Y - 22.f, 14.f, Parchment, 1);
			}
		}
	}
}

// ----- top bar ------------------------------------------------------------------------------------------------------

void ATSCHUD::DrawTop(ATSCGameMode* GM)
{
	// resources, top-left
	WoodPanel(18.f, 14.f, 570.f, 66.f);
	Knot(556.f, 47.f, 9.f, FLinearColor(Gold.R, Gold.G, Gold.B, 0.5f));
	float X = 40.f;
	const float Y = 30.f;
	// wood
	Box(X, Y + 8.f, 34.f, 18.f, FLinearColor(0.42f, 0.24f, 0.1f, 1.f));
	Box(X + 28.f, Y + 8.f, 6.f, 18.f, FLinearColor(0.8f, 0.62f, 0.36f, 1.f));
	Box(X + 4.f, Y + 14.f, 20.f, 2.f, FLinearColor(0.25f, 0.13f, 0.05f, 1.f));
	Txt(FString::Printf(TEXT("%d"), FMath::FloorToInt(GM->Wood)), X + 44.f, Y - 1.f, 28.f, Parchment);
	Txt(FString::Printf(TEXT("%+d"), GM->WoodRate), X + 44.f + Measure(FString::Printf(TEXT("%d"), FMath::FloorToInt(GM->Wood)), 28.f).X + 8.f, Y + 7.f, 19.f,
		GM->WoodRate >= 0 ? FLinearColor(0.55f, 0.9f, 0.5f, 1.f) : FLinearColor(1.f, 0.4f, 0.3f, 1.f));
	X += 170.f;
	// food
	Box(X + 6.f, Y + 4.f, 22.f, 22.f, FLinearColor(0.2f, 0.6f, 0.15f, 1.f));
	Box(X + 16.f, Y, 2.f, 30.f, FLinearColor(0.1f, 0.35f, 0.08f, 1.f));
	Box(X + 10.f, Y + 12.f, 14.f, 2.f, FLinearColor(0.1f, 0.35f, 0.08f, 1.f));
	Txt(FString::Printf(TEXT("%d"), FMath::FloorToInt(GM->Food)), X + 44.f, Y - 1.f, 28.f, Parchment);
	Txt(FString::Printf(TEXT("%+d"), GM->FoodRate), X + 44.f + Measure(FString::Printf(TEXT("%d"), FMath::FloorToInt(GM->Food)), 28.f).X + 8.f, Y + 7.f, 19.f,
		GM->FoodRate >= 0 ? FLinearColor(0.55f, 0.9f, 0.5f, 1.f) : FLinearColor(1.f, 0.4f, 0.3f, 1.f));
	X += 170.f;
	// population
	Box(X + 10.f, Y + 2.f, 12.f, 12.f, FLinearColor(0.85f, 0.65f, 0.5f, 1.f));
	Box(X + 5.f, Y + 15.f, 22.f, 14.f, FLinearColor(0.3f, 0.4f, 0.6f, 1.f));
	Txt(FString::Printf(TEXT("%d/%d"), GM->Pop, GM->PopCap), X + 40.f, Y - 1.f, 28.f, Parchment);

	// phase, top-centre
	const float Pw = 520.f, Px = SW * 0.5f - Pw * 0.5f;
	WoodPanel(Px, 14.f, Pw, 66.f);
	Txt(GM->PhaseLabel(), SW * 0.5f, 22.f, 21.f, GM->Phase == ETSCPhase::Night ? FLinearColor(1.f, 0.5f, 0.35f, 1.f) : Gold, 1);
	Box(Px + 24.f, 56.f, Pw - 48.f, 10.f, FLinearColor(0.f, 0.f, 0.f, 0.8f));
	Box(Px + 26.f, 58.f, (Pw - 52.f) * GM->PhaseFrac(), 6.f, GM->Phase == ETSCPhase::Night ? FLinearColor(0.5f, 0.6f, 1.f, 1.f) : Gold);

	if (GM->Phase == ETSCPhase::Night)
	{
		// the two things that must hold
		const float Bw = 250.f;
		WoodPanel(Px, 88.f, Pw, 50.f);
		Txt(TEXT("WEST GATE"), Px + 20.f, 92.f, 14.f, Parchment);
		Box(Px + 20.f, 114.f, Bw - 30.f, 12.f, FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Box(Px + 22.f, 116.f, (Bw - 34.f) * FMath::Clamp(GM->GateHP / GM->GateMaxHP, 0.f, 1.f), 8.f, GM->bGateBroken ? CutRed : FLinearColor(0.75f, 0.55f, 0.3f, 1.f));
		Txt(TEXT("LONGHOUSE HEARTH"), Px + Bw + 20.f, 92.f, 14.f, Parchment);
		Box(Px + Bw + 20.f, 114.f, Bw - 30.f, 12.f, FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Box(Px + Bw + 22.f, 116.f, (Bw - 34.f) * FMath::Clamp(GM->HallHP / GM->HallMaxHP, 0.f, 1.f), 8.f, FLinearColor(1.f, 0.5f, 0.15f, 1.f));
	}

	// edits + CUT!, top-right
	const float Rx = SW - 18.f - 430.f;
	WoodPanel(Rx, 14.f, 430.f, 96.f);
	Txt(TEXT("EDITS LEFT"), Rx + 22.f, 24.f, 15.f, Dim);
	for (int32 I = 0; I < GM->MaxEdits; ++I)
	{
		// film-frame pips
		const float Fx = Rx + 22.f + I * 44.f;
		const bool bHave = I < GM->EditsLeft;
		Box(Fx, 48.f, 36.f, 44.f, FLinearColor(0.f, 0.f, 0.f, 0.9f));
		Box(Fx + 5.f, 56.f, 26.f, 28.f, bHave ? Gold : FLinearColor(0.15f, 0.13f, 0.1f, 1.f));
		for (int32 H = 0; H < 4; ++H) { Box(Fx + 3.f + H * 8.5f, 50.f, 4.f, 3.f, Dim); Box(Fx + 3.f + H * 8.5f, 87.f, 4.f, 3.f, Dim); }
	}
	const bool bCan = GM->CanCut();
	const float Pulse = 0.75f + 0.25f * FMath::Sin(GM->GetWorldTime() * 8.f);
	const FLinearColor CutCol = bCan ? FLinearColor(CutRed.R * Pulse, CutRed.G * Pulse, CutRed.B * Pulse, 1.f) : FLinearColor(0.12f, 0.08f, 0.07f, 1.f);
	Button(IdCut, Rx + 190.f, 24.f, 150.f, 76.f, TEXT("CUT!"), bCan, 36.f, &CutCol);
	if (bCan) { Frame(Rx + 186.f, 20.f, 158.f, 84.f, FLinearColor(1.f, 0.8f, 0.4f, Pulse), 3.f); }
	Button(IdSpeed, Rx + 352.f, 24.f, 62.f, 76.f, FString::Printf(TEXT("x%d"), FMath::RoundToInt(GM->SpeedMul)), true, 22.f);
}

// ----- job panel ----------------------------------------------------------------------------------------------------

void ATSCHUD::DrawJobs(ATSCGameMode* GM)
{
	const float X = 18.f, Y = 96.f, W = 330.f;
	WoodPanel(X, Y, W, 196.f);
	Txt(TEXT("YOUR PEOPLE"), X + 20.f, Y + 12.f, 17.f, Gold);
	Knot(X + W - 40.f, Y + 24.f, 7.f, FLinearColor(Gold.R, Gold.G, Gold.B, 0.5f));
	const bool bIdle = GM->NumIdle > 0;

	float Ry = Y + 46.f;
	Txt(TEXT("Woodcutters"), X + 20.f, Ry + 6.f, 19.f, Parchment, 0, false);
	Button(IdWoodMinus, X + 176.f, Ry, 38.f, 38.f, TEXT("-"), GM->NumWood > 0, 24.f);
	Txt(FString::FromInt(GM->NumWood), X + 238.f, Ry + 2.f, 26.f, Parchment, 1);
	Button(IdWoodPlus, X + 262.f, Ry, 38.f, 38.f, TEXT("+"), bIdle, 24.f);

	Ry += 48.f;
	Txt(TEXT("Gatherers"), X + 20.f, Ry + 6.f, 19.f, Parchment, 0, false);
	Button(IdFoodMinus, X + 176.f, Ry, 38.f, 38.f, TEXT("-"), GM->NumFood > 0, 24.f);
	Txt(FString::FromInt(GM->NumFood), X + 238.f, Ry + 2.f, 26.f, Parchment, 1);
	Button(IdFoodPlus, X + 262.f, Ry, 38.f, 38.f, TEXT("+"), bIdle, 24.f);

	Ry += 50.f;
	Txt(TEXT("Builders / at prayer"), X + 20.f, Ry + 4.f, 19.f, Dim, 0, false);
	Txt(FString::FromInt(GM->NumIdle), X + 238.f, Ry, 26.f, Dim, 1);
}

// ----- build bar ----------------------------------------------------------------------------------------------------

void ATSCHUD::BuildIcon(int32 Type, float Cx, float Cy, float S, float Alpha)
{
	const FLinearColor Timber(0.55f, 0.36f, 0.18f, Alpha);
	const FLinearColor Roof(0.3f, 0.5f, 0.2f, Alpha);
	const FLinearColor Glow(1.f, 0.7f, 0.25f, Alpha);
	switch (Type)
	{
	case 0: // house
		Box(Cx - 20.f * S, Cy - 2.f * S, 40.f * S, 24.f * S, Timber);
		for (int32 I = 0; I < 5; ++I) { Box(Cx - (26.f - I * 5.f) * S, Cy - (6.f + I * 4.f) * S, (52.f - I * 10.f) * S, 4.f * S, Roof); }
		Box(Cx - 4.f * S, Cy + 8.f * S, 9.f * S, 14.f * S, Glow);
		break;
	case 1: // sawmill: log and blade
		Box(Cx - 26.f * S, Cy + 4.f * S, 52.f * S, 14.f * S, Timber);
		Box(Cx + 20.f * S, Cy + 4.f * S, 6.f * S, 14.f * S, FLinearColor(0.85f, 0.68f, 0.42f, Alpha));
		for (int32 I = 0; I < 5; ++I) { Box(Cx - (14.f - I * 6.f) * S, Cy - (18.f - (I % 2) * 6.f) * S, 6.f * S, (22.f - (I % 2) * 6.f) * S, FLinearColor(0.7f, 0.72f, 0.76f, Alpha)); }
		break;
	case 2: // healer: green cross
		Box(Cx - 7.f * S, Cy - 22.f * S, 14.f * S, 44.f * S, FLinearColor(0.2f, 0.8f, 0.3f, Alpha));
		Box(Cx - 22.f * S, Cy - 7.f * S, 44.f * S, 14.f * S, FLinearColor(0.2f, 0.8f, 0.3f, Alpha));
		break;
	case 3: // hammer
		Box(Cx - 4.f * S, Cy - 12.f * S, 8.f * S, 36.f * S, Timber);
		Box(Cx - 20.f * S, Cy - 24.f * S, 40.f * S, 16.f * S, Glow);
		break;
	default: // stag: antlers
	{
		const FLinearColor C(0.5f, 1.f, 0.7f, Alpha);
		Box(Cx - 3.f * S, Cy, 6.f * S, 22.f * S, C);
		Seg(Cx, Cy + 2.f * S, Cx - 20.f * S, Cy - 22.f * S, C, 4.f * S);
		Seg(Cx, Cy + 2.f * S, Cx + 20.f * S, Cy - 22.f * S, C, 4.f * S);
		Seg(Cx - 11.f * S, Cy - 11.f * S, Cx - 24.f * S, Cy - 10.f * S, C, 3.f * S);
		Seg(Cx + 11.f * S, Cy - 11.f * S, Cx + 24.f * S, Cy - 10.f * S, C, 3.f * S);
		break;
	}
	}
}

void ATSCHUD::DrawBuildBar(ATSCGameMode* GM)
{
	static const TCHAR* Names[5] = { TEXT("House"), TEXT("Sawmill"), TEXT("Healer's hut"), TEXT("Hammer shrine"), TEXT("Stag shrine") };
	static const TCHAR* Tips[5] = {
		TEXT("Raises the population cap by 3. Two settlers arrive: more workers by day, one more defender at night."),
		TEXT("+2 wood income. Wood left at nightfall braces the west gate."),
		TEXT("Wounded defenders are tended during the final night, even without Sigrun."),
		TEXT("Blessing for defenders: every axe strikes 30% harder on the final night."),
		TEXT("Blessing for forest and harvest: +2 food income, and fewer creatures come out of the fog.") };
	const float Bs = 92.f, Gap = 14.f;
	const float W = 5.f * Bs + 4.f * Gap + 48.f;
	const float X = SW * 0.5f - W * 0.5f, Y = SH - 150.f;
	WoodPanel(X, Y, W, 136.f);
	Knot(X + 14.f, Y + 68.f, 6.f, FLinearColor(Gold.R, Gold.G, Gold.B, 0.4f));
	Knot(X + W - 14.f, Y + 68.f, 6.f, FLinearColor(Gold.R, Gold.G, Gold.B, 0.4f));
	const bool bPlot = GM->FreePlot() >= 0;
	int32 HoverIdx = -1;
	for (int32 I = 0; I < 5; ++I)
	{
		const TSC::EBuilding Type = (TSC::EBuilding)I;
		const bool bCan = GM->CanBuild(Type);
		const float Bx = X + 24.f + I * (Bs + Gap), By = Y + 12.f;
		Button(IdBuild + I, Bx, By, Bs, Bs, FString(), bCan);
		if (Hover(Bx, By, Bs, Bs)) { HoverIdx = I; }
		BuildIcon(I, Bx + Bs * 0.5f, By + Bs * 0.5f - 4.f, 1.f, bCan ? 1.f : 0.35f);
		Txt(FString::FromInt(I + 1), Bx + 9.f, By + 6.f, 14.f, Dim);
		const int32 Cost = GM->BuildCost(Type);
		Txt(FString::Printf(TEXT("%d wood"), Cost), Bx + Bs * 0.5f, By + Bs + 4.f, 15.f, GM->Wood >= Cost ? Parchment : FLinearColor(1.f, 0.45f, 0.35f, 1.f), 1);
	}
	if (HoverIdx >= 0)
	{
		const float Tw = 520.f;
		const float Tx = SW * 0.5f - Tw * 0.5f, Ty = Y - 112.f;
		WoodPanel(Tx, Ty, Tw, 102.f);
		Txt(Names[HoverIdx], Tx + 20.f, Ty + 10.f, 19.f, Gold);
		Wrapped(Tips[HoverIdx], Tx + 20.f, Ty + 38.f, Tw - 40.f, 16.f, Parchment);
	}
	else if (!bPlot)
	{
		Txt(GM->Day < 3 ? TEXT("No free plot. Another is cleared on Day 3.") : TEXT("No free plot."), SW * 0.5f, Y - 30.f, 16.f, Dim, 1, false);
	}
	else
	{
		Txt(TEXT("A plot is marked and waiting: choose what to raise on it."), SW * 0.5f, Y - 30.f, 16.f, Parchment, 1, false);
	}
}

// ----- banner and toasts --------------------------------------------------------------------------------------------

void ATSCHUD::DrawBanner(ATSCGameMode* GM)
{
	if (GM->BannerTimer <= 0.f || GM->Banner.IsEmpty() || GM->bCutOpen) { return; }
	const bool bScene = GM->ActiveScene >= 0;
	const float W = 900.f, X = SW * 0.5f - W * 0.5f;
	const float Y = GM->Phase == ETSCPhase::Night ? 150.f : 96.f;
	const float A = FMath::Clamp(GM->BannerTimer, 0.f, 1.f);
	const bool bCan = GM->CanCut();
	const float H = bCan ? 118.f : 84.f;
	Box(X, Y, W, H, FLinearColor(0.02f, 0.015f, 0.01f, 0.88f * A));
	Frame(X, Y, W, H, bScene ? FLinearColor(CutRed.R, CutRed.G, CutRed.B, A) : FLinearColor(WoodLight.R, WoodLight.G, WoodLight.B, A), 3.f);
	const float End = Wrapped(GM->Banner, SW * 0.5f, Y + 14.f, W - 60.f, 20.f, FLinearColor(Parchment.R, Parchment.G, Parchment.B, A), true, 1);
	if (bCan)
	{
		Txt(TEXT("Press CUT! (or Space) to stop the scene and choose another take."), SW * 0.5f, FMath::Max(End + 4.f, Y + 80.f), 17.f,
			FLinearColor(1.f, 0.75f, 0.35f, 0.7f + 0.3f * FMath::Sin(GM->GetWorldTime() * 6.f)), 1);
	}
	else if (bScene && GM->Scenes[GM->ActiveScene].bCutOpenable && GM->EditsLeft <= 0)
	{
		Txt(TEXT("No edits left. The saga runs as written."), SW * 0.5f, Y + H - 28.f, 16.f, Dim, 1);
	}
}

void ATSCHUD::DrawToasts(ATSCGameMode* GM)
{
	float Y = SH - 190.f;
	for (int32 I = GM->Toasts.Num() - 1; I >= 0; --I)
	{
		const FTSCToast& T = GM->Toasts[I];
		const float A = FMath::Clamp(T.Time * 0.8f, 0.f, 1.f);
		const float W = 520.f;
		// measure height by drawing transparent first is overkill: assume up to three lines
		const int32 Lines = FMath::Clamp(FMath::CeilToInt(Measure(T.Text, 15.f, false).X / (W - 28.f) + 0.15f), 1, 4);
		const float H = Lines * 21.f + 14.f;
		Y -= H + 6.f;
		Box(18.f, Y, W, H, FLinearColor(0.02f, 0.015f, 0.01f, 0.78f * A));
		Box(18.f, Y, 4.f, H, FLinearColor(Gold.R, Gold.G, Gold.B, A));
		Wrapped(T.Text, 32.f, Y + 7.f, W - 28.f, 15.f, FLinearColor(Parchment.R, Parchment.G, Parchment.B, A));
		if (Y < 320.f) { break; }
	}
}

// ----- CUT! timeline ------------------------------------------------------------------------------------------------

void ATSCHUD::DrawCut(ATSCGameMode* GM)
{
	if (GM->ActiveScene < 0) { return; }
	const FTSCScene& S = GM->Scenes[GM->ActiveScene];
	Box(0.f, 0.f, SW, SH, FLinearColor(0.f, 0.f, 0.f, 0.62f));
	Txt(TEXT("CUT!"), SW * 0.5f, 70.f, 68.f, CutRed, 1);
	Txt(TEXT("The world is frozen.  ") + S.Title, SW * 0.5f, 164.f, 26.f, Parchment, 1);

	// film strip
	const float StripY = 230.f, StripH = 560.f;
	Box(0.f, StripY, SW, StripH, FLinearColor(0.015f, 0.012f, 0.01f, 0.97f));
	for (float X = 14.f; X < SW; X += 46.f)
	{
		Box(X, StripY + 12.f, 24.f, 18.f, FLinearColor(0.3f, 0.28f, 0.24f, 1.f));
		Box(X, StripY + StripH - 30.f, 24.f, 18.f, FLinearColor(0.3f, 0.28f, 0.24f, 1.f));
	}
	const float Fw = FMath::Min(760.f, SW * 0.5f - 60.f), Fh = StripH - 96.f, Fy = StripY + 48.f;
	for (int32 Take = 0; Take < 2; ++Take)
	{
		const float Fx = Take == 0 ? SW * 0.5f - Fw - 20.f : SW * 0.5f + 20.f;
		const bool bSel = GM->SelectedTake == Take;
		const bool bHover = Hover(Fx, Fy, Fw, Fh);
		const FLinearColor Tint = Take == 0 ? FLinearColor(0.035f, 0.085f, 0.05f, 1.f) : FLinearColor(0.085f, 0.07f, 0.06f, 1.f);
		Box(Fx, Fy, Fw, Fh, bHover && !bSel ? FLinearColor(Tint.R * 1.5f, Tint.G * 1.5f, Tint.B * 1.5f, 1.f) : Tint);
		// a strip of sky and ground: the mood of each saga
		Box(Fx, Fy, Fw, 8.f, Take == 0 ? LivingGreen : FLinearColor(0.9f, 0.4f, 0.12f, 1.f));
		Frame(Fx, Fy, Fw, Fh, bSel ? Gold : FLinearColor(0.3f, 0.28f, 0.24f, 1.f), bSel ? 5.f : 2.f);
		Txt(Take == 0 ? TEXT("TAKE A  -  THE SCENE AS IT STANDS") : TEXT("TAKE B  -  THE ALTERNATE TAKE"), Fx + 28.f, Fy + 26.f, 16.f, Dim);
		Wrapped(Take == 0 ? S.LivingName : S.AshenName, Fx + 28.f, Fy + 56.f, Fw - 56.f, 27.f, Take == 0 ? LivingGreen : FLinearColor(1.f, 0.62f, 0.3f, 1.f), true);
		Wrapped(Take == 0 ? S.LivingText : S.AshenText, Fx + 28.f, Fy + 150.f, Fw - 56.f, 21.f, Parchment);
		Txt(Take == 0 ? TEXT("Costs no edit.") : TEXT("Costs 1 edit. Permanent."), Fx + 28.f, Fy + Fh - 48.f, 18.f, Take == 0 ? Dim : FLinearColor(1.f, 0.75f, 0.35f, 1.f));
		if (bSel) { Txt(TEXT("SELECTED"), Fx + Fw - 28.f, Fy + 26.f, 16.f, Gold, 2); }
		FHit Hit;
		Hit.Box = FBox2D(FVector2D(Fx * U, Fy * U), FVector2D((Fx + Fw) * U, (Fy + Fh) * U));
		Hit.Id = Take == 0 ? IdTakeLiving : IdTakeAshen;
		Hits.Add(Hit);
	}
	Txt(TEXT("Every edit solves one problem and creates or keeps another."), SW * 0.5f, StripY + StripH + 22.f, 20.f, Dim, 1, false);
	const FString Label = GM->SelectedTake == 1 ? FString::Printf(TEXT("CONFIRM THE EDIT  (%d left)"), GM->EditsLeft) : FString(TEXT("KEEP THIS TAKE - RESUME"));
	const FLinearColor Col = GM->SelectedTake == 1 ? CutRed : WoodMid;
	Button(IdConfirm, SW * 0.5f - 260.f, StripY + StripH + 66.f, 520.f, 74.f, Label, true, 26.f, &Col);
}

// ----- title and epilogue -------------------------------------------------------------------------------------------

void ATSCHUD::DrawTitle(ATSCGameMode* GM)
{
	Box(0.f, 0.f, SW, SH, FLinearColor(0.f, 0.f, 0.f, 0.55f));
	const float W = 980.f, X = SW * 0.5f - W * 0.5f, Y = 130.f;
	WoodPanel(X, Y, W, 760.f);
	Knot(X + 70.f, Y + 78.f, 14.f, Gold);
	Knot(X + W - 70.f, Y + 78.f, 14.f, Gold);
	Txt(TEXT("THE SAGA CUTTER"), SW * 0.5f, Y + 36.f, 66.f, Gold, 1);
	Txt(TEXT("three days, one night, two sagas"), SW * 0.5f, Y + 128.f, 22.f, Dim, 1, false);
	float Ty = Y + 196.f;
	Ty = Wrapped(TEXT("Your settlement exists in two tellings. The Living Saga is a misty forest, rich in timber, food and glowing mushrooms, and crawling with the things that grow in the fog. The Ashen Saga is the same land after fire and war: open paths, stone walls, and little left to harvest."),
		X + 70.f, Ty, W - 140.f, 21.f, Parchment);
	Ty = Wrapped(TEXT("When a scene goes wrong, press CUT! The world freezes and you may swap that scene for its other take. Every edit solves one problem and creates or keeps another, and you have only three."),
		X + 70.f, Ty + 16.f, W - 140.f, 21.f, Parchment);
	Ty = Wrapped(TEXT("By day: assign your people to wood and food, and raise a building on the marked plot. On the final night, creatures come from the forest and raiders from the sea. Then the skald will sing your cut of the saga."),
		X + 70.f, Ty + 16.f, W - 140.f, 21.f, Parchment);
	const FLinearColor Col = CutRed;
	Button(IdStart, SW * 0.5f - 220.f, Y + 610.f, 440.f, 84.f, TEXT("BEGIN THE SAGA"), true, 30.f, &Col);
	Txt(TEXT("Mouse: buttons   |   Space: CUT!   |   WASD / wheel: move and zoom the camera"), SW * 0.5f, Y + 712.f, 16.f, Dim, 1, false);
}

void ATSCHUD::DrawEpilogue(ATSCGameMode* GM)
{
	Box(0.f, 0.f, SW, SH, FLinearColor(0.f, 0.f, 0.f, 0.6f));
	const float W = 1180.f, X = SW * 0.5f - W * 0.5f, Y = 50.f;
	WoodPanel(X, Y, W, SH - 100.f);
	Knot(X + 60.f, Y + 62.f, 12.f, Gold);
	Knot(X + W - 60.f, Y + 62.f, 12.f, Gold);
	Txt(GM->EpilogueTitle, SW * 0.5f, Y + 34.f, 40.f, Gold, 1);
	float Ty = Y + 112.f;
	for (int32 I = 0; I < GM->Epilogue.Num(); ++I)
	{
		const bool bLast = I == GM->Epilogue.Num() - 1;
		Ty = Wrapped(GM->Epilogue[I], X + 60.f, Ty, W - 120.f, bLast ? 16.f : 19.f, I == 0 || bLast ? Dim : Parchment, false) + 9.f;
	}
	const FLinearColor Col = CutRed;
	Button(IdRestart, SW * 0.5f - 200.f, SH - 140.f, 400.f, 70.f, TEXT("CUT A NEW SAGA"), true, 26.f, &Col);
}
