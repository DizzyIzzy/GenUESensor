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
}

void AGenesisOSCReceiver::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up background Python Script process when the actor is destroyed
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
	}
	Super::EndPlay(EndPlayReason);
}

void AGenesisOSCReceiver::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Push geo position to Cesium world origin
	if (bGeoUpdated && IsValid(GeoreferenceTarget))
	{
		GeoreferenceTarget->SetOriginLongitudeLatitudeHeight(
			static_cast<double>(Longitude),
			static_cast<double>(Latitude),
			static_cast<double>(AltitudeMSL));
		bGeoUpdated = false;
		UE_LOG(LogTemp, Verbose, TEXT("GenesisOSCReceiver: Georeference updated -> Lat:%.6f Lon:%.6f Alt:%.2f"), Latitude, Longitude, AltitudeMSL);
	}

	// Push offset + rotation to the camera actor
	if (bCameraUpdated && IsValid(Camera1Actor))
	{
		Camera1Actor->SetActorLocation(Camera1Offset);
		Camera1Actor->SetActorRotation(Camera1Rotation);
		bCameraUpdated = false;
		UE_LOG(LogTemp, Verbose, TEXT("GenesisOSCReceiver: Camera1 updated -> Offset:%s Rot:%s"), *Camera1Offset.ToString(), *Camera1Rotation.ToString());
	}
}

void AGenesisOSCReceiver::OnOSCMessageReceived(const FOSCMessage& Message, const FString& IPAddress, int32 Port)
{
	FString Address = UOSCManager::GetOSCMessageAddress(Message).GetFullPath();

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