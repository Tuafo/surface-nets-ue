#include "PlanetActor.h"
#include "OctreeComponent.h"
#include "NoiseGenerator.h"
#include "PlanetChunk.h"
#include "SurfaceNetsUE.h"
#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/Engine.h"

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
    
    // Initialize parameters
    PlanetRadius = 1000.0f;
    MaxMeshComponents = 100;
    bEnableCollision = true;
    MeshUpdateFrequency = 0.2f;
    MeshUpdateTimer = 0.0f;
    PlanetMaterial = nullptr;
}

void APlanetActor::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize the planet
    InitializePlanet();
}

void APlanetActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    MeshUpdateTimer += DeltaTime;
    
    if (MeshUpdateTimer >= MeshUpdateFrequency)
    {
        MeshUpdateTimer = 0.0f;
        UpdatePlanetMeshes();
    }
}

void APlanetActor::InitializePlanet()
{
    if (!OctreeComponent || !NoiseGenerator)
    {
        UE_LOG(LogSurfaceNets, Error, TEXT("Missing required components for planet initialization"));
        return;
    }
    
    // Set up noise generator
    NoiseGenerator->PlanetRadius = PlanetRadius;
    
    // Set noise generator in octree
    OctreeComponent->SetNoiseGenerator(NoiseGenerator);
    
    // Initialize octree
    OctreeComponent->InitializeOctree(GetActorLocation());
    
    // Pre-create some mesh components
    for (int32 i = 0; i < FMath::Min(MaxMeshComponents / 4, 25); i++)
    {
        UProceduralMeshComponent* MeshComp = CreateMeshComponent();
        AvailableMeshComponents.Add(MeshComp);
    }
    
    UE_LOG(LogSurfaceNets, Log, TEXT("Planet initialized with radius %f"), PlanetRadius);
}

void APlanetActor::UpdatePlanetMeshes()
{
    if (!OctreeComponent)
    {
        return;
    }
    
    // Get visible chunks from octree
    TArray<FPlanetChunk*> VisibleChunks;
    OctreeComponent->GetVisibleChunks(VisibleChunks);
    
    // Clear all currently used mesh components
    ClearUnusedMeshComponents();
    
    // Update meshes for visible chunks
    for (FPlanetChunk* Chunk : VisibleChunks)
    {
        if (Chunk && Chunk->bIsGenerated && Chunk->Vertices.Num() > 0)
        {
            UProceduralMeshComponent* MeshComp = GetMeshComponent();
            if (MeshComp)
            {
                UpdateChunkMesh(Chunk, MeshComp);
                UsedMeshComponents.Add(MeshComp);
            }
        }
    }
    
    UE_LOG(LogSurfaceNets, Verbose, TEXT("Updated %d chunk meshes"), VisibleChunks.Num());
}

UProceduralMeshComponent* APlanetActor::GetMeshComponent()
{
    // Try to get from available pool
    if (AvailableMeshComponents.Num() > 0)
    {
        UProceduralMeshComponent* MeshComp = AvailableMeshComponents.Pop();
        return MeshComp;
    }
    
    // Create new one if pool is empty and we haven't hit the limit
    if (MeshComponents.Num() < MaxMeshComponents)
    {
        return CreateMeshComponent();
    }
    
    // No components available
    return nullptr;
}

void APlanetActor::ReturnMeshComponent(UProceduralMeshComponent* MeshComponent)
{
    if (MeshComponent)
    {
        // Clear the mesh and hide it
        MeshComponent->ClearAllMeshSections();
        MeshComponent->SetVisibility(false);
        
        // Return to available pool
        AvailableMeshComponents.AddUnique(MeshComponent);
        UsedMeshComponents.Remove(MeshComponent);
    }
}

UProceduralMeshComponent* APlanetActor::CreateMeshComponent()
{
    FString CompName = FString::Printf(TEXT("PlanetMesh_%d"), MeshComponents.Num());
    UProceduralMeshComponent* MeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(*CompName);
    
    if (MeshComp)
    {
        MeshComp->SetupAttachment(RootComponent);
        
        // Set material if available
        if (PlanetMaterial)
        {
            MeshComp->SetMaterial(0, PlanetMaterial);
        }
        
        // Configure collision
        if (bEnableCollision)
        {
            MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            MeshComp->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
        }
        else
        {
            MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        
        MeshComponents.Add(MeshComp);
    }
    
    return MeshComp;
}

void APlanetActor::UpdateChunkMesh(FPlanetChunk* Chunk, UProceduralMeshComponent* MeshComponent)
{
    if (!Chunk || !MeshComponent || !Chunk->bIsGenerated)
    {
        return;
    }
    
    // Clear existing mesh sections
    MeshComponent->ClearAllMeshSections();
    
    // Create mesh section with chunk data
    if (Chunk->Vertices.Num() > 0 && Chunk->Triangles.Num() > 0)
    {
        TArray<FLinearColor> VertexColors; // Empty for now
        TArray<FProcMeshTangent> Tangents;  // Empty for now
        
        MeshComponent->CreateMeshSection_LinearColor(
            0,                          // Section index
            Chunk->Vertices,           // Vertices
            Chunk->Triangles,          // Triangles
            Chunk->Normals,            // Normals
            Chunk->UVs,                // UVs
            VertexColors,              // Vertex colors
            Tangents,                  // Tangents
            bEnableCollision           // Create collision
        );
        
        // Set world position
        FVector ChunkWorldPos = Chunk->Position;
        MeshComponent->SetWorldLocation(ChunkWorldPos);
        MeshComponent->SetVisibility(true);
    }
}

void APlanetActor::ClearUnusedMeshComponents()
{
    // Return all currently used mesh components to the pool
    for (UProceduralMeshComponent* MeshComp : UsedMeshComponents)
    {
        ReturnMeshComponent(MeshComp);
    }
    
    UsedMeshComponents.Empty();
}