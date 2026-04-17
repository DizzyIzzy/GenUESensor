// GenesisDebugHUD.h - Debug HUD overlay for visualizing MQTT/OSC stream data

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GenesisDebugHUD.generated.h"

class AGenesisOSCReceiver;

/**
 * Debug HUD for visualizing real-time MQTT/OSC data stream
 * Displays connection status, incoming values, and fallback warnings
 */
UCLASS()
class GENESIS53SENSOR_API AGenesisDebugHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	AGenesisDebugHUD();

	virtual void DrawHUD() override;
	virtual void BeginPlay() override;

	// Reference to the OSC Receiver to monitor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Genesis Debug")
	AGenesisOSCReceiver* OSCReceiver;

	// Display Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Genesis Debug")
	bool bShowGenesisDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Genesis Debug")
	float DebugTextScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Genesis Debug")
	FLinearColor NormalColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Genesis Debug")
	FLinearColor WarningColor = FLinearColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Genesis Debug")
	FLinearColor ErrorColor = FLinearColor::Red;

private:
	// Helper to draw text at current Y position and auto-increment
	void DrawDebugText(const FString& Text, float X, float& Y, const FLinearColor& Color);

	// Calculate connection health based on last update time
	bool bOSCConnectionHealthy = false;
	double LastOSCUpdateTime = 0.0;
	const double OSCTimeoutSeconds = 5.0; // Consider connection dead after 5 seconds
};
