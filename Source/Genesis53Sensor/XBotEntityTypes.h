#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "XBotEntityTypes.generated.h"

// ---------------------------------------------------------------------------
// LOD tier — determines interpolation frequency based on distance to camera
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EXBotLODTier : uint8
{
	Near    UMETA(DisplayName = "Near  (full smooth, every frame)"),
	Mid     UMETA(DisplayName = "Mid   (reduced tick, interpolated)"),
	Far     UMETA(DisplayName = "Far   (snap to latest, min cycles)"),
};

// ---------------------------------------------------------------------------
// Parsed state for a single xBot entity from sim.state.update
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FXBotEntityState
{
	GENERATED_BODY()

	// Identity
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Model;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Type;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Status;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Domain;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Faction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Callsign;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Mode;

	// Position — alt in METERS MSL, lat/lon decimal degrees
	UPROPERTY(EditAnywhere, BlueprintReadOnly) double  Lat        = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) double  Lon        = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) double  AltMeters  = 0.0;

	// Motion — heading degrees true, speed KNOTS
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float   HeadingDeg = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float   SpeedKts   = 0.0f;

	// External control flags
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool    bExternallyControlled = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString ControllerGenesisId;
};

// ---------------------------------------------------------------------------
// Snapshot: a received state paired with the wall-clock time it arrived.
// Used by the smoothing buffer in AXBotEntityActor.
// ---------------------------------------------------------------------------
USTRUCT()
struct FXBotStateSnapshot
{
	GENERATED_BODY()

	FXBotEntityState State;
	double           ReceivedTime = 0.0;   // FPlatformTime::Seconds() at receipt
};

// ---------------------------------------------------------------------------
// Data Table row — maps a catalog model string to a 3D mesh asset.
// Create DT_XBotModelCatalog in the Editor using this struct as the row type.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FXBotModelMappingRow : public FTableRowBase
{
	GENERATED_BODY()

	// The static mesh to represent this model.
	// Leave null to use the domain-default placeholder (cone/box/sphere).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UStaticMesh> Mesh;

	// Scale applied to the mesh component (allows per-model size tuning).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);

	// Fallback color used when no mesh is assigned (tints a primitive shape).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor FallbackColor = FLinearColor::White;
};
