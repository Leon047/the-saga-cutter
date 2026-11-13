// The Saga Cutter - game jam project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TSCHUD.generated.h"

class ATSCGameMode;

/**
 * Whole UI drawn on the canvas: carved-wood resource bar, job panel, build bar, health bars and need bubbles,
 * the CUT! film-strip timeline, title screen and the skald's epilogue. Buttons are plain hit rectangles.
 */
UCLASS()
class ATSCHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Left mouse click at viewport position. Returns true if a button took it. */
	bool HandleClick(const FVector2D& Pos);
	/** Space bar: press CUT!, or confirm in the timeline. */
	void HandleCutKey();

private:
	struct FHit
	{
		FBox2D Box;
		int32 Id;
	};

	void Box(float X, float Y, float W, float H, const FLinearColor& C);
	void Frame(float X, float Y, float W, float H, const FLinearColor& C, float Th = 2.f);
	void Seg(float X1, float Y1, float X2, float Y2, const FLinearColor& C, float Th = 2.f);
	FVector2D Measure(const FString& S, float Size, bool bBold = true) const;
	void Txt(const FString& S, float X, float Y, float Size, const FLinearColor& C, int32 Align = 0, bool bBold = true);
	/** Draws S as given (already translated). */
	void TxtRaw(const FString& S, float X, float Y, float Size, const FLinearColor& C, int32 Align = 0, bool bBold = true);
	float Wrapped(const FString& S, float X, float Y, float MaxW, float Size, const FLinearColor& C, bool bBold = false, int32 Align = 0);
	void WoodPanel(float X, float Y, float W, float H);
	void Knot(float Cx, float Cy, float R, const FLinearColor& C);
	bool Button(int32 Id, float X, float Y, float W, float H, const FString& Label, bool bEnabled, float Size = 20.f, const FLinearColor* Tint = nullptr);
	bool Hover(float X, float Y, float W, float H) const;
	void Act(int32 Id);

	void DrawWorld(ATSCGameMode* GM);
	void DrawTop(ATSCGameMode* GM);
	void DrawJobs(ATSCGameMode* GM);
	void DrawBuildBar(ATSCGameMode* GM);
	void DrawDayOnePrompt(ATSCGameMode* GM);
	void DrawBanner(ATSCGameMode* GM);
	void DrawToasts(ATSCGameMode* GM);
	void DrawCut(ATSCGameMode* GM);
	void DrawTitle(ATSCGameMode* GM);
	void DrawEpilogue(ATSCGameMode* GM);
	void BuildIcon(int32 Type, float Cx, float Cy, float S, float Alpha);

	TArray<FHit> Hits;
	FVector2D Mouse = FVector2D(-1.f, -1.f);
	float U = 1.f;      // UI scale (1 at 1080p)
	float SW = 1920.f;  // screen size in UI units
	float SH = 1080.f;
};
