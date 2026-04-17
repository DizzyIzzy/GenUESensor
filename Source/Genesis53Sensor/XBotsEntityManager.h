#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "XBotEntityTypes.h"
#include "XBotsEntityManager.generated.h"

class AXBotEntityActor;
class UXBotsSubsystem;

/** World subsystem that drives the full lifecycle of xBot entity actors.
 *  Listens to UXBotsSubsystem delegates, spawns/despawns AXBotEntityActors,
 *  and evaluates per-frame distance to determine LOD tier for each actor. */
UCLASS(Blueprintable, Config=Game)
class GENESIS53SENSOR_API UXBotsEntityManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	// Config — editable via DefaultGame.ini or BP override
	// ------------------------------------------------------------------

	/** Geo-reference Data Table keyed on model string (e.g. "AZOR") →
	 *  FXBotModelMappingRow. Assign in Blueprints or Project Settings. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "XBots|Assets")
	TSoftObjectPtr<UDataTable> ModelCatalogTable;

	/** Mesh shown when a model name has no row in ModelCatalogTable.
	 *  Defaults to the horten placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XBots|Assets")
	TSoftObjectPtr<UStaticMesh> FallbackMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Game/xbotsTools/models/horten/horten.horten")));

	/** Seconds to buffer before rendering positions (passed to every actor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XBots|Smoothing",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float EntitySmoothingDelay = 1.0f;

	/** Show callsign labels on all entities. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XBots|Display")
	bool bShowEntityLabels = true;

	/** Master switch for distance-based LOD tick-rate reduction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XBots|LOD")
	bool bEnableDistanceLOD = true;

	/** Boundary between Near and Mid LOD tiers (km from camera). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XBots|LOD",
		meta = (ClampMin = "1.0"))
	float LodNearDistanceKm = 15.0f;

	/** Boundary between Mid and Far LOD tiers (km from camera). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XBots|LOD",
		meta = (ClampMin = "1.0"))
	float LodMidDistanceKm = 50.0f;

	/** Show entities whose domain is "AIR". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XBots|Filter")
	bool bShowAir = true;

	/** Show entities whose domain is "SEA". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XBots|Filter")
	bool bShowSea = true;

	/** Show entities whose domain is "LAND". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XBots|Filter")
	bool bShowLand = true;

	// ------------------------------------------------------------------
	// Runtime stats (read-only)
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "XBots")
	int32 LiveActorCount = 0;

	// ------------------------------------------------------------------
	// Blueprint-callable setters (propagate to all live actors)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "XBots|Display")
	void SetEntityLabelsVisible(bool bVisible);

	// ------------------------------------------------------------------
	// UWorldSubsystem / UTickableWorldSubsystem interface
	// ------------------------------------------------------------------

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	// All live entity actors keyed by entity ID
	TMap<FString, AXBotEntityActor*> LiveActors;

	// Cached weak pointer to the game-instance subsystem
	TWeakObjectPtr<UXBotsSubsystem> XBotsSubsystemRef;

	// ------------------------------------------------------------------
	// Delegate handlers
	// ------------------------------------------------------------------

	UFUNCTION()
	void HandleEntityAdded(const FXBotEntityState& State);

	UFUNCTION()
	void HandleEntityUpdated(const FXBotEntityState& State);

	UFUNCTION()
	void HandleEntityRemoved(const FString& EntityId);

	// ------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------

	AXBotEntityActor* SpawnActorForEntity(const FXBotEntityState& State);
	void ApplyMeshToActor(AXBotEntityActor* Actor, const FString& ModelName);
	EXBotLODTier ComputeLODTier(AXBotEntityActor* Actor) const;
};
