#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OSCServer.h"
#include "CesiumGeoreference.h"
#include "GenesisOSCReceiver.generated.h"

UCLASS()
class GENESIS53SENSOR_API AGenesisOSCReceiver : public AActor
{
	GENERATED_BODY()
	
public:	
	AGenesisOSCReceiver();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

public:
	// Master enable/disable toggle - can be changed in editor or at runtime
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OSC Configuration")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OSC Configuration")
	FString ReceiveIP = TEXT("127.0.0.1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OSC Configuration")
	int32 ReceivePort = 8000;

	// Python Bridge Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MQTT Bridge Configuration")
	bool bAutoStartPythonBridge = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MQTT Bridge Configuration")
	FString MQTTBrokerIP = TEXT("10.0.0.17");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MQTT Bridge Configuration")
	int32 MQTTBrokerPort = 1883;

	// Core Parsed Values
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parsed Data")
	float Latitude = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parsed Data")
	float Longitude = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parsed Data")
	float AltitudeMSL = 0.0f;

	// Camera 1 Values
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parsed Data|Camera 1")
	FVector Camera1Offset = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parsed Data|Camera 1")
	FRotator Camera1Rotation = FRotator::ZeroRotator;

	// Status & Warning Information
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status")
	FString LastWarningMessage = TEXT("");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status")
	double LastDataReceivedTime = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status")
	bool bIsReceivingData = false;

	// ---- Live Wiring ----
	// Drag the CesiumGeoreference actor from the Outliner into this slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Genesis Wiring")
	ACesiumGeoreference* GeoreferenceTarget = nullptr;

	// Drag the OWL Cine Camera actor from the Outliner into this slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Genesis Wiring")
	AActor* Camera1Actor = nullptr;

	// Runtime control functions
	UFUNCTION(BlueprintCallable, Category="Genesis Control")
	void SetEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintCallable, Category="Genesis Control")
	bool IsEnabled() const { return bEnabled; }

	UFUNCTION(BlueprintCallable, Category="Genesis Control")
	void ToggleEnabled();

private:
	UPROPERTY()
	UOSCServer* OSCServer;

	// Handle to the background Python process
	FProcHandle PythonBridgeProcessHandle;

	// Dirty flags — set by the OSC callback, consumed by Tick on the game thread
	bool bGeoUpdated = false;
	bool bCameraUpdated = false;

	// Track whether we've started the receiver to avoid redundant starts
	bool bReceiverStarted = false;

	// Internal helper functions
	void StartReceiver();
	void StopReceiver();

	UFUNCTION()
	void OnOSCMessageReceived(const FOSCMessage& Message, const FString& IPAddress, int32 Port);
};