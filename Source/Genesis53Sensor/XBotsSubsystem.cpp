#include "XBotsSubsystem.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"

void UXBotsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("XBotsSubsystem: Initialized. Target: ws://%s:%d/ws"), *XBotsIP, XBotsWSPort);

	if (bAutoConnect)
	{
		Connect();
	}
}

void UXBotsSubsystem::Deinitialize()
{
	bIntentionalDisconnect = true;

	if (GetGameInstance())
	{
		GetGameInstance()->GetTimerManager().ClearTimer(ReconnectTimerHandle);
	}

	Disconnect();
	Super::Deinitialize();
}

void UXBotsSubsystem::Connect()
{
	if (bIsConnected || (WebSocket.IsValid() && WebSocket->IsConnected()))
	{
		return;
	}

	if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
	{
		FModuleManager::Get().LoadModule("WebSockets");
	}

	const FString URL = FString::Printf(TEXT("ws://%s:%d/ws"), *XBotsIP, XBotsWSPort);
	UE_LOG(LogTemp, Log, TEXT("XBotsSubsystem: Connecting to %s ..."), *URL);

	WebSocket = FWebSocketsModule::Get().CreateWebSocket(URL, TEXT("ws"));

	WebSocket->OnConnected().AddUObject(this, &UXBotsSubsystem::OnConnected);
	WebSocket->OnConnectionError().AddUObject(this, &UXBotsSubsystem::OnConnectionError);
	WebSocket->OnClosed().AddUObject(this, &UXBotsSubsystem::OnClosed);
	WebSocket->OnMessage().AddUObject(this, &UXBotsSubsystem::OnMessage);

	WebSocket->Connect();
}

void UXBotsSubsystem::Disconnect()
{
	if (WebSocket.IsValid())
	{
		WebSocket->Close();
		WebSocket.Reset();
	}
	bIsConnected = false;
}

void UXBotsSubsystem::SendCommand(const FString& RoutingKey, const FString& JsonPayload)
{
	if (!WebSocket.IsValid() || !WebSocket->IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("XBotsSubsystem: Cannot send command — not connected."));
		return;
	}

	// xBots WS command envelope: { "type": "command", "routing_key": "...", "payload": {...} }
	const FString Envelope = FString::Printf(
		TEXT("{\"type\":\"command\",\"routing_key\":\"%s\",\"payload\":%s}"),
		*RoutingKey, *JsonPayload);

	WebSocket->Send(Envelope);
	UE_LOG(LogTemp, Log, TEXT("XBotsSubsystem: Sent command [%s]: %s"), *RoutingKey, *JsonPayload);
}

TArray<FXBotEntityState> UXBotsSubsystem::GetAllEntities() const
{
	TArray<FXBotEntityState> Out;
	EntityCache.GenerateValueArray(Out);
	return Out;
}

bool UXBotsSubsystem::GetEntityById(const FString& EntityId, FXBotEntityState& OutState) const
{
	const FXBotEntityState* Found = EntityCache.Find(EntityId);
	if (Found)
	{
		OutState = *Found;
		return true;
	}
	return false;
}

// -----------------------------------------------------------------------------
// WebSocket callbacks (fire on game thread via UE's WebSockets module)
// -----------------------------------------------------------------------------

void UXBotsSubsystem::OnConnected()
{
	bIsConnected = true;
	bIntentionalDisconnect = false;
	UE_LOG(LogTemp, Log, TEXT("XBotsSubsystem: Connected to ws://%s:%d/ws"), *XBotsIP, XBotsWSPort);
}

void UXBotsSubsystem::OnConnectionError(const FString& Error)
{
	bIsConnected = false;
	UE_LOG(LogTemp, Error, TEXT("XBotsSubsystem: Connection error: %s"), *Error);
	ScheduleReconnect();
}

void UXBotsSubsystem::OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	bIsConnected = false;
	UE_LOG(LogTemp, Warning, TEXT("XBotsSubsystem: Disconnected (code=%d, reason=%s, clean=%s)"),
		StatusCode, *Reason, bWasClean ? TEXT("yes") : TEXT("no"));

	if (!bIntentionalDisconnect)
	{
		ScheduleReconnect();
	}
}

void UXBotsSubsystem::OnMessage(const FString& Message)
{
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("XBotsSubsystem: Received non-JSON message (len=%d)"), Message.Len());
		return;
	}

	// The gateway broadcasts sim.state.update bodies which contain an "entities" array.
	// Other messages (e.g. command acks) will not have this field and are ignored for now.
	if (JsonObj->HasField(TEXT("entities")))
	{
		ProcessStateUpdate(JsonObj);
	}
}

// -----------------------------------------------------------------------------
// State parsing
// -----------------------------------------------------------------------------

void UXBotsSubsystem::ProcessStateUpdate(const TSharedPtr<FJsonObject>& JsonObj)
{
	LastUpdateTime = FPlatformTime::Seconds();

	const TArray<TSharedPtr<FJsonValue>>* EntitiesArr;
	if (!JsonObj->TryGetArrayField(TEXT("entities"), EntitiesArr))
	{
		return;
	}

	// Build a set of IDs we see this tick to detect removals
	TSet<FString> SeenIds;

	for (const TSharedPtr<FJsonValue>& EntVal : *EntitiesArr)
	{
		const TSharedPtr<FJsonObject>* EntObjPtr;
		if (!EntVal->TryGetObject(EntObjPtr))
		{
			continue;
		}

		FXBotEntityState NewState = ParseEntityJson(*EntObjPtr);
		if (NewState.Id.IsEmpty())
		{
			continue;
		}

		// Skip DEAD entities — signal removal then skip caching
		if (NewState.Status == TEXT("DEAD"))
		{
			if (EntityCache.Contains(NewState.Id))
			{
				EntityCache.Remove(NewState.Id);
				OnEntityRemoved.Broadcast(NewState.Id);
			}
			continue;
		}

		SeenIds.Add(NewState.Id);

		if (!EntityCache.Contains(NewState.Id))
		{
			EntityCache.Add(NewState.Id, NewState);
			OnEntityAdded.Broadcast(NewState);
		}
		else
		{
			EntityCache[NewState.Id] = NewState;
			OnEntityUpdated.Broadcast(NewState);
		}
	}

	LastEntityCount = EntityCache.Num();
}

FXBotEntityState UXBotsSubsystem::ParseEntityJson(const TSharedPtr<FJsonObject>& EntObj) const
{
	FXBotEntityState S;

	// Helper lambdas for safe field reads
	auto GetStr = [&](const FString& Key) -> FString
	{
		FString Val;
		EntObj->TryGetStringField(Key, Val);
		return Val;
	};
	auto GetDbl = [&](const FString& Key) -> double
	{
		double Val = 0.0;
		EntObj->TryGetNumberField(Key, Val);
		return Val;
	};
	auto GetFlt = [&](const FString& Key) -> float
	{
		double Val = 0.0;
		EntObj->TryGetNumberField(Key, Val);
		return static_cast<float>(Val);
	};
	auto GetBool = [&](const FString& Key) -> bool
	{
		bool Val = false;
		EntObj->TryGetBoolField(Key, Val);
		return Val;
	};

	S.Id                    = GetStr(TEXT("id"));
	S.Model                 = GetStr(TEXT("model"));
	S.Type                  = GetStr(TEXT("type"));
	S.Status                = GetStr(TEXT("status"));
	S.Domain                = GetStr(TEXT("domain"));
	S.Faction               = GetStr(TEXT("faction"));
	S.Callsign              = GetStr(TEXT("callsign"));
	S.Mode                  = GetStr(TEXT("mode"));
	S.Lat                   = GetDbl(TEXT("lat"));
	S.Lon                   = GetDbl(TEXT("lon"));
	S.AltMeters             = GetDbl(TEXT("alt"));   // published as "alt" in meters
	S.HeadingDeg            = GetFlt(TEXT("heading"));
	S.SpeedKts              = GetFlt(TEXT("speed"));
	S.bExternallyControlled = GetBool(TEXT("externally_controlled"));
	S.ControllerGenesisId   = GetStr(TEXT("controller_genesis_id"));

	return S;
}

// -----------------------------------------------------------------------------
// Reconnect logic
// -----------------------------------------------------------------------------

void UXBotsSubsystem::ScheduleReconnect()
{
	if (bIntentionalDisconnect || !GetGameInstance())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("XBotsSubsystem: Scheduling reconnect in %.1f seconds..."), ReconnectIntervalSeconds);
	GetGameInstance()->GetTimerManager().SetTimer(
		ReconnectTimerHandle,
		[this]() { Connect(); },
		ReconnectIntervalSeconds,
		false   // one-shot; Connect() will re-arm on next failure
	);
}
