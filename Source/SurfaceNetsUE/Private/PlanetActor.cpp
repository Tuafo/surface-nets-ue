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
}

void APlanetActor::BeginPlay()
{
    Super::BeginPlay();
    InitializePlanet();
}

void APlanetActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    LODUpdateTimer += DeltaTime;
    if (LODUpdateTimer >= LODUpdateInterval)
    {
        LODUpdateTimer = 0.0f;

        // Get camera position
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            FVector CameraLocation;
            FRotator CameraRotation;
            PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
            UpdatePlanetLOD(CameraLocation);
        }
    }
}

void APlanetActor::InitializePlanet()
{
    if (OctreeComponent && NoiseGenerator)
    {
        OctreeComponent->InitializeOctree(GetActorLocation());
        
        // Pre-create some mesh components
        for (int32 i = 0; i < 50; i++)
        {
            UProceduralMeshComponent* MeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(
                *FString::Printf(TEXT("PlanetMesh_%d"), i)
            );
            MeshComp->SetupAttachment(RootComponent);
            MeshComp->SetMaterial(0, PlanetMaterial);
            MeshComp->SetVisibility(false);
            MeshComponentPool.Add(MeshComp);
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

    TArray<FPlanetChunk*> ActiveChunks = OctreeComponent->GetActiveChunks();
    TSet<FPlanetChunk*> NewActiveChunks(ActiveChunks);

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

    // Add meshes for new active chunks
    for (auto& Chunk : ActiveChunks)
    {
        if (!ActiveChunkMeshes.Contains(Chunk))
        {
            UProceduralMeshComponent* MeshComponent = GetMeshFromPool();
            if (MeshComponent)
            {
                ActiveChunkMeshes.Add(Chunk, MeshComponent);
                GenerateChunkMesh(Chunk, MeshComponent);
            }
        }
    }
}

UProceduralMeshComponent* APlanetActor::GetMeshFromPool()
{
    for (UProceduralMeshComponent* MeshComp : MeshComponentPool)
    {
        if (!MeshComp->IsVisible())
        {
            MeshComp->SetVisibility(true);
            return MeshComp;
        }
    }

    // If no available mesh component, create a new one
    UProceduralMeshComponent* NewMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(
        *FString::Printf(TEXT("PlanetMesh_%d"), MeshComponentPool.Num())
    );
    NewMeshComp->SetupAttachment(RootComponent);
    NewMeshComp->SetMaterial(0, PlanetMaterial);
    MeshComponentPool.Add(NewMeshComp);
    
    return NewMeshComp;
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
    if (!Chunk->bIsGenerated)
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
            true // Create collision
        );
    }
}