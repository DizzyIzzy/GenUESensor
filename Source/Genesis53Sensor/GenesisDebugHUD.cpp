// GenesisDebugHUD.cpp - Debug HUD implementation

#include "GenesisDebugHUD.h"
#include "GenesisOSCReceiver.h"
#include "XBotsSubsystem.h"
#include "XBotsEntityManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

AGenesisDebugHUD::AGenesisDebugHUD()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGenesisDebugHUD::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("GenesisDebugHUD: BeginPlay called - HUD is spawning!"));
	UE_LOG(LogTemp, Warning, TEXT("GenesisDebugHUD: bShowGenesisDebug = %s"), bShowGenesisDebug ? TEXT("TRUE") : TEXT("FALSE"));

	// Auto-find the OSCReceiver if not set
	if (!IsValid(OSCReceiver))
	{
		for (TActorIterator<AGenesisOSCReceiver> It(GetWorld()); It; ++It)
		{
			OSCReceiver = *It;
			UE_LOG(LogTemp, Warning, TEXT("GenesisDebugHUD: Auto-found OSCReceiver: %s"), *OSCReceiver->GetName());
			break;
		}

		if (!IsValid(OSCReceiver))
		{
			UE_LOG(LogTemp, Error, TEXT("GenesisDebugHUD: Could NOT find GenesisOSCReceiver in level! Add one to your level."));
		}
	}
}

void AGenesisDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!bShowGenesisDebug)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenesisDebugHUD: bShowGenesisDebug is FALSE - toggle it on in Details or set default to true"));
		return;
	}

	if (!IsValid(OSCReceiver))
	{
		// Draw error message on screen
		if (Canvas)
		{
			FCanvasTextItem TextItem(FVector2D(50, 100), FText::FromString(TEXT("ERROR: GenesisOSCReceiver not found in level!")), GEngine->GetLargeFont(), FLinearColor::Red);
			TextItem.bOutlined = true;
			Canvas->DrawItem(TextItem);
		}
		return;
	}

	// Starting position for debug overlay
	float X = 50.0f;
	float Y = 100.0f;
	const float LineHeight = 20.0f * DebugTextScale;

	// Title
	DrawDebugText(TEXT("=== GENESIS MQTT/OSC DEBUG MONITOR ==="), X, Y, FLinearColor::White);
	Y += LineHeight * 0.5f;

	// Enabled/Disabled Status (prominent)
	FString EnabledStatus = OSCReceiver->IsEnabled() 
		? TEXT("✓ RECEIVER ENABLED") 
		: TEXT("✗ RECEIVER DISABLED - Toggle bEnabled to activate");
	FLinearColor EnabledColor = OSCReceiver->IsEnabled() ? NormalColor : ErrorColor;
	DrawDebugText(EnabledStatus, X, Y, EnabledColor);
	Y += LineHeight * 0.5f;

	// Only show connection status if receiver is enabled
	bool bReceivingData = (OSCReceiver->Latitude != 0.0f || 
						   OSCReceiver->Longitude != 0.0f || 
						   OSCReceiver->AltitudeMSL != 0.0f);

	FString ConnectionStatus;
	FLinearColor ConnectionColor;
	if (!OSCReceiver->IsEnabled())
	{
		ConnectionStatus = TEXT("⊝ Receiver Disabled");
		ConnectionColor = FLinearColor::Gray;
	}
	else if (bReceivingData)
	{
		ConnectionStatus = TEXT("✓ CONNECTED - Receiving Data");
		ConnectionColor = NormalColor;
	}
	else
	{
		ConnectionStatus = TEXT("✗ NO DATA - Check Python Bridge & MQTT Broker");
		ConnectionColor = ErrorColor;
	}
	DrawDebugText(ConnectionStatus, X, Y, ConnectionColor);

	DrawDebugText(FString::Printf(TEXT("MQTT Broker: %s:%d"), 
		*OSCReceiver->MQTTBrokerIP, OSCReceiver->MQTTBrokerPort), X, Y, FLinearColor::Gray);
	
	DrawDebugText(FString::Printf(TEXT("OSC Listening: %s:%d"), 
		*OSCReceiver->ReceiveIP, OSCReceiver->ReceivePort), X, Y, FLinearColor::Gray);

	Y += LineHeight * 0.5f;

	// Geospatial Data Section
	DrawDebugText(TEXT("--- GEOSPATIAL POSITION ---"), X, Y, FLinearColor(0.0f, 1.0f, 1.0f));
	
	DrawDebugText(FString::Printf(TEXT("Latitude:     %.6f°"), OSCReceiver->Latitude), 
		X, Y, bReceivingData ? NormalColor : FLinearColor::Gray);
	
	DrawDebugText(FString::Printf(TEXT("Longitude:    %.6f°"), OSCReceiver->Longitude), 
		X, Y, bReceivingData ? NormalColor : FLinearColor::Gray);
	
	DrawDebugText(FString::Printf(TEXT("Altitude MSL: %.2f ft (%.2f m)"), 
		OSCReceiver->AltitudeMSL, OSCReceiver->AltitudeMSL * 0.3048f), 
		X, Y, bReceivingData ? NormalColor : FLinearColor::Gray);

	// Cesium Georeference Status
	bool bGeoreferenceValid = IsValid(OSCReceiver->GeoreferenceTarget);
	FString GeoreferenceStatus = bGeoreferenceValid 
		? FString::Printf(TEXT("✓ Cesium Georeference: %s"), *OSCReceiver->GeoreferenceTarget->GetName())
		: TEXT("✗ WARNING: Cesium Georeference NOT SET");
	DrawDebugText(GeoreferenceStatus, X, Y, bGeoreferenceValid ? NormalColor : WarningColor);

	Y += LineHeight * 0.5f;

	// Camera 1 Data Section
	DrawDebugText(TEXT("--- CAMERA 1 DATA ---"), X, Y, FLinearColor(0.0f, 1.0f, 1.0f));
	
	bool bCameraDataPresent = !OSCReceiver->Camera1Offset.IsNearlyZero() || 
	                          !OSCReceiver->Camera1Rotation.IsNearlyZero();
	
	DrawDebugText(FString::Printf(TEXT("Offset (cm):  X=%.2f Y=%.2f Z=%.2f"), 
		OSCReceiver->Camera1Offset.X, OSCReceiver->Camera1Offset.Y, OSCReceiver->Camera1Offset.Z), 
		X, Y, bCameraDataPresent ? NormalColor : FLinearColor::Gray);
	
	DrawDebugText(FString::Printf(TEXT("Rotation:     Roll=%.2f Pitch=%.2f Yaw=%.2f"), 
		OSCReceiver->Camera1Rotation.Roll, OSCReceiver->Camera1Rotation.Pitch, OSCReceiver->Camera1Rotation.Yaw), 
		X, Y, bCameraDataPresent ? NormalColor : FLinearColor::Gray);

	// Camera Actor Status
	bool bCameraActorValid = IsValid(OSCReceiver->Camera1Actor);
	FString CameraActorStatus = bCameraActorValid 
		? FString::Printf(TEXT("✓ Camera Actor: %s"), *OSCReceiver->Camera1Actor->GetName())
		: TEXT("✗ WARNING: Camera1Actor NOT SET");
	// Show disabled message prominently if receiver is off
	if (!OSCReceiver->IsEnabled())
	{
		DrawDebugText(TEXT("⊝ RECEIVER IS DISABLED"), X, Y, FLinearColor::Gray);
		DrawDebugText(TEXT("  Enable via Details Panel (bEnabled) or Blueprint"), X, Y, FLinearColor::Gray);
	}
	else DrawDebugText(CameraActorStatus, X, Y, bCameraActorValid ? NormalColor : WarningColor);

	Y += LineHeight * 0.5f;

	// Warnings and Fallback Info Section
	DrawDebugText(TEXT("--- SYSTEM STATUS ---"), X, Y, FLinearColor(0.0f, 1.0f, 1.0f));

	// Display latest warning message if present
	if (!OSCReceiver->LastWarningMessage.IsEmpty())
	{
		DrawDebugText(FString::Printf(TEXT("⚠ %s"), *OSCReceiver->LastWarningMessage), 
			X, Y, WarningColor);
	}

	if (!bReceivingData)
	{
		DrawDebugText(TEXT("⚠ NO DATA RECEIVED"), X, Y, ErrorColor);
		DrawDebugText(TEXT("  Troubleshooting:"), X, Y, FLinearColor::Gray);
		DrawDebugText(TEXT("  1. Check Python bridge is running"), X, Y, FLinearColor::Gray);
		DrawDebugText(TEXT("  2. Verify MQTT broker is accessible"), X, Y, FLinearColor::Gray);
		DrawDebugText(TEXT("  3. Check channelList.conf topics"), X, Y, FLinearColor::Gray);
	}
	else if (!bGeoreferenceValid || !bCameraActorValid)
	{
		DrawDebugText(TEXT("⚠ PARTIAL CONFIGURATION"), X, Y, WarningColor);
		DrawDebugText(TEXT("  Please set missing references in OSCReceiver"), X, Y, FLinearColor::Gray);
	}
	else
	{
		DrawDebugText(TEXT("✓ All Systems Operational"), X, Y, NormalColor);
	}

	// Python Bridge Status
	DrawDebugText(FString::Printf(TEXT("Python Auto-Start: %s"), 
		OSCReceiver->bAutoStartPythonBridge ? TEXT("Enabled") : TEXT("Disabled")), 
		X, Y, FLinearColor::Gray);

	Y += LineHeight * 0.5f;

	// ----------------------------------------------------------------
	// xBots Stream Section
	// ----------------------------------------------------------------
	DrawDebugText(TEXT("--- XBOTS STREAM ---"), X, Y, FLinearColor(0.0f, 1.0f, 1.0f));

	UXBotsSubsystem* XBS = nullptr;
	UXBotsEntityManager* XBM = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		XBS = GI->GetSubsystem<UXBotsSubsystem>();
	}
	if (UWorld* W = GetWorld())
	{
		XBM = W->GetSubsystem<UXBotsEntityManager>();
	}

	if (XBS)
	{
		const bool bWSConnected = XBS->bIsConnected;
		DrawDebugText(
			bWSConnected ? TEXT("✓ WebSocket CONNECTED") : TEXT("✗ WebSocket DISCONNECTED"),
			X, Y, bWSConnected ? NormalColor : ErrorColor);

		DrawDebugText(
			FString::Printf(TEXT("Target: ws://%s:%d/ws"), *XBS->XBotsIP, XBS->XBotsWSPort),
			X, Y, FLinearColor::Gray);

		if (bWSConnected)
		{
			const double SecsSinceUpdate = FPlatformTime::Seconds() - XBS->LastUpdateTime;
			DrawDebugText(
				FString::Printf(TEXT("Entities tracked: %d  |  Last update: %.2f s ago"),
					XBS->LastEntityCount, SecsSinceUpdate),
				X, Y, NormalColor);

			// Entity breakdown by domain
			int32 AirCount = 0, SeaCount = 0, LandCount = 0;
			for (const FXBotEntityState& E : XBS->GetAllEntities())
			{
				if (E.Domain == TEXT("AIR"))       ++AirCount;
				else if (E.Domain == TEXT("SEA"))  ++SeaCount;
				else if (E.Domain == TEXT("LAND")) ++LandCount;
			}
			DrawDebugText(
				FString::Printf(TEXT("  AIR: %d  SEA: %d  LAND: %d"), AirCount, SeaCount, LandCount),
				X, Y, FLinearColor::Gray);
		}
	}
	else
	{
		DrawDebugText(TEXT("XBotsSubsystem unavailable"), X, Y, WarningColor);
	}

	if (XBM)
	{
		DrawDebugText(
			FString::Printf(TEXT("Live actors: %d  |  Labels: %s  |  LOD: %s"),
				XBM->LiveActorCount,
				XBM->bShowEntityLabels ? TEXT("ON") : TEXT("OFF"),
				XBM->bEnableDistanceLOD ? TEXT("ON") : TEXT("OFF")),
			X, Y, FLinearColor::Gray);
	}

	// Instructions
	Y += LineHeight;
	DrawDebugText(TEXT("Press ~ to toggle console | Set bShowDebugInfo=false to hide"), 
		X, Y, FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
}

void AGenesisDebugHUD::DrawDebugText(const FString& Text, float X, float& Y, const FLinearColor& Color)
{
	const float LineHeight = 20.0f * DebugTextScale;
	
	if (Canvas)
	{
		FCanvasTextItem TextItem(FVector2D(X, Y), FText::FromString(Text), GEngine->GetMediumFont(), Color);
		TextItem.Scale = FVector2D(DebugTextScale, DebugTextScale);
		TextItem.bOutlined = true;
		Canvas->DrawItem(TextItem);
	}
	
	Y += LineHeight;
}
