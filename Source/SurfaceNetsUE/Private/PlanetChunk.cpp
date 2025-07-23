#include "PlanetChunk.h"
#include "NoiseGenerator.h"
#include "SurfaceNets.h"
#include "SurfaceNetsUE.h"

FPlanetChunk::FPlanetChunk()
    : Position(FVector::ZeroVector)
    , LODLevel(0)
    , Size(1000.0f)
    , bIsGenerated(false)
    , bIsGenerating(false)
    , bIsEmpty(false)
    , DistanceFromCamera(0.0f)
{
}

FPlanetChunk::FPlanetChunk(const FVector& InPosition, int32 InLODLevel, float InSize)
    : Position(InPosition)
    , LODLevel(InLODLevel)
    , Size(InSize)
    , bIsGenerated(false)
    , bIsGenerating(false)
    , bIsEmpty(false)
    , DistanceFromCamera(0.0f)
{
}

bool FPlanetChunk::GenerateMesh(const UNoiseGenerator* NoiseGenerator)
{
    if (!NoiseGenerator || bIsGenerating)
    {
        return false;
    }

    bIsGenerating = true;
    ClearMesh();

    TArray<float> DensityField;
    int32 PaddedSize;
    FVector PaddedOrigin;
    float VoxelSize;

    // Generate density field with padding (like Rust implementation)
    if (!GeneratePaddedDensityField(NoiseGenerator, DensityField, PaddedSize, PaddedOrigin, VoxelSize))
    {
        bIsGenerating = false;
        bIsEmpty = true;
        bIsGenerated = true;
        return false;
    }

    // Early exit if no surface (like Rust optimization)
    if (!FSurfaceNets::HasSurfaceInChunk(DensityField))
    {
        bIsGenerating = false;
        bIsEmpty = true;
        bIsGenerated = true;
        return false;
    }

    // Generate mesh using Surface Nets with Rust-like bounds
    FSurfaceNets SurfaceNets;
    SurfaceNets.GenerateMesh(
        DensityField,
        PaddedSize,
        VoxelSize,
        PaddedOrigin,
        Vertices,
        Triangles,
        Normals,
        FIntVector(0, 0, 0),                    // Min bounds
        FIntVector(UNPADDED_CHUNK_SIZE + 1)     // Max bounds (17,17,17) like Rust [0;3], [17;3]
    );

    // Generate UVs
    UVs.SetNum(Vertices.Num());
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        // Simple planar UV mapping
        FVector LocalPos = Vertices[i] - Position;
        UVs[i] = FVector2D(
            (LocalPos.X / Size) + 0.5f,
            (LocalPos.Y / Size) + 0.5f
        );
    }

    bIsGenerating = false;
    bIsGenerated = true;
    bIsEmpty = (Vertices.Num() == 0);

    UE_LOG(LogSurfaceNets, Verbose, TEXT("Generated chunk at %s with %d vertices, %d triangles"), 
           *Position.ToString(), Vertices.Num(), Triangles.Num() / 3);

    return !bIsEmpty;
}

void FPlanetChunk::ClearMesh()
{
    Vertices.Empty();
    Triangles.Empty();
    Normals.Empty();
    UVs.Empty();
    bIsGenerated = false;
    bIsEmpty = false;
}

int32 FPlanetChunk::GetVoxelResolution() const
{
    // LOD-based resolution like original design
    return FMath::Max(8, UNPADDED_CHUNK_SIZE >> LODLevel);
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

    // Use padded size (18x18x18) like Rust implementation
    OutPaddedSize = PADDED_CHUNK_SIZE;
    OutVoxelSize = Size / UNPADDED_CHUNK_SIZE;
    
    // Calculate padded origin (offset by -1 voxel for padding)
    OutPaddedOrigin = Position - FVector(Size * 0.5f) - FVector(OutVoxelSize);

    // Allocate density field
    OutDensityField.SetNum(OutPaddedSize * OutPaddedSize * OutPaddedSize);

    // Generate density values with padding (matching Rust approach)
    bool HasSurface = false;
    bool HasPositive = false;
    bool HasNegativeOrZero = false;

    for (int32 z = 0; z < OutPaddedSize; z++)
    {
        for (int32 y = 0; y < OutPaddedSize; y++)
        {
            for (int32 x = 0; x < OutPaddedSize; x++)
            {
                // Convert to world coordinates
                FVector WorldPos = OutPaddedOrigin + FVector(
                    x * OutVoxelSize,
                    y * OutVoxelSize,
                    z * OutVoxelSize
                );

                // Sample density
                float Density = NoiseGenerator->SampleDensity(WorldPos);

                int32 Index = x + y * OutPaddedSize + z * OutPaddedSize * OutPaddedSize;
                OutDensityField[Index] = Density;

                // Track if surface exists (like Rust early detection)
                if (Density > 0.0f)
                {
                    HasPositive = true;
                }
                else
                {
                    HasNegativeOrZero = true;
                }

                if (HasPositive && HasNegativeOrZero)
                {
                    HasSurface = true;
                }
            }
        }
    }

    return HasSurface;
}