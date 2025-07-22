#include "PlanetActor.h"
#include "NoiseGenerator.h"
#include "PlanetChunk.h"
#include "SurfaceNets.h"
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogSurfaceNets, Log, All);

APlanetActor::APlanetActor()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Create root component
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
    RootComponent = RootSceneComponent;
    
    // Create noise generator
    NoiseGenerator = CreateDefaultSubobject<UNoiseGenerator>(TEXT("NoiseGenerator"));
    
    // Initialize parameters - adjusted for proper sphere generation
    PlanetRadius = 1000.0f;
    ChunkSize = 64.0f;  // Smaller chunks for better detail
    ChunksPerAxis = 16; // More chunks to cover sphere properly
    VoxelsPerChunk = 16; // Base resolution, will be 18x18x18 with padding
    bEnableCollision = true;
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
}

void APlanetActor::InitializePlanet()
{
    if (!NoiseGenerator)
    {
        UE_LOG(LogSurfaceNets, Error, TEXT("Missing noise generator for planet initialization"));
        return;
    }
    
    // Set up noise generator with actor's world position as planet center
    FVector ActorPosition = GetActorLocation();
    NoiseGenerator->PlanetRadius = PlanetRadius;
    NoiseGenerator->PlanetCenter = ActorPosition;
    
    // Generate all chunks immediately
    GenerateAllChunks();
    
    UE_LOG(LogSurfaceNets, Log, TEXT("Planet initialized at %s with radius %f and %d chunks"), 
           *ActorPosition.ToString(), PlanetRadius, PlanetChunks.Num());
}

void APlanetActor::GenerateAllChunks()
{
    // Clear existing chunks and mesh components
    PlanetChunks.Empty();
    
    // Destroy existing mesh components
    for (UProceduralMeshComponent* MeshComp : MeshComponents)
    {
        if (MeshComp && IsValid(MeshComp))
        {
            MeshComp->DestroyComponent();
        }
    }
    MeshComponents.Empty();
    
    // Calculate chunk bounds to cover sphere properly
    float TotalSize = ChunksPerAxis * ChunkSize;
    float HalfSize = TotalSize * 0.5f;
    FVector StartPosition = GetActorLocation() - FVector(HalfSize, HalfSize, HalfSize);
    
    // Generate chunks in a grid pattern
    int32 GeneratedChunks = 0;
    for (int32 X = 0; X < ChunksPerAxis; X++)
    {
        for (int32 Y = 0; Y < ChunksPerAxis; Y++)
        {
            for (int32 Z = 0; Z < ChunksPerAxis; Z++)
            {
                // Calculate chunk center
                FVector ChunkCenter = StartPosition + FVector(
                    (X * ChunkSize) + (ChunkSize * 0.5f),
                    (Y * ChunkSize) + (ChunkSize * 0.5f),
                    (Z * ChunkSize) + (ChunkSize * 0.5f)
                );
                
                // Only generate chunks that might intersect the sphere
                float DistanceToSphere = FVector::Dist(ChunkCenter, GetActorLocation());
                float ChunkRadius = ChunkSize * 1.732f; // sqrt(3) for diagonal
                
                // Generate chunk if it might contain part of the sphere surface
                if (DistanceToSphere < PlanetRadius + ChunkRadius && 
                    DistanceToSphere > PlanetRadius - ChunkRadius)
                {
                    GenerateChunk(X, Y, Z, ChunkCenter);
                    GeneratedChunks++;
                }
            }
        }
    }
    
    UE_LOG(LogSurfaceNets, Log, TEXT("Generated %d chunks for sphere (out of %d total grid positions)"), 
           GeneratedChunks, ChunksPerAxis * ChunksPerAxis * ChunksPerAxis);
}

void APlanetActor::GenerateChunk(int32 X, int32 Y, int32 Z, const FVector& ChunkCenter)
{
    // Create chunk with proper LOD level
    TUniquePtr<FPlanetChunk> NewChunk = MakeUnique<FPlanetChunk>(ChunkCenter, 0, ChunkSize);
    
    // Set the base voxel resolution (will be padded internally)
    NewChunk->VoxelResolution = VoxelsPerChunk;
    
    // Generate mesh using the existing FPlanetChunk::GenerateMesh method
    // This should handle boundary padding internally
    NewChunk->GenerateMesh(NoiseGenerator);
    
    // Only create mesh component if chunk has valid mesh data
    if (NewChunk->bIsGenerated && NewChunk->Vertices.Num() > 0 && NewChunk->Triangles.Num() > 0)
    {
        // Create mesh component
        UProceduralMeshComponent* MeshComponent = CreateMeshComponent();
        
        // Create mesh section using the chunk's existing mesh data
        TArray<FColor> VertexColors;
        TArray<FProcMeshTangent> Tangents;
        
        // Apply the chunk's world position offset to vertices
        TArray<FVector> WorldVertices = NewChunk->Vertices;
        // Vertices should already be in world coordinates from the chunk generation
        
        MeshComponent->CreateMeshSection(
            0,
            WorldVertices,
            NewChunk->Triangles,
            NewChunk->Normals,
            NewChunk->UVs,
            VertexColors,
            Tangents,
            bEnableCollision
        );
        
        // Apply material
        if (PlanetMaterial)
        {
            MeshComponent->SetMaterial(0, PlanetMaterial);
        }
        
        // Store mesh component reference
        MeshComponents.Add(MeshComponent);
        
        UE_LOG(LogSurfaceNets, Verbose, TEXT("Generated chunk at (%d,%d,%d) with %d vertices, %d triangles"), 
               X, Y, Z, WorldVertices.Num(), NewChunk->Triangles.Num() / 3);
    }
    
    PlanetChunks.Add(MoveTemp(NewChunk));
}

UProceduralMeshComponent* APlanetActor::CreateMeshComponent()
{
    // Create component at runtime using NewObject
    UProceduralMeshComponent* MeshComponent = NewObject<UProceduralMeshComponent>(this);
    
    // Attach to root component
    MeshComponent->SetupAttachment(RootComponent);
    
    // Set collision
    MeshComponent->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    
    // Register the component with the world
    if (GetWorld())
    {
        MeshComponent->RegisterComponent();
    }
    
    return MeshComponent;
}