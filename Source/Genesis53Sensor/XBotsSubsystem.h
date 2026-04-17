#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "XBotEntityTypes.h"
#include "IWebSocket.h"
#include "XBotsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnXBotEntityAdded,   const FXBotEntityState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnXBotEntityUpdated, const FXBotEntityState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnXBotEntityRemoved, const FString&,          EntityId);

/**
 * UXBotsSubsystem
 * 
 * Game instance subsystem that maintains a WebSocket connection to the xBots
 * API Gateway and broadcasts parsed entity state to the rest of the game.
 *
 * Connection:  ws://{XBotsIP}:{XBotsWSPort}/ws
 * Data source: sim.state.update JSON — { ts, entities: [...] }
 */
UCLASS(BlueprintType)
class GENESIS53SENSOR_API UXBotsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// -------------------------------------------------------------------------
	// USubsystem interface
	// -------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// -------------------------------------------------------------------------
	// Configuration (editable via Project Settings or Blueprint)
	// -------------------------------------------------------------------------

	/** IP address of the xBots API Gateway machine */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="xBots Configuration")
	FString XBotsIP = TEXT("10.0.0.100");

	/** Port the xBots API Gateway listens on (default 3000) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="xBots Configuration")
	int32 XBotsWSPort = 3000;

	/** Connect automatically on Initialize() */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="xBots Configuration")
	bool bAutoConnect = true;

	/** Seconds between reconnect attempts when connection drops */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="xBots Configuration")
	float ReconnectIntervalSeconds = 5.0f;

	// -------------------------------------------------------------------------
	// Status (read-only)
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category="xBots Status")
	bool bIsConnected = false;

	UPROPERTY(BlueprintReadOnly, Category="xBots Status")
	double LastUpdateTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="xBots Status")
	int32 LastEntityCount = 0;

	// -------------------------------------------------------------------------
	// Delegates — bind these to react to entity lifecycle changes
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category="xBots Events")
	FOnXBotEntityAdded OnEntityAdded;

	UPROPERTY(BlueprintAssignable, Category="xBots Events")
	FOnXBotEntityUpdated OnEntityUpdated;

	UPROPERTY(BlueprintAssignable, Category="xBots Events")
	FOnXBotEntityRemoved OnEntityRemoved;

	// -------------------------------------------------------------------------
	// Public API
	// -------------------------------------------------------------------------

	/** Manually connect (called automatically if bAutoConnect=true) */
	UFUNCTION(BlueprintCallable, Category="xBots Connection")
	void Connect();

	/** Disconnect cleanly */
	UFUNCTION(BlueprintCallable, Category="xBots Connection")
	void Disconnect();

	/**
	 * Send a command to xBots via the same WebSocket.
	 * Payload must be a valid JSON object string, e.g. {"entity_id":"AZOR_01","mode":"PATROL"}
	 */
	UFUNCTION(BlueprintCallable, Category="xBots Commands")
	void SendCommand(const FString& RoutingKey, const FString& JsonPayload);

	/** Returns a snapshot of the current entity cache (all known entities) */
	UFUNCTION(BlueprintCallable, Category="xBots Data")
	TArray<FXBotEntityState> GetAllEntities() const;

	/** Returns the state for a single entity by ID, or false if not found */
	UFUNCTION(BlueprintCallable, Category="xBots Data")
	bool GetEntityById(const FString& EntityId, FXBotEntityState& OutState) const;

private:
	TSharedPtr<IWebSocket> WebSocket;
	TMap<FString, FXBotEntityState> EntityCache;

	// Reconnect timer
	FTimerHandle ReconnectTimerHandle;
	bool bIntentionalDisconnect = false;

	void OnConnected();
	void OnConnectionError(const FString& Error);
	void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnMessage(const FString& Message);

	void ProcessStateUpdate(const TSharedPtr<FJsonObject>& JsonObj);
	FXBotEntityState ParseEntityJson(const TSharedPtr<FJsonObject>& EntObj) const;

	void ScheduleReconnect();
};
