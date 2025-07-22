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
    
    // Generate padded density field for seamless chunk boundaries
    TArray<float> PaddedDensityField;
    int32 PaddedSize;
    FVector PaddedOrigin;
    float VoxelSize;
    
    GeneratePaddedDensityField(NoiseGenerator, PaddedDensityField, PaddedSize, PaddedOrigin, VoxelSize);
    
    // Generate mesh using Surface Nets on padded data
    FSurfaceNets SurfaceNets;
    TArray<FVector> PaddedVertices;
    TArray<int32> PaddedTriangles;
    TArray<FVector> PaddedNormals;
    
    SurfaceNets.GenerateMesh(PaddedDensityField, PaddedSize, VoxelSize, PaddedOrigin, 
                           PaddedVertices, PaddedTriangles, PaddedNormals);
    
    // Filter vertices and triangles to only include those within the actual chunk bounds
    // (excluding padding area)
    FVector ChunkMin = Position - FVector(Size * 0.5f);
    FVector ChunkMax = Position + FVector(Size * 0.5f);
    
    TMap<int32, int32> VertexRemapping;
    
    // Filter vertices that are within chunk bounds
    for (int32 i = 0; i < PaddedVertices.Num(); i++)
    {
        const FVector& Vertex = PaddedVertices[i];
        
        // Check if vertex is within chunk bounds (with small tolerance for boundary vertices)
        if (Vertex.X >= ChunkMin.X - VoxelSize * 0.1f && Vertex.X <= ChunkMax.X + VoxelSize * 0.1f &&
            Vertex.Y >= ChunkMin.Y - VoxelSize * 0.1f && Vertex.Y <= ChunkMax.Y + VoxelSize * 0.1f &&
            Vertex.Z >= ChunkMin.Z - VoxelSize * 0.1f && Vertex.Z <= ChunkMax.Z + VoxelSize * 0.1f)
        {
            int32 NewIndex = Vertices.Num();
            VertexRemapping.Add(i, NewIndex);
            Vertices.Add(Vertex);
            Normals.Add(PaddedNormals[i]);
        }
    }
    
    // Filter triangles that reference valid vertices
    for (int32 i = 0; i < PaddedTriangles.Num(); i += 3)
    {
        int32 V1 = PaddedTriangles[i];
        int32 V2 = PaddedTriangles[i + 1];
        int32 V3 = PaddedTriangles[i + 2];
        
        int32* NewV1 = VertexRemapping.Find(V1);
        int32* NewV2 = VertexRemapping.Find(V2);
        int32* NewV3 = VertexRemapping.Find(V3);
        
        if (NewV1 && NewV2 && NewV3)
        {
            Triangles.Add(*NewV1);
            Triangles.Add(*NewV2);
            Triangles.Add(*NewV3);
        }
    }
    
    // Generate UVs
    UVs.SetNum(Vertices.Num());
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        FVector LocalPos = (Vertices[i] - ChunkMin) / Size;
        UVs[i] = FVector2D(LocalPos.X, LocalPos.Y);
    }
    
    bIsGenerated = true;
    bIsGenerating = false;
}

void FPlanetChunk::GeneratePaddedDensityField(
    const UNoiseGenerator* NoiseGenerator,
    TArray<float>& OutDensityField,
    int32& OutPaddedSize,
    FVector& OutPaddedOrigin,
    float& OutVoxelSize)
{
    // Add 1-voxel padding on all sides (like Rust implementation)
    int32 PaddedResolution = VoxelResolution + 2; // +2 for padding on both sides
    OutPaddedSize = PaddedResolution;
    
    OutVoxelSize = Size / static_cast<float>(VoxelResolution);
    
    // Origin should be offset by one voxel to account for padding
    OutPaddedOrigin = Position - FVector(Size * 0.5f) - FVector(OutVoxelSize);
    
    OutDensityField.SetNum(PaddedResolution * PaddedResolution * PaddedResolution);
    
    // Sample density field with padding
    for (int32 z = 0; z < PaddedResolution; z++)
    {
        for (int32 y = 0; y < PaddedResolution; y++)
        {
            for (int32 x = 0; x < PaddedResolution; x++)
            {
                FVector WorldPos = OutPaddedOrigin + FVector(
                    x * OutVoxelSize,
                    y * OutVoxelSize,
                    z * OutVoxelSize
                );
                
                int32 Index = x + y * PaddedResolution + z * PaddedResolution * PaddedResolution;
                OutDensityField[Index] = NoiseGenerator->SampleDensity(WorldPos);
            }
        }
    }
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
    // Adjust voxel resolution based on LOD level
    switch (LODLevel)
    {
        case 0: return 32;  // Highest detail
        case 1: return 24;  // Good detail
        case 2: return 16;  // Medium detail
        case 3: return 12;  // Lower detail
        case 4: return 8;   // Low detail
        default: return 6;  // Lowest detail
    }
}

int32 FPlanetChunk::GetPaddedVoxelResolution() const
{
    return GetVoxelResolution() + 2 * CHUNK_PADDING + 1;
}

bool FPlanetChunk::ShouldSubdivide(float CameraDistance, float SubdivisionDistance) const
{
    return CameraDistance < SubdivisionDistance && LODLevel < 5;
}

bool FPlanetChunk::ShouldMerge(float CameraDistance, float MergeDistance) const
{
    return CameraDistance > MergeDistance * 1.5f; // Hysteresis to prevent flickering
}