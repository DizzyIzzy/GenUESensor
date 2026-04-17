#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XBotEntityTypes.h"
#include "XBotEntityActor.generated.h"

class UCesiumGlobeAnchorComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Static-mesh actor that represents a single xBot entity in the world.
 *  Position smoothing uses a delayed-playback ring buffer.  Two algorithms
 *  are available and switchable via the console CVar xbots.Interpolation:
 *    0 = Linear lerp   (simple, can appear fast-slow-fast on uneven update rates)
 *    1 = Catmull-Rom   (smooth speed through 4-point spline — default)
 *  The playback delay is globally overridden by CVar xbots.SmoothingDelay. */
UCLASS()
class GENESIS53SENSOR_API AXBotEntityActor : public AActor
{
	GENERATED_BODY()

public:
	AXBotEntityActor();

	// -----------------------------------------------------------------
	// Components
	// -----------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XBots")
	TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XBots")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XBots")
	TObjectPtr<UTextRenderComponent> LabelComp;

	// -----------------------------------------------------------------
	// Config
	// -----------------------------------------------------------------

	/** How far (seconds) behind real-time to render this entity to absorb jitter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XBots|Smoothing", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float SmoothingDelaySeconds = 1.0f;

	/** Show the callsign label above the entity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XBots|Display")
	bool bShowLabel = true;

	// -----------------------------------------------------------------
	// Runtime state (read-only from outside)
	// -----------------------------------------------------------------

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "XBots|Debug")
	FString EntityId;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "XBots|Debug")
	FXBotEntityState LastKnownState;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "XBots|Debug")
	EXBotLODTier CurrentLODTier = EXBotLODTier::Near;

	// -----------------------------------------------------------------
	// Public API (called by XBotsEntityManager)
	// -----------------------------------------------------------------

	/** Push a new simulation snapshot into the ring buffer. */
	void EnqueueState(const FXBotEntityState& State);

	/** Change LOD tier; adjusts tick interval to match. */
	void SetLODTier(EXBotLODTier NewTier);

	/** Propagate label visibility immediately. */
	void SetLabelVisible(bool bVisible);

	// AActor interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	// Ring buffer of raw snapshots.
	// Capacity is driven by CVar xbots.BufferSeconds (default 30s at ~10Hz = 300 snapshots).
	TArray<FXBotStateSnapshot> SnapshotBuffer;

	// Tick intervals per LOD tier (seconds). 0 = every frame.
	static constexpr float TickIntervalNear = 0.0f;
	static constexpr float TickIntervalMid  = 0.1f;
	static constexpr float TickIntervalFar  = 0.5f;

	/** Find the two snapshots that bracket TargetTime.
	 *  Returns false if the buffer doesn't have enough data yet. */
	bool FindBracketingSnapshots(
		double TargetTime,
		FXBotStateSnapshot& OutBefore,
		FXBotStateSnapshot& OutAfter,
		float& OutAlpha) const;

	/** Find 4 snapshots for Catmull-Rom: P0..P3 where the target sits between P1 and P2.
	 *  Falls back to the bracketing pair clamped at buffer edges. */
	bool FindCatmullRomSnapshots(
		double TargetTime,
		FXBotStateSnapshot& P0,
		FXBotStateSnapshot& P1,
		FXBotStateSnapshot& P2,
		FXBotStateSnapshot& P3,
		float& OutAlpha) const;

	/** Apply linearly-interpolated geodetic position + heading to the globe anchor. */
	void ApplyInterpolatedPose(const FXBotStateSnapshot& Before, const FXBotStateSnapshot& After, float Alpha);

	/** Apply Catmull-Rom spline pose across 4 snapshots. */
	void ApplyCatmullRomPose(
		const FXBotStateSnapshot& P0,
		const FXBotStateSnapshot& P1,
		const FXBotStateSnapshot& P2,
		const FXBotStateSnapshot& P3,
		float Alpha);

	/** Normalize a heading to [0, 360). */
	static float NormalizeHeading(float Deg);

	/** Shortest-arc lerp between two headings accounting for 0/360 wrap. */
	static float LerpHeading(float FromDeg, float ToDeg, float Alpha);

	/** Catmull-Rom interpolation for a scalar value (no wraparound). */
	static double CatmullRom(double P0, double P1, double P2, double P3, float T);
};
