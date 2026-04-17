#include "XBotsEntityManager.h"
#include "XBotsSubsystem.h"
#include "XBotEntityActor.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

// ============================================================
// UWorldSubsystem / UTickableWorldSubsystem boilerplate
// ============================================================

void UXBotsEntityManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UGameInstance* GI = GetWorld()->GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("XBotsEntityManager: No GameInstance — cannot bind subsystem."));
		return;
	}

	UXBotsSubsystem* XBS = GI->GetSubsystem<UXBotsSubsystem>();
	if (!XBS)
	{
		UE_LOG(LogTemp, Error, TEXT("XBotsEntityManager: UXBotsSubsystem not found."));
		return;
	}

	XBotsSubsystemRef = XBS;

	XBS->OnEntityAdded.AddDynamic(this, &UXBotsEntityManager::HandleEntityAdded);
	XBS->OnEntityUpdated.AddDynamic(this, &UXBotsEntityManager::HandleEntityUpdated);
	XBS->OnEntityRemoved.AddDynamic(this, &UXBotsEntityManager::HandleEntityRemoved);

	UE_LOG(LogTemp, Log, TEXT("XBotsEntityManager: Initialized and bound to XBotsSubsystem."));
}

void UXBotsEntityManager::Deinitialize()
{
	if (UXBotsSubsystem* XBS = XBotsSubsystemRef.Get())
	{
		XBS->OnEntityAdded.RemoveDynamic(this, &UXBotsEntityManager::HandleEntityAdded);
		XBS->OnEntityUpdated.RemoveDynamic(this, &UXBotsEntityManager::HandleEntityUpdated);
		XBS->OnEntityRemoved.RemoveDynamic(this, &UXBotsEntityManager::HandleEntityRemoved);
	}

	// Destroy all live actors
	for (auto& Pair : LiveActors)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}
	LiveActors.Empty();

	Super::Deinitialize();
}

void UXBotsEntityManager::Tick(float DeltaTime)
{
	if (!bEnableDistanceLOD || LiveActors.IsEmpty())
	{
		return;
	}

	// Get camera location in UE world space for distance computations
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const float NearThresholdCm = LodNearDistanceKm * 1000.f * 100.f; // km → cm
	const float MidThresholdCm  = LodMidDistanceKm  * 1000.f * 100.f;

	for (auto& Pair : LiveActors)
	{
		AXBotEntityActor* Actor = Pair.Value;
		if (!IsValid(Actor))
		{
			continue;
		}

		const float Dist = FVector::Dist(Actor->GetActorLocation(), CamLoc);
		EXBotLODTier NewTier;

		if (Dist <= NearThresholdCm)
		{
			NewTier = EXBotLODTier::Near;
		}
		else if (Dist <= MidThresholdCm)
		{
			NewTier = EXBotLODTier::Mid;
		}
		else
		{
			NewTier = EXBotLODTier::Far;
		}

		Actor->SetLODTier(NewTier);
	}
}

TStatId UXBotsEntityManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UXBotsEntityManager, STATGROUP_Tickables);
}

// ============================================================
// Blueprint-callable setters
// ============================================================

void UXBotsEntityManager::SetEntityLabelsVisible(bool bVisible)
{
	bShowEntityLabels = bVisible;
	for (auto& Pair : LiveActors)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->SetLabelVisible(bVisible);
		}
	}
}

// ============================================================
// Delegate handlers
// ============================================================

void UXBotsEntityManager::HandleEntityAdded(const FXBotEntityState& State)
{
	// Domain filter
	if ((!bShowAir  && State.Domain == TEXT("AIR"))  ||
		(!bShowSea  && State.Domain == TEXT("SEA"))  ||
		(!bShowLand && State.Domain == TEXT("LAND")))
	{
		return;
	}

	if (LiveActors.Contains(State.Id))
	{
		// Shouldn't normally happen — treat as update
		HandleEntityUpdated(State);
		return;
	}

	AXBotEntityActor* NewActor = SpawnActorForEntity(State);
	if (!NewActor)
	{
		return;
	}

	NewActor->EntityId               = State.Id;
	NewActor->SmoothingDelaySeconds  = EntitySmoothingDelay;
	NewActor->SetLabelVisible(bShowEntityLabels);
	NewActor->EnqueueState(State);
	ApplyMeshToActor(NewActor, State.Model);

	LiveActors.Add(State.Id, NewActor);
	LiveActorCount = LiveActors.Num();

	UE_LOG(LogTemp, Log, TEXT("XBotsEntityManager: Spawned actor for [%s] model=%s domain=%s"),
		*State.Id, *State.Model, *State.Domain);
}

void UXBotsEntityManager::HandleEntityUpdated(const FXBotEntityState& State)
{
	AXBotEntityActor** ActorPtr = LiveActors.Find(State.Id);
	if (!ActorPtr || !IsValid(*ActorPtr))
	{
		// Actor was filtered out on add, or has been destroyed — ignore
		return;
	}

	(*ActorPtr)->EnqueueState(State);
}

void UXBotsEntityManager::HandleEntityRemoved(const FString& EntityId)
{
	AXBotEntityActor* Actor = nullptr;
	if (LiveActors.RemoveAndCopyValue(EntityId, Actor))
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
		LiveActorCount = LiveActors.Num();
		UE_LOG(LogTemp, Log, TEXT("XBotsEntityManager: Removed actor for entity [%s]"), *EntityId);
	}
}

// ============================================================
// Spawn helper
// ============================================================

AXBotEntityActor* UXBotsEntityManager::SpawnActorForEntity(const FXBotEntityState& State)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn at world origin — position will be applied by actor's first Tick
	// once the snapshot buffer has data
	AXBotEntityActor* Actor = GetWorld()->SpawnActor<AXBotEntityActor>(
		AXBotEntityActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		Params);

	return Actor;
}

// ============================================================
// Mesh / Data Table lookup
// ============================================================

void UXBotsEntityManager::ApplyMeshToActor(AXBotEntityActor* Actor, const FString& ModelName)
{
	if (!Actor)
	{
		return;
	}

	UDataTable* Table = ModelCatalogTable.LoadSynchronous();
	if (!Table)
	{
		// No catalog assigned — apply fallback mesh directly
		UStaticMesh* Fallback = FallbackMesh.LoadSynchronous();
		if (Fallback)
		{
			Actor->MeshComp->SetStaticMesh(Fallback);
		}
		return;
	}

	const FXBotModelMappingRow* Row =
		Table->FindRow<FXBotModelMappingRow>(FName(*ModelName), TEXT("XBotsEntityManager"));

	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("XBotsEntityManager: No mesh entry for model '%s' — using fallback mesh."), *ModelName);
		UStaticMesh* Fallback = FallbackMesh.LoadSynchronous();
		if (Fallback)
		{
			Actor->MeshComp->SetStaticMesh(Fallback);
		}
		return;
	}

	UStaticMesh* Mesh = Row->Mesh.LoadSynchronous();
	if (Mesh)
	{
		Actor->MeshComp->SetStaticMesh(Mesh);
		Actor->MeshComp->SetRelativeScale3D(Row->Scale);
	}
}

EXBotLODTier UXBotsEntityManager::ComputeLODTier(AXBotEntityActor* Actor) const
{
	// This helper is unused — LOD computation is inlined in Tick for efficiency.
	// Kept for potential Blueprint override paths.
	return EXBotLODTier::Near;
}
