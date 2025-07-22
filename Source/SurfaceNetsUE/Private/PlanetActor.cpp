#include "PlanetActor.h"
#include "PlanetChunk.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"

APlanetActor::APlanetActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create root component
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
    RootComponent = RootSceneComponent;

    // Create octree component
    OctreeComponent = CreateDefaultSubobject<UOctreeComponent>(TEXT("OctreeComponent"));

    // Create noise generator
    NoiseGenerator = CreateDefaultSubobject<UNoiseGenerator>(TEXT("NoiseGenerator"));

    LODUpdateTimer = 0.0f;

    // Pre-create mesh components in the constructor (reduced from 50 to 30 for performance)
    for (int32 i = 0; i < 30; i++)
    {
        UProceduralMeshComponent* MeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(
            *FString::Printf(TEXT("PlanetMesh_%d"), i)
        );
        MeshComp->SetupAttachment(RootComponent);
        MeshComp->SetVisibility(false);
        MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Disable collision for performance
        MeshComponentPool.Add(MeshComp);
    }
}

void APlanetActor::BeginPlay()
{
    Super::BeginPlay();
    InitializePlanet();
}

void APlanetActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Performance monitoring
    if (bEnablePerformanceLogging)
    {
        LastFrameTime = DeltaTime * 1000.0f; // Convert to milliseconds
    }

    // Reset per-frame counters
    ChunksGeneratedThisFrame = 0;

    if (!bLODUpdatesEnabled)
    {
        return;
    }

    LODUpdateTimer += DeltaTime;
    
    // Process pending chunks every frame but limit the number
    ProcessPendingChunks();

    // Only update LOD periodically or when camera moves significantly
    if (LODUpdateTimer >= LODUpdateInterval)
    {
        LODUpdateTimer = 0.0f;

        // Get camera position
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            FVector CameraLocation;
            FRotator CameraRotation;
            PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
            
            if (ShouldUpdateLOD(CameraLocation))
            {
                UpdatePlanetLOD(CameraLocation);
                LastCameraPosition = CameraLocation;
            }
        }
    }

    // Performance logging
    if (bEnablePerformanceLogging && GEngine)
    {
        FString DebugString = FString::Printf(
            TEXT("Planet Performance: Frame: %.2fms | Active Chunks: %d | Pending: %d | Generated This Frame: %d"),
            LastFrameTime,
            ActiveChunkMeshes.Num(),
            PendingChunks.IsEmpty() ? 0 : 1, // Approximation since TQueue doesn't have size
            ChunksGeneratedThisFrame
        );
        GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, DebugString);
    }
}

bool APlanetActor::ShouldUpdateLOD(const FVector& CameraPosition)
{
    // Check if camera has moved significantly
    float MovementDistance = FVector::Dist(CameraPosition, LastCameraPosition);
    return MovementDistance > MinCameraMovementForUpdate;
}

void APlanetActor::InitializePlanet()
{
    if (OctreeComponent && NoiseGenerator)
    {
        OctreeComponent->InitializeOctree(GetActorLocation());
        
        // Set materials for pre-created mesh components
        for (UProceduralMeshComponent* MeshComp : MeshComponentPool)
        {
            if (MeshComp && PlanetMaterial)
            {
                MeshComp->SetMaterial(0, PlanetMaterial);
            }
        }

        // Clear any existing state
        ActiveChunkMeshes.Empty();
        while (!PendingChunks.IsEmpty())
        {
            TSharedPtr<FPlanetChunk> DummyChunk;
            PendingChunks.Dequeue(DummyChunk);
        }

        UE_LOG(LogTemp, Log, TEXT("Planet initialized successfully"));
    }
}

void APlanetActor::SetLODUpdatesEnabled(bool bEnabled)
{
    bLODUpdatesEnabled = bEnabled;
    
    if (!bEnabled)
    {
        // Clear pending work when disabling
        while (!PendingChunks.IsEmpty())
        {
            TSharedPtr<FPlanetChunk> DummyChunk;
            PendingChunks.Dequeue(DummyChunk);
        }
    }
}

void APlanetActor::UpdatePlanetLOD(const FVector& CameraPosition)
{
    if (!OctreeComponent)
    {
        return;
    }

    // Update octree LOD
    OctreeComponent->UpdateLOD(CameraPosition);

    // Update chunk meshes
    UpdateChunkMeshes();
}

void APlanetActor::UpdateChunkMeshes()
{
    if (!OctreeComponent)
    {
        return;
    }

    // Get active chunks as shared pointers
    TArray<TSharedPtr<FPlanetChunk>> ActiveChunkPtrs = OctreeComponent->GetActiveChunks();
    
    // Limit the number of active chunks for performance
    if (ActiveChunkPtrs.Num() > MaxActiveChunks)
    {
        // Sort by distance and keep only the closest ones
        ActiveChunkPtrs.Sort([](const TSharedPtr<FPlanetChunk>& A, const TSharedPtr<FPlanetChunk>& B) {
            return A->DistanceFromCamera < B->DistanceFromCamera;
        });
        ActiveChunkPtrs.SetNum(MaxActiveChunks);
    }
    
    // Convert to raw pointers for the mesh mapping
    TArray<FPlanetChunk*> ActiveChunks;
    TSet<FPlanetChunk*> NewActiveChunks;
    
    for (const auto& ChunkPtr : ActiveChunkPtrs)
    {
        if (ChunkPtr.IsValid())
        {
            FPlanetChunk* Chunk = ChunkPtr.Get();
            ActiveChunks.Add(Chunk);
            NewActiveChunks.Add(Chunk);
        }
    }

    // Remove meshes for chunks that are no longer active
    TArray<FPlanetChunk*> ChunksToRemove;
    for (auto& Pair : ActiveChunkMeshes)
    {
        if (!NewActiveChunks.Contains(Pair.Key))
        {
            ChunksToRemove.Add(Pair.Key);
            ReturnMeshToPool(Pair.Value);
        }
    }

    for (auto& ChunkToRemove : ChunksToRemove)
    {
        ActiveChunkMeshes.Remove(ChunkToRemove);
    }

    // Add new chunks to pending queue instead of processing immediately
    for (auto& Chunk : ActiveChunks)
    {
        if (!ActiveChunkMeshes.Contains(Chunk))
        {
            // Find the shared pointer for this chunk
            for (const auto& ChunkPtr : ActiveChunkPtrs)
            {
                if (ChunkPtr.Get() == Chunk)
                {
                    PendingChunks.Enqueue(ChunkPtr);
                    break;
                }
            }
        }
    }
}

void APlanetActor::ProcessPendingChunks()
{
    int32 ProcessedThisFrame = 0;
    
    while (!PendingChunks.IsEmpty() && ProcessedThisFrame < MaxChunksPerFrame)
    {
        TSharedPtr<FPlanetChunk> ChunkPtr;
        PendingChunks.Dequeue(ChunkPtr);
        
        if (!ChunkPtr.IsValid())
        {
            continue;
        }
        
        FPlanetChunk* Chunk = ChunkPtr.Get();
        
        // Check if this chunk is still needed
        if (ActiveChunkMeshes.Contains(Chunk))
        {
            continue;
        }
        
        UProceduralMeshComponent* MeshComponent = GetMeshFromPool();
        if (MeshComponent)
        {
            ActiveChunkMeshes.Add(Chunk, MeshComponent);
            GenerateChunkMesh(Chunk, MeshComponent);
            ProcessedThisFrame++;
            ChunksGeneratedThisFrame++;
        }
        else
        {
            // No available mesh components, put chunk back in queue
            PendingChunks.Enqueue(ChunkPtr);
            break;
        }
    }
}

UProceduralMeshComponent* APlanetActor::GetMeshFromPool()
{
    // Find an available mesh component from the pre-created pool
    for (UProceduralMeshComponent* MeshComp : MeshComponentPool)
    {
        if (MeshComp && !MeshComp->IsVisible())
        {
            MeshComp->SetVisibility(true);
            return MeshComp;
        }
    }

    // If no available mesh component, we've run out of pool
    if (bEnablePerformanceLogging)
    {
        UE_LOG(LogTemp, Warning, TEXT("APlanetActor: Ran out of mesh components in pool. Consider increasing pool size."));
    }
    return nullptr;
}

void APlanetActor::ReturnMeshToPool(UProceduralMeshComponent* MeshComponent)
{
    if (MeshComponent)
    {
        MeshComponent->ClearAllMeshSections();
        MeshComponent->SetVisibility(false);
    }
}

void APlanetActor::GenerateChunkMesh(FPlanetChunk* Chunk, UProceduralMeshComponent* MeshComponent)
{
    if (!Chunk || !MeshComponent || !NoiseGenerator)
    {
        return;
    }

    // Generate mesh if not already generated
    if (!Chunk->bIsGenerated && !Chunk->bIsGenerating)
    {
        Chunk->GenerateMesh(NoiseGenerator);
    }

    // Create mesh section
    if (Chunk->Vertices.Num() > 0 && Chunk->Triangles.Num() > 0)
    {
        MeshComponent->CreateMeshSection(
            0,
            Chunk->Vertices,
            Chunk->Triangles,
            Chunk->Normals,
            Chunk->UVs,
            TArray<FColor>(),
            TArray<FProcMeshTangent>(),
            false // Disable collision for performance
        );
    }
}