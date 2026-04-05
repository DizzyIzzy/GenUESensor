#include "GenesisOSCReceiver.h"
#include "OSCManager.h"
#include "OSCMessage.h"
#include "Misc/Char.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

AGenesisOSCReceiver::AGenesisOSCReceiver()
{
	PrimaryActorTick.bCanEverTick = false;
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
		}
		else if (Address.Equals(TEXT("/genesis/longitude"), ESearchCase::IgnoreCase))
		{
			Longitude = FloatVal;
		}
		else if (Address.Equals(TEXT("/genesis/altitudeMSL"), ESearchCase::IgnoreCase))
		{
			AltitudeMSL = FloatVal;
		}
		// --- Camera 1 Offsets ---
		else if (Address.Equals(TEXT("/genesis/camera1Xoffset"), ESearchCase::IgnoreCase))
		{
			Camera1Offset.X = FloatVal * 100.0f; // Convert meters to cm for Unreal
		}
		else if (Address.Equals(TEXT("/genesis/camera1Yoffset"), ESearchCase::IgnoreCase))
		{
			Camera1Offset.Y = FloatVal * 100.0f; // Convert meters to cm for Unreal
		}
		else if (Address.Equals(TEXT("/genesis/camera1Zoffset"), ESearchCase::IgnoreCase))
		{
			Camera1Offset.Z = FloatVal * 100.0f; // Convert meters to cm for Unreal
		}
		// --- Camera 1 Rotations ---
		else if (Address.Equals(TEXT("/genesis/camera1Xrotation"), ESearchCase::IgnoreCase))
		{
			Camera1Rotation.Roll = FloatVal;
		}
		else if (Address.Equals(TEXT("/genesis/camera1Yrotation"), ESearchCase::IgnoreCase))
		{
			Camera1Rotation.Pitch = FloatVal;
		}
		else if (Address.Equals(TEXT("/genesis/camera1Zrotation"), ESearchCase::IgnoreCase))
		{
			Camera1Rotation.Yaw = FloatVal;
		}
	}
}