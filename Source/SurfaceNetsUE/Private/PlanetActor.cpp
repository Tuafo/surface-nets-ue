#include "PlanetActor.h"
#include "NoiseGenerator.h"
#include "PlanetChunk.h"
#include "SurfaceNetsUE.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"

APlanetActor::APlanetActor()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Create root component
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    
    // Match Rust example parameters more closely
    PlanetRadius = 1000.0f;
    ChunkSize = 128.0f;  // Larger chunks like Rust (16 voxels * 8 = 128 units)
    ChunksPerAxis = 16;
    VoxelsPerChunk = 16; // Same as Rust
    bEnableCollision = false;
    
    // Create noise generator
    NoiseGenerator = CreateDefaultSubobject<UNoiseGenerator>(TEXT("NoiseGenerator"));
}

void APlanetActor::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogSurfaceNets, Log, TEXT("Planet spawned successfully at %s"), *GetActorLocation().ToString());
    
    // CRITICAL FIX: Ensure NoiseGenerator exists before using it
    if (!NoiseGenerator)
    {
        UE_LOG(LogSurfaceNets, Error, TEXT("NoiseGenerator is null, creating new one"));
        NoiseGenerator = NewObject<UNoiseGenerator>(this);
    }
    
    // Initialize planet on begin play
    InitializePlanet();
}

UProceduralMeshComponent* APlanetActor::CreateMeshComponent()
{
    UProceduralMeshComponent* MeshComponent = NewObject<UProceduralMeshComponent>(this);
    MeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
    MeshComponent->RegisterComponent();
    return MeshComponent;
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
    
    // Calculate chunk bounds exactly like Rust implementation
    // Rust uses: chunks_extent = Extent3i::from_min_and_lub(IVec3::from([-5; 3]), IVec3::from([5; 3]))
    // Which creates a 10x10x10 grid centered around origin
    float HalfExtent = (ChunksPerAxis / 2) * ChunkSize;
    FVector PlanetCenter = GetActorLocation();
    FVector StartPosition = PlanetCenter - FVector(HalfExtent, HalfExtent, HalfExtent);
    
    UE_LOG(LogSurfaceNets, Log, TEXT("Generating chunks from %s to %s (ChunkSize: %f)"), 
           *StartPosition.ToString(), 
           *(StartPosition + FVector(ChunksPerAxis * ChunkSize)).ToString(),
           ChunkSize);
    
    // Generate chunks in a grid pattern (equivalent to Rust chunks_extent.iter3())
    int32 GeneratedChunks = 0;
    int32 ProcessedChunks = 0;
    
    for (int32 X = 0; X < ChunksPerAxis; X++)
    {
        for (int32 Y = 0; Y < ChunksPerAxis; Y++)
        {
            for (int32 Z = 0; Z < ChunksPerAxis; Z++)
            {
                ProcessedChunks++;
                
                // Calculate chunk center (equivalent to Rust chunk_min calculation)
                FVector ChunkCenter = StartPosition + FVector(
                    (X * ChunkSize) + (ChunkSize * 0.5f),
                    (Y * ChunkSize) + (ChunkSize * 0.5f),
                    (Z * ChunkSize) + (ChunkSize * 0.5f)
                );
                
                // Debug: Check distance from planet center for a few chunks
                float DistanceFromCenter = (ChunkCenter - PlanetCenter).Size();
                if (ProcessedChunks <= 10 || (ProcessedChunks % 100 == 0))
                {
                    UE_LOG(LogSurfaceNets, Log, TEXT("Chunk (%d,%d,%d) at %s, distance from center: %f (radius: %f)"), 
                           X, Y, Z, *ChunkCenter.ToString(), DistanceFromCenter, PlanetRadius);
                }
                
                // Generate chunk - let the chunk itself determine if it has surface intersection
                if (GenerateChunk(X, Y, Z, ChunkCenter))
                {
                    GeneratedChunks++;
                }
            }
        }
    }
    
    UE_LOG(LogSurfaceNets, Log, TEXT("Generated %d chunks for sphere (out of %d total grid positions)"), 
           GeneratedChunks, ProcessedChunks);
}

bool APlanetActor::GenerateChunk(int32 X, int32 Y, int32 Z, const FVector& ChunkCenter)
{
    // CRITICAL FIX: Null check before passing NoiseGenerator
    if (!NoiseGenerator)
    {
        UE_LOG(LogSurfaceNets, Error, TEXT("NoiseGenerator is null in GenerateChunk"));
        return false;
    }
    
    // Create chunk with proper LOD level
    TUniquePtr<FPlanetChunk> NewChunk = MakeUnique<FPlanetChunk>(ChunkCenter, 0, ChunkSize);
    
    // Generate mesh using the chunk's GenerateMesh method (equivalent to Rust generate_and_process_chunk)
    bool bMeshGenerated = NewChunk->GenerateMesh(NoiseGenerator);
    
    // Only create mesh component if chunk has valid mesh data (like Rust early return)
    if (bMeshGenerated && NewChunk->Vertices.Num() > 0 && NewChunk->Triangles.Num() > 0)
    {
        // Create mesh component
        UProceduralMeshComponent* MeshComponent = CreateMeshComponent();
        
        // Create mesh section using the chunk's mesh data
        TArray<FColor> VertexColors;
        TArray<FProcMeshTangent> Tangents;
        
        MeshComponent->CreateMeshSection(
            0,
            NewChunk->Vertices,
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
        
        UE_LOG(LogSurfaceNets, Log, TEXT("Generated chunk at (%d,%d,%d) with %d vertices, %d triangles"), 
               X, Y, Z, NewChunk->Vertices.Num(), NewChunk->Triangles.Num() / 3);
    }
    else
    {
        // Debug logging for first few failed chunks
        static int32 LoggedFailures = 0;
        if (LoggedFailures < 5)
        {
            UE_LOG(LogSurfaceNets, Warning, TEXT("Chunk at (%d,%d,%d) center %s skipped - no surface intersection"), 
                   X, Y, Z, *ChunkCenter.ToString());
            LoggedFailures++;
        }
    }
    
    // Always store the chunk (even if it has no mesh) for consistency
    PlanetChunks.Add(MoveTemp(NewChunk));
    return bMeshGenerated;
}

void APlanetActor::LogPlanetStats()
{
    int32 TotalVertices = 0;
    int32 TotalTriangles = 0;
    
    for (const auto& Chunk : PlanetChunks)
    {
        if (Chunk.IsValid())
        {
            TotalVertices += Chunk->Vertices.Num();
            TotalTriangles += Chunk->Triangles.Num() / 3;
        }
    }
    
    UE_LOG(LogSurfaceNets, Warning, TEXT("Planet Stats:"));
    UE_LOG(LogSurfaceNets, Warning, TEXT("  Generated Chunks: %d"), PlanetChunks.Num());
    UE_LOG(LogSurfaceNets, Warning, TEXT("  Active Mesh Components: %d"), MeshComponents.Num());
    UE_LOG(LogSurfaceNets, Warning, TEXT("  Total Vertices: %d"), TotalVertices);
    UE_LOG(LogSurfaceNets, Warning, TEXT("  Total Triangles: %d"), TotalTriangles);
    UE_LOG(LogSurfaceNets, Warning, TEXT("  Planet Radius: %f"), PlanetRadius);
    UE_LOG(LogSurfaceNets, Warning, TEXT("  Chunk Size: %f"), ChunkSize);
}