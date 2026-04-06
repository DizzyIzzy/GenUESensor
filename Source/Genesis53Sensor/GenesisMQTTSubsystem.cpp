// Fill out your copyright notice in the Description page of Project Settings.

// MQTT PLUGIN DISABLED - The Engine's MQTT plugin intermediate files were accidentally deleted.
// To re-enable MQTT functionality:
// 1. Verify/Repair UE 5.3 installation via Epic Games Launcher
// 2. Uncomment MQTTCore in Genesis53Sensor.Build.cs  
// 3. Restore the original implementation of this file

#include "GenesisMQTTSubsystem.h"

void UGenesisMQTTSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Warning, TEXT("GenesisMQTTSubsystem: MQTT functionality is temporarily disabled."));
}

void UGenesisMQTTSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UGenesisMQTTSubsystem::ConnectToBroker()
{
	UE_LOG(LogTemp, Warning, TEXT("GenesisMQTTSubsystem: MQTT functionality is temporarily disabled."));
}

void UGenesisMQTTSubsystem::DisconnectFromBroker()
{
}

void UGenesisMQTTSubsystem::OnMQTTConnected(EGenesisMQTTConnectReturnCode ReturnCode)
{
}

void UGenesisMQTTSubsystem::OnMQTTDisconnected()
{
}

void UGenesisMQTTSubsystem::ParseChannelListJSON()
{
}

