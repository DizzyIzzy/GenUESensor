// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GenesisMQTTSubsystem.generated.h"

// Stub enum for temporary MQTT disable
UENUM(BlueprintType)
enum class EGenesisMQTTConnectReturnCode : uint8
{
	Accepted UMETA(DisplayName = "Accepted")
};

/**
 * Subsystem to manage the MQTT connection to receive positioning and camera orientation.
 * NOTE: MQTT functionality is temporarily disabled due to missing Engine plugin files.
 */
UCLASS(BlueprintType)
class GENESIS53SENSOR_API UGenesisMQTTSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem Override
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Connection Details
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MQTT Configuration")
	FString BrokerIP = TEXT("10.0.0.17");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MQTT Configuration")
	int32 Port = 1883;

	UPROPERTY(BlueprintReadOnly, Category = "MQTT Configuration")
	UObject* MQTTClient;

	// Exposed Functions
	UFUNCTION(BlueprintCallable, Category = "MQTT Connection")
	void ConnectToBroker();

	UFUNCTION(BlueprintCallable, Category = "MQTT Connection")
	void DisconnectFromBroker();

private:
	// Handle established connection
	UFUNCTION()
	void OnMQTTConnected(EGenesisMQTTConnectReturnCode ReturnCode);

	// Handle broken connection
	UFUNCTION()
	void OnMQTTDisconnected();

	// Function to parse the config JSON for topics
	void ParseChannelListJSON();
};
