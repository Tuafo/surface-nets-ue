#include "PlanetChunk.h"
#include "NoiseGenerator.h"
#include "SurfaceNets.h"
#include "SurfaceNetsUE.h"

FPlanetChunk::FPlanetChunk()
    : Position(FVector::ZeroVector)
    , LODLevel(0)
    , Size(128.0f)  // Match the larger chunk size
    , VoxelResolution(16)
    , bIsGenerated(false)
    , bIsGenerating(false)
    , DistanceFromCamera(0.0f)
{
}

FPlanetChunk::FPlanetChunk(const FVector& InPosition, int32 InLODLevel, float InSize)
    : Position(InPosition)
    , LODLevel(InLODLevel)
    , Size(InSize)
    , VoxelResolution(GetVoxelResolution())
    , bIsGenerated(false)
    , bIsGenerating(false)
    , DistanceFromCamera(0.0f)
{
}

bool FPlanetChunk::GenerateMesh(const UNoiseGenerator* NoiseGenerator)
{
    if (bIsGenerating || bIsGenerated || !NoiseGenerator)
    {
        return false;
    }
    
    bIsGenerating = true;
    ClearMesh();
    
    // Generate padded density field exactly like Rust implementation
    TArray<float> PaddedDensityField;
    int32 PaddedSize;
    FVector PaddedOrigin;
    float VoxelSize;
    
    bool bHasSurface = GeneratePaddedDensityField(NoiseGenerator, PaddedDensityField, PaddedSize, PaddedOrigin, VoxelSize);
    
    // Early exit if no surface intersection (like Rust version)
    if (!bHasSurface)
    {
        UE_LOG(LogSurfaceNets, VeryVerbose, TEXT("Chunk at %s has no surface intersection - skipping"), *Position.ToString());
        bIsGenerating = false;
        return false;
    }
    
    // Generate mesh using Surface Nets with exact Rust parameters
    FSurfaceNets SurfaceNets;
    
    // Use same parameters as Rust: min=[0,0,0], max=[UNPADDED_SIZE+1, UNPADDED_SIZE+1, UNPADDED_SIZE+1]
    FIntVector MinBounds(0, 0, 0);
    FIntVector MaxBounds(VoxelResolution + 1, VoxelResolution + 1, VoxelResolution + 1);
    
    SurfaceNets.GenerateMesh(PaddedDensityField, PaddedSize, VoxelSize, PaddedOrigin, 
                           Vertices, Triangles, Normals, MinBounds, MaxBounds);
    
    // Generate UVs
    UVs.SetNum(Vertices.Num());
    FVector ChunkMin = Position - FVector(Size * 0.5f);
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        FVector LocalPos = (Vertices[i] - ChunkMin) / Size;
        UVs[i] = FVector2D(LocalPos.X, LocalPos.Y);
    }
    
    UE_LOG(LogSurfaceNets, Log, TEXT("Chunk at %s generated %d vertices, %d triangles"), 
           *Position.ToString(), Vertices.Num(), Triangles.Num() / 3);
    
    bIsGenerated = true;
    bIsGenerating = false;
    return true;
}

bool FPlanetChunk::GeneratePaddedDensityField(
    const UNoiseGenerator* NoiseGenerator,
    TArray<float>& OutDensityField,
    int32& OutPaddedSize,
    FVector& OutPaddedOrigin,
    float& OutVoxelSize)
{
    if (!NoiseGenerator)
    {
        return false;
    }
    
    // Exact same as Rust: 1 voxel padding
    int32 Padding = 1;
    OutPaddedSize = VoxelResolution + (2 * Padding);  // e.g., 16 + 2 = 18
    OutVoxelSize = Size / float(VoxelResolution);
    
    // Calculate padded origin
    FVector ChunkMin = Position - FVector(Size * 0.5f);
    OutPaddedOrigin = ChunkMin - FVector(Padding * OutVoxelSize);
    
    // Generate density field
    int32 TotalVoxels = OutPaddedSize * OutPaddedSize * OutPaddedSize;
    OutDensityField.SetNum(TotalVoxels);
    
    bool bHasPositive = false;
    bool bHasNegativeOrZero = false;
    
    // Sample a few debug values
    int32 SampleCount = 0;
    float MinDensity = FLT_MAX;
    float MaxDensity = -FLT_MAX;
    
    for (int32 Z = 0; Z < OutPaddedSize; Z++)
    {
        for (int32 Y = 0; Y < OutPaddedSize; Y++)
        {
            for (int32 X = 0; X < OutPaddedSize; X++)
            {
                FVector WorldPos = OutPaddedOrigin + FVector(
                    X * OutVoxelSize,
                    Y * OutVoxelSize,
                    Z * OutVoxelSize
                );
                
                int32 Index = X + Y * OutPaddedSize + Z * OutPaddedSize * OutPaddedSize;
                float DensityValue = NoiseGenerator->SampleDensity(WorldPos);
                OutDensityField[Index] = DensityValue;
                
                // Track min/max for debugging
                MinDensity = FMath::Min(MinDensity, DensityValue);
                MaxDensity = FMath::Max(MaxDensity, DensityValue);
                
                // Track positive and negative values (like Rust implementation)
                if (DensityValue > 0.0f)
                {
                    bHasPositive = true;
                }
                else
                {
                    bHasNegativeOrZero = true;
                }
                
                // Debug first few samples
                if (SampleCount < 5)
                {
                    UE_LOG(LogSurfaceNets, VeryVerbose, TEXT("Sample %d at %s: density = %f"), 
                           SampleCount, *WorldPos.ToString(), DensityValue);
                    SampleCount++;
                }
            }
        }
    }
    
    // Only return true if we have both positive and negative values (surface intersection)
    bool bHasSurface = bHasPositive && bHasNegativeOrZero;
    
    UE_LOG(LogSurfaceNets, Verbose, TEXT("Chunk at %s: MinDensity=%f, MaxDensity=%f, Positive=%s, Negative=%s, HasSurface=%s"), 
           *Position.ToString(), 
           MinDensity, MaxDensity,
           bHasPositive ? TEXT("Yes") : TEXT("No"),
           bHasNegativeOrZero ? TEXT("Yes") : TEXT("No"),
           bHasSurface ? TEXT("Yes") : TEXT("No"));
    
    return bHasSurface;
}

void FPlanetChunk::ClearMesh()
{
    Vertices.Empty();
    Triangles.Empty();
    Normals.Empty();
    UVs.Empty();
    bIsGenerated = false;
}

int32 FPlanetChunk::GetVoxelResolution() const
{
    // Adjust resolution based on LOD level
    int32 BaseResolution = 16;
    return FMath::Max(4, BaseResolution >> LODLevel);
}