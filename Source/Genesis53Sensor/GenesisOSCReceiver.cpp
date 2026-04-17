#include "GenesisOSCReceiver.h"
#include "OSCManager.h"
#include "OSCMessage.h"
#include "Misc/Char.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

AGenesisOSCReceiver::AGenesisOSCReceiver()
{
	PrimaryActorTick.bCanEverTick = true;
	OSCServer = nullptr;
}

void AGenesisOSCReceiver::BeginPlay()
{
	Super::BeginPlay();

	// Only start if enabled
	if (bEnabled)
	{
		StartReceiver();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GenesisOSCReceiver: Starting disabled (bEnabled=false). Use SetEnabled(true) to activate."));
	}
}

void AGenesisOSCReceiver::StartReceiver()
{
	if (bReceiverStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenesisOSCReceiver: Already started, ignoring duplicate start request."));
		return;
	}

	// Create and Initialize the OSC Server
	OSCServer = UOSCManager::CreateOSCServer(ReceiveIP, ReceivePort, false, true, TEXT("GenesisOSCServer"), this);
	if (OSCServer)
	{
		OSCServer->OnOscMessageReceived.AddDynamic(this, &AGenesisOSCReceiver::OnOSCMessageReceived);
		UE_LOG(LogTemp, Log, TEXT("GenesisOSCReceiver: Listening for OSC on %s:%d"), *ReceiveIP, ReceivePort);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GenesisOSCReceiver: Failed to create OSC Server on %s:%d"), *ReceiveIP, ReceivePort);
	}

	// Optionally launch the Python Bridge automatically
	if (bAutoStartPythonBridge)
	{
		FString ProjectDir = FPaths::ProjectContentDir();
		FString PythonExe = FPaths::Combine(*ProjectDir, TEXT("cameraService"), TEXT("PythonClient"), TEXT("venv"), TEXT("Scripts"), TEXT("python.exe"));
		FString PythonScript = FPaths::Combine(*ProjectDir, TEXT("cameraService"), TEXT("PythonClient"), TEXT("mqtt_osc_bridge.py"));

		if (FPaths::FileExists(PythonExe) && FPaths::FileExists(PythonScript))
		{
			// Construct arguments for python script
			FString Args = FString::Printf(TEXT("\"%s\" --broker %s --port %d --osc-ip %s --osc-port %d"), 
				*PythonScript, *MQTTBrokerIP, MQTTBrokerPort, *ReceiveIP, ReceivePort);

			UE_LOG(LogTemp, Log, TEXT("GenesisOSCReceiver: Launching Python Bridge: %s %s"), *PythonExe, *Args);

			// Start the process in the background, hidden
			PythonBridgeProcessHandle = FPlatformProcess::CreateProc(*PythonExe, *Args, true, true, true, nullptr, 0, nullptr, nullptr);
			
			if (!PythonBridgeProcessHandle.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("GenesisOSCReceiver: Failed to launch Python Bridge background process."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GenesisOSCReceiver: Could not find Python venv executable or script at %s. Ensure you have run the python venv creation steps."), *PythonExe);
		}
	}

	bReceiverStarted = true;
	UE_LOG(LogTemp, Log, TEXT("GenesisOSCReceiver: Started successfully."));
}

void AGenesisOSCReceiver::StopReceiver()
{
	if (!bReceiverStarted)
	{
		return; // Already stopped
	}

	// Clean up background Python Script process
	if (PythonBridgeProcessHandle.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("GenesisOSCReceiver: Terminating Python Bridge background process."));
		FPlatformProcess::TerminateProc(PythonBridgeProcessHandle);
		FPlatformProcess::CloseProc(PythonBridgeProcessHandle);
	}

	if (OSCServer)
	{
		OSCServer->OnOscMessageReceived.RemoveDynamic(this, &AGenesisOSCReceiver::OnOSCMessageReceived);
		OSCServer->Stop();
		OSCServer = nullptr;
	}

	bReceiverStarted = false;
	UE_LOG(LogTemp, Log, TEXT("GenesisOSCReceiver: Stopped."));
}

void AGenesisOSCReceiver::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopReceiver();
	Super::EndPlay(EndPlayReason);
}

void AGenesisOSCReceiver::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Skip processing if disabled
	if (!bEnabled)
	{
		return;
	}

	// Update connection health status
	const double CurrentTime = FPlatformTime::Seconds();
	const double TimeSinceLastData = CurrentTime - LastDataReceivedTime;
	bIsReceivingData = (TimeSinceLastData < 5.0); // Consider connected if data within last 5 seconds

	// Push geo position to Cesium world origin
	// AltitudeMSL arrives in feet MSL from the broker — convert to meters for Cesium
	const double AltitudeMeters = static_cast<double>(AltitudeMSL) * 0.3048;
	if (bGeoUpdated && IsValid(GeoreferenceTarget))
	{
		FVector LongLatHeight(
			static_cast<double>(Longitude),
			static_cast<double>(Latitude),
			AltitudeMeters);
		GeoreferenceTarget->SetOriginLongitudeLatitudeHeight(LongLatHeight);
		bGeoUpdated = false;
		UE_LOG(LogTemp, Log, TEXT("GenesisOSCReceiver: Georeference updated -> Lat:%.6f Lon:%.6f AltFt:%.1f AltM:%.1f"), Latitude, Longitude, AltitudeMSL, AltitudeMeters);
	}
	else if (bGeoUpdated)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenesisOSCReceiver: Geo data received but GeoreferenceTarget is NOT set. Assign it in the Details panel."));
		bGeoUpdated = false;
	}

	// Push offset + rotation to the camera actor as a RELATIVE transform
	// so it moves correctly when parented to the platform/georeference hierarchy
	if (bCameraUpdated)
	{
		UE_LOG(LogTemp, Log, TEXT("GenesisOSCReceiver: Camera1 data -> Offset(cm):%s Rot:%s | Actor valid: %s"),
			*Camera1Offset.ToString(), *Camera1Rotation.ToString(), IsValid(Camera1Actor) ? TEXT("YES") : TEXT("NO — assign Camera1Actor in Details panel"));
		if (IsValid(Camera1Actor))
		{
			Camera1Actor->SetActorRelativeLocation(Camera1Offset);
			Camera1Actor->SetActorRelativeRotation(Camera1Rotation);
		}
		bCameraUpdated = false;
	}
}

void AGenesisOSCReceiver::OnOSCMessageReceived(const FOSCMessage& Message, const FString& IPAddress, int32 Port)
{
	FString Address = UOSCManager::GetOSCMessageAddress(Message).GetFullPath();
	UE_LOG(LogTemp, Log, TEXT("GenesisOSCReceiver: OSC packet received from %s:%d | Address: %s"), *IPAddress, Port, *Address);

	// Update last received timestamp
	LastDataReceivedTime = FPlatformTime::Seconds();

	// Check for warning messages (sent as string instead of float)
	FString StringVal;
	if (Address.Equals(TEXT("/genesis/warning"), ESearchCase::IgnoreCase))
	{
		if (UOSCManager::GetString(Message, 0, StringVal))
		{
			LastWarningMessage = StringVal;
			UE_LOG(LogTemp, Warning, TEXT("GenesisOSCReceiver: FALLBACK WARNING: %s"), *StringVal);
		}
		return;
	}

	float FloatVal = 0.0f;
	if (UOSCManager::GetFloat(Message, 0, FloatVal))
	{
		// Map the OSC paths to our float variables
		if (Address.Equals(TEXT("/genesis/latitude"), ESearchCase::IgnoreCase))
		{
			Latitude = FloatVal;
			bGeoUpdated = true;
		}
		else if (Address.Equals(TEXT("/genesis/longitude"), ESearchCase::IgnoreCase))
		{
			Longitude = FloatVal;
			bGeoUpdated = true;
		}
		else if (Address.Equals(TEXT("/genesis/altitudeMSL"), ESearchCase::IgnoreCase))
		{
			AltitudeMSL = FloatVal;
			bGeoUpdated = true;
		}
		// --- Camera 1 Offsets ---
		else if (Address.Equals(TEXT("/genesis/camera1Xoffset"), ESearchCase::IgnoreCase))
		{
			Camera1Offset.X = FloatVal * 100.0f; // Convert meters to cm for Unreal
			bCameraUpdated = true;
		}
		else if (Address.Equals(TEXT("/genesis/camera1Yoffset"), ESearchCase::IgnoreCase))
		{
			Camera1Offset.Y = FloatVal * 100.0f; // Convert meters to cm for Unreal
			bCameraUpdated = true;
		}
		else if (Address.Equals(TEXT("/genesis/camera1Zoffset"), ESearchCase::IgnoreCase))
		{
			Camera1Offset.Z = FloatVal * 100.0f; // Convert meters to cm for Unreal
			bCameraUpdated = true;
		}
		// --- Camera 1 Rotations ---
		else if (Address.Equals(TEXT("/genesis/camera1Xrotation"), ESearchCase::IgnoreCase))
		{
			Camera1Rotation.Roll = FloatVal;
			bCameraUpdated = true;
		}
		else if (Address.Equals(TEXT("/genesis/camera1Yrotation"), ESearchCase::IgnoreCase))
		{
			Camera1Rotation.Pitch = FloatVal;
			bCameraUpdated = true;
		}
		else if (Address.Equals(TEXT("/genesis/camera1Zrotation"), ESearchCase::IgnoreCase))
		{
			Camera1Rotation.Yaw = FloatVal;
			bCameraUpdated = true;
		}
	}
}

void AGenesisOSCReceiver::SetEnabled(bool bNewEnabled)
{
	if (bEnabled == bNewEnabled)
	{
		return; // No change
	}

	bEnabled = bNewEnabled;

	if (bEnabled)
	{
		UE_LOG(LogTemp, Log, TEXT("GenesisOSCReceiver: Enabling receiver..."));
		StartReceiver();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("GenesisOSCReceiver: Disabling receiver..."));
		StopReceiver();
	}
}

void AGenesisOSCReceiver::ToggleEnabled()
{
	SetEnabled(!bEnabled);
	UE_LOG(LogTemp, Log, TEXT("GenesisOSCReceiver: Toggled to %s"), bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
}