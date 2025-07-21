#include "PlanetChunk.h"
#include "NoiseGenerator.h"
#include "SurfaceNets.h"

FPlanetChunk::FPlanetChunk()
    : Position(FVector::ZeroVector)
    , LODLevel(0)
    , Size(64.0f)
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

void FPlanetChunk::GenerateMesh(const UNoiseGenerator* NoiseGenerator)
{
    if (bIsGenerating || bIsGenerated || !NoiseGenerator)
    {
        return;
    }
    
    bIsGenerating = true;
    
    // Clear previous data
    ClearMesh();
    
    // Generate density field
    TArray<float> DensityField;
    int32 VoxelCount = (VoxelResolution + 1) * (VoxelResolution + 1) * (VoxelResolution + 1);
    DensityField.SetNum(VoxelCount);
    
    float VoxelSize = Size / VoxelResolution;
    FVector ChunkOrigin = Position - FVector(Size * 0.5f);
    
    // Sample density field
    for (int32 z = 0; z <= VoxelResolution; z++)
    {
        for (int32 y = 0; y <= VoxelResolution; y++)
        {
            for (int32 x = 0; x <= VoxelResolution; x++)
            {
                FVector VoxelPos = ChunkOrigin + FVector(x, y, z) * VoxelSize;
                int32 Index = x + y * (VoxelResolution + 1) + z * (VoxelResolution + 1) * (VoxelResolution + 1);
                DensityField[Index] = NoiseGenerator->SampleDensity(VoxelPos);
            }
        }
    }
    
    // Generate mesh using Surface Nets
    FSurfaceNets SurfaceNets;
    SurfaceNets.GenerateMesh(DensityField, VoxelResolution + 1, VoxelSize, ChunkOrigin, Vertices, Triangles, Normals);
    
    // Generate UVs (simple planar mapping for now)
    UVs.SetNum(Vertices.Num());
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        FVector LocalPos = (Vertices[i] - ChunkOrigin) / Size;
        UVs[i] = FVector2D(LocalPos.X, LocalPos.Y);
    }
    
    bIsGenerated = true;
    bIsGenerating = false;
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
    // Adjust voxel resolution based on LOD level for better planet representation
    // Higher LOD levels have lower resolution but still enough detail for spheres
    switch (LODLevel)
    {
        case 0: return 32;  // Highest detail - reduced from 64 for better performance
        case 1: return 24;  // Good detail
        case 2: return 16;  // Medium detail
        case 3: return 12;  // Lower detail
        case 4: return 8;   // Lowest detail
        default: return 6;  // Fallback
    }
}

bool FPlanetChunk::ShouldSubdivide(float CameraDistance, float SubdivisionDistance) const
{
    return CameraDistance < SubdivisionDistance && LODLevel < 4;
}

bool FPlanetChunk::ShouldMerge(float CameraDistance, float MergeDistance) const
{
    return CameraDistance > MergeDistance && LODLevel > 0;
}