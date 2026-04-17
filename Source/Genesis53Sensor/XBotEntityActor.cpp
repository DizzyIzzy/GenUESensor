#include "XBotEntityActor.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "HAL/IConsoleManager.h"

// --------------------------------------------------------------------------
// Console Variables — tweak live in PIE with the ~ console
//   xbots.SmoothingDelay  <seconds>   e.g. "xbots.SmoothingDelay 0.5"
//   xbots.Interpolation   0|1         0=Linear, 1=CatmullRom (default)
// --------------------------------------------------------------------------
static TAutoConsoleVariable<float> CVarXBotSmoothingDelay(
	TEXT("xbots.SmoothingDelay"),
	1.0f,
	TEXT("Seconds behind real-time to render xBot entities (interpolation window).\n")
	TEXT("Lower = less delay but more jitter. Higher = smoother but more lag.\n")
	TEXT("Recommended range: 0.1 – 2.0"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarXBotInterpolation(
	TEXT("xbots.Interpolation"),
	1,
	TEXT("xBot position interpolation algorithm:\n")
	TEXT("  0 = Linear lerp (fast, can appear fast-slow-fast on uneven updates)\n")
	TEXT("  1 = Catmull-Rom spline (smooth speed through waypoints, default)"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarXBotBufferSeconds(
	TEXT("xbots.BufferSeconds"),
	30.0f,
	TEXT("How many seconds of snapshot history to keep per entity.\n")
	TEXT("Must be >= xbots.SmoothingDelay. Default 30s covers any reasonable delay value."),
	ECVF_Default);

AXBotEntityActor::AXBotEntityActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(MeshComp);

	LabelComp = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelComp->SetupAttachment(MeshComp);
	LabelComp->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	LabelComp->SetTextRenderColor(FColor::White);
	LabelComp->SetWorldSize(50.f);
	LabelComp->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
}

void AXBotEntityActor::BeginPlay()
{
	Super::BeginPlay();
	LabelComp->SetVisibility(bShowLabel, true);
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

void AXBotEntityActor::EnqueueState(const FXBotEntityState& State)
{
	LastKnownState = State;

	FXBotStateSnapshot Snap;
	Snap.State        = State;
	Snap.ReceivedTime = FPlatformTime::Seconds();

	SnapshotBuffer.Add(MoveTemp(Snap));

	// Keep the ring buffer capped to BufferSeconds worth of data at ~10 Hz.
	// Use CVar so it can be tuned without recompile.
	const int32 MaxSnaps = FMath::Max(10,
		FMath::RoundToInt(CVarXBotBufferSeconds.GetValueOnGameThread() * 10.0f));
	while (SnapshotBuffer.Num() > MaxSnaps)
	{
		SnapshotBuffer.RemoveAt(0, 1, false);
	}

	// Update the label text whenever new data arrives
	if (!State.Callsign.IsEmpty())
	{
		LabelComp->SetText(FText::FromString(State.Callsign));
	}
	else
	{
		LabelComp->SetText(FText::FromString(State.Id));
	}
}

void AXBotEntityActor::SetLODTier(EXBotLODTier NewTier)
{
	if (CurrentLODTier == NewTier)
	{
		return;
	}
	CurrentLODTier = NewTier;

	switch (NewTier)
	{
	case EXBotLODTier::Near:
		SetActorTickInterval(TickIntervalNear);
		SetActorHiddenInGame(false);
		break;
	case EXBotLODTier::Mid:
		SetActorTickInterval(TickIntervalMid);
		SetActorHiddenInGame(false);
		break;
	case EXBotLODTier::Far:
		SetActorTickInterval(TickIntervalFar);
		SetActorHiddenInGame(false);
		break;
	}
}

void AXBotEntityActor::SetLabelVisible(bool bVisible)
{
	bShowLabel = bVisible;
	LabelComp->SetVisibility(bVisible, true);
}

// --------------------------------------------------------------------------
// Tick — smoothed pose update
// --------------------------------------------------------------------------

void AXBotEntityActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (SnapshotBuffer.IsEmpty())
	{
		return;
	}

	// Read global CVar — overrides the per-actor UPROPERTY so all entities
	// respond immediately to console tweaks during PIE.
	const float Delay = CVarXBotSmoothingDelay.GetValueOnGameThread();
	const bool bCatmullRom = (CVarXBotInterpolation.GetValueOnGameThread() == 1);

	// No delay or only one snapshot — snap to latest position
	if (SnapshotBuffer.Num() == 1 || Delay <= 0.f)
	{
		const FXBotStateSnapshot& Latest = SnapshotBuffer.Last();
		GlobeAnchor->MoveToLongitudeLatitudeHeight(
			FVector(Latest.State.Lon, Latest.State.Lat, Latest.State.AltMeters));
		SetActorRotation(FRotator(0.f, Latest.State.HeadingDeg, 0.f));
		return;
	}

	const double TargetTime = FPlatformTime::Seconds() - Delay;

	// ---- Catmull-Rom path (default) ----
	if (bCatmullRom && SnapshotBuffer.Num() >= 2)
	{
		FXBotStateSnapshot P0, P1, P2, P3;
		float Alpha = 0.f;
		if (FindCatmullRomSnapshots(TargetTime, P0, P1, P2, P3, Alpha))
		{
			ApplyCatmullRomPose(P0, P1, P2, P3, Alpha);
			return;
		}
		// Fall through to oldest pose if buffer not warm yet
		const FXBotStateSnapshot& Oldest = SnapshotBuffer[0];
		GlobeAnchor->MoveToLongitudeLatitudeHeight(
			FVector(Oldest.State.Lon, Oldest.State.Lat, Oldest.State.AltMeters));
		SetActorRotation(FRotator(0.f, Oldest.State.HeadingDeg, 0.f));
		return;
	}

	// ---- Linear lerp path ----
	FXBotStateSnapshot Before, After;
	float Alpha = 0.f;
	if (!FindBracketingSnapshots(TargetTime, Before, After, Alpha))
	{
		const FXBotStateSnapshot& Oldest = SnapshotBuffer[0];
		GlobeAnchor->MoveToLongitudeLatitudeHeight(
			FVector(Oldest.State.Lon, Oldest.State.Lat, Oldest.State.AltMeters));
		SetActorRotation(FRotator(0.f, Oldest.State.HeadingDeg, 0.f));
		return;
	}
	ApplyInterpolatedPose(Before, After, Alpha);
}


// --------------------------------------------------------------------------
// Private helpers
// --------------------------------------------------------------------------

bool AXBotEntityActor::FindBracketingSnapshots(
	double TargetTime,
	FXBotStateSnapshot& OutBefore,
	FXBotStateSnapshot& OutAfter,
	float& OutAlpha) const
{
	if (SnapshotBuffer.Num() < 2)
	{
		return false;
	}

	// Buffer is stored oldest-first (indices 0..N-1 ascending in ReceivedTime)

	// If target is beyond the newest snapshot, clamp to latest
	if (TargetTime >= SnapshotBuffer.Last().ReceivedTime)
	{
		OutBefore = SnapshotBuffer.Last();
		OutAfter  = SnapshotBuffer.Last();
		OutAlpha  = 1.f;
		return true;
	}

	// If target is before the oldest snapshot
	if (TargetTime <= SnapshotBuffer[0].ReceivedTime)
	{
		return false;
	}

	// Linear scan for the pair that brackets TargetTime
	for (int32 i = 0; i < SnapshotBuffer.Num() - 1; ++i)
	{
		const FXBotStateSnapshot& A = SnapshotBuffer[i];
		const FXBotStateSnapshot& B = SnapshotBuffer[i + 1];

		if (A.ReceivedTime <= TargetTime && TargetTime < B.ReceivedTime)
		{
			OutBefore = A;
			OutAfter  = B;
			const double Span = B.ReceivedTime - A.ReceivedTime;
			OutAlpha = (Span > SMALL_NUMBER)
				? static_cast<float>((TargetTime - A.ReceivedTime) / Span)
				: 0.f;
			return true;
		}
	}

	return false;
}

void AXBotEntityActor::ApplyInterpolatedPose(
	const FXBotStateSnapshot& Before,
	const FXBotStateSnapshot& After,
	float Alpha)
{
	const double ILon     = FMath::Lerp(Before.State.Lon,       After.State.Lon,       static_cast<double>(Alpha));
	const double ILat     = FMath::Lerp(Before.State.Lat,       After.State.Lat,       static_cast<double>(Alpha));
	const double IAltM    = FMath::Lerp(Before.State.AltMeters, After.State.AltMeters, static_cast<double>(Alpha));
	const float  IHeading = LerpHeading(Before.State.HeadingDeg, After.State.HeadingDeg, Alpha);

	// Cesium MoveToLongitudeLatitudeHeight expects (Longitude, Latitude, Height in meters)
	GlobeAnchor->MoveToLongitudeLatitudeHeight(FVector(ILon, ILat, IAltM));
	SetActorRotation(FRotator(0.f, IHeading, 0.f));
}

// static
float AXBotEntityActor::NormalizeHeading(float Deg)
{
	Deg = FMath::Fmod(Deg, 360.f);
	if (Deg < 0.f) Deg += 360.f;
	return Deg;
}

// static
float AXBotEntityActor::LerpHeading(float FromDeg, float ToDeg, float Alpha)
{
	FromDeg = NormalizeHeading(FromDeg);
	ToDeg   = NormalizeHeading(ToDeg);

	float Delta = ToDeg - FromDeg;
	// Choose the shorter arc across the 0/360 wrap
	if (Delta > 180.f)  Delta -= 360.f;
	if (Delta < -180.f) Delta += 360.f;

	return NormalizeHeading(FromDeg + Delta * Alpha);
}

// --------------------------------------------------------------------------
// Catmull-Rom helpers
// --------------------------------------------------------------------------

bool AXBotEntityActor::FindCatmullRomSnapshots(
	double TargetTime,
	FXBotStateSnapshot& P0,
	FXBotStateSnapshot& P1,
	FXBotStateSnapshot& P2,
	FXBotStateSnapshot& P3,
	float& OutAlpha) const
{
	if (SnapshotBuffer.Num() < 2)
	{
		return false;
	}

	// Target is past the newest snapshot — clamp to latest two
	if (TargetTime >= SnapshotBuffer.Last().ReceivedTime)
	{
		const int32 N = SnapshotBuffer.Num();
		P0 = SnapshotBuffer[FMath::Max(0, N - 3)];
		P1 = SnapshotBuffer[FMath::Max(0, N - 2)];
		P2 = SnapshotBuffer.Last();
		P3 = SnapshotBuffer.Last();
		OutAlpha = 1.f;
		return true;
	}

	// Target is before the oldest snapshot
	if (TargetTime <= SnapshotBuffer[0].ReceivedTime)
	{
		return false;
	}

	// Find index i such that Buffer[i].ReceivedTime <= TargetTime < Buffer[i+1].ReceivedTime
	for (int32 i = 0; i < SnapshotBuffer.Num() - 1; ++i)
	{
		const FXBotStateSnapshot& A = SnapshotBuffer[i];
		const FXBotStateSnapshot& B = SnapshotBuffer[i + 1];

		if (A.ReceivedTime <= TargetTime && TargetTime < B.ReceivedTime)
		{
			// P1=A, P2=B; clamp neighbours at buffer edges
			const int32 N = SnapshotBuffer.Num();
			P0 = SnapshotBuffer[FMath::Max(0, i - 1)];
			P1 = A;
			P2 = B;
			P3 = SnapshotBuffer[FMath::Min(N - 1, i + 2)];

			const double Span = B.ReceivedTime - A.ReceivedTime;
			OutAlpha = (Span > SMALL_NUMBER)
				? static_cast<float>((TargetTime - A.ReceivedTime) / Span)
				: 0.f;
			return true;
		}
	}

	return false;
}

void AXBotEntityActor::ApplyCatmullRomPose(
	const FXBotStateSnapshot& P0,
	const FXBotStateSnapshot& P1,
	const FXBotStateSnapshot& P2,
	const FXBotStateSnapshot& P3,
	float Alpha)
{
	const double ILon  = CatmullRom(P0.State.Lon,       P1.State.Lon,       P2.State.Lon,       P3.State.Lon,       Alpha);
	const double ILat  = CatmullRom(P0.State.Lat,       P1.State.Lat,       P2.State.Lat,       P3.State.Lat,       Alpha);
	const double IAltM = CatmullRom(P0.State.AltMeters, P1.State.AltMeters, P2.State.AltMeters, P3.State.AltMeters, Alpha);
	// Heading: Catmull-Rom on angles needs unwrapping; use shortest-arc lerp P1->P2 instead
	const float IHeading = LerpHeading(P1.State.HeadingDeg, P2.State.HeadingDeg, Alpha);

	GlobeAnchor->MoveToLongitudeLatitudeHeight(FVector(ILon, ILat, IAltM));
	SetActorRotation(FRotator(0.f, IHeading, 0.f));
}

// static
double AXBotEntityActor::CatmullRom(double V0, double V1, double V2, double V3, float T)
{
	// Standard centripetal Catmull-Rom (alpha=0.5 uniform form)
	// Interpolates between V1 and V2; V0 and V3 shape the tangents.
	const double T2 = T * T;
	const double T3 = T2 * T;
	return 0.5 * (
		  2.0 * V1
		+ (-V0 + V2)               * T
		+ (2.0*V0 - 5.0*V1 + 4.0*V2 - V3) * T2
		+ (-V0 + 3.0*V1 - 3.0*V2 + V3)    * T3
	);
}
