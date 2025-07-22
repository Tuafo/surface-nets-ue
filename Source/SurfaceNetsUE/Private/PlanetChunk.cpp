#include "PlanetChunk.h"
#include "NoiseGenerator.h"
#include "SurfaceNets.h"
#include "SurfaceNetsUE.h"

FPlanetChunk::FPlanetChunk()
    : Position(FVector::ZeroVector)
    , LODLevel(0)
    , Size(1000.0f)
    , VoxelResolution(BASE_VOXEL_RESOLUTION)
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

int32 FPlanetChunk::GetVoxelResolution() const
{
    // Higher LOD levels have lower resolution
    // LOD 0 = BASE_VOXEL_RESOLUTION, LOD 1 = BASE_VOXEL_RESOLUTION/2, etc.
    int32 Divisor = FMath::RoundToInt(FMath::Pow(2.0f, static_cast<float>(LODLevel)));
    return FMath::Max(8, BASE_VOXEL_RESOLUTION / Divisor);
}

int32 FPlanetChunk::GetPaddedVoxelResolution() const
{
    return GetVoxelResolution() + (CHUNK_PADDING * 2);
}

bool FPlanetChunk::ShouldSubdivide(float CameraDistance, float SubdivisionDistance) const
{
    return CameraDistance < SubdivisionDistance && LODLevel < 8; // Max 8 LOD levels
}

bool FPlanetChunk::ShouldMerge(float CameraDistance, float MergeDistance) const
{
    return CameraDistance > MergeDistance && LODLevel > 0; // Min LOD level 0
}

void FPlanetChunk::GenerateMesh(const UNoiseGenerator* NoiseGenerator)
{
    if (!NoiseGenerator || bIsGenerating)
    {
        return;
    }

    bIsGenerating = true;
    ClearMesh();

    // Generate density field with padding
    TArray<float> DensityField;
    int32 PaddedSize;
    FVector PaddedOrigin;
    float VoxelSize;
    
    GeneratePaddedDensityField(NoiseGenerator, DensityField, PaddedSize, PaddedOrigin, VoxelSize);

    // Generate mesh using Surface Nets
    FSurfaceNets SurfaceNets;
    SurfaceNets.GenerateMesh(
        DensityField,
        PaddedSize,
        VoxelSize,
        PaddedOrigin,
        Vertices,
        Triangles,
        Normals
    );

    // Generate UVs (simple planar mapping for now)
    UVs.SetNum(Vertices.Num());
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        FVector LocalPos = (Vertices[i] - Position) / Size;
        UVs[i] = FVector2D(LocalPos.X + 0.5f, LocalPos.Y + 0.5f);
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

void FPlanetChunk::GeneratePaddedDensityField(
    const UNoiseGenerator* NoiseGenerator,
    TArray<float>& OutDensityField,
    int32& OutPaddedSize,
    FVector& OutPaddedOrigin,
    float& OutVoxelSize)
{
    int32 Resolution = GetVoxelResolution();
    OutPaddedSize = Resolution + (CHUNK_PADDING * 2);
    
    OutVoxelSize = Size / static_cast<float>(Resolution);
    OutPaddedOrigin = Position - FVector(Size * 0.5f) - FVector(OutVoxelSize * static_cast<float>(CHUNK_PADDING));
    
    // Allocate density field
    int32 TotalVoxels = OutPaddedSize * OutPaddedSize * OutPaddedSize;
    OutDensityField.SetNum(TotalVoxels);
    
    // Sample density field
    for (int32 z = 0; z < OutPaddedSize; z++)
    {
        for (int32 y = 0; y < OutPaddedSize; y++)
        {
            for (int32 x = 0; x < OutPaddedSize; x++)
            {
                FVector WorldPos = OutPaddedOrigin + FVector(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) * OutVoxelSize;
                float Density = NoiseGenerator->SampleDensity(WorldPos);
                
                int32 Index = x + y * OutPaddedSize + z * OutPaddedSize * OutPaddedSize;
                OutDensityField[Index] = Density;
            }
        }
    }
}