#pragma once

#include "CoreMinimal.h"

class UNoiseGenerator;

/**
 * Represents a chunk of planet terrain, equivalent to Rust chunk implementation
 */
struct SURFACENETSUE_API FPlanetChunk
{
public:
    FPlanetChunk();
    FPlanetChunk(const FVector& InPosition, int32 InLODLevel, float InSize);

    /** Chunk center position in world coordinates */
    FVector Position;
    
    /** LOD level (0 = highest detail) */
    int32 LODLevel;
    
    /** Size of the chunk in world units */
    float Size;
    
    /** Base voxel resolution (unpadded) */
    int32 VoxelResolution;
    
    /** Generated mesh data */
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    
    /** Generation state */
    bool bIsGenerated;
    bool bIsGenerating;
    
    /** Distance from camera for LOD calculations */
    float DistanceFromCamera;

    /** Generate mesh for this chunk (equivalent to Rust chunk processing) */
    bool GenerateMesh(const UNoiseGenerator* NoiseGenerator);
    
    /** Clear all mesh data */
    void ClearMesh();
    
    /** Get voxel resolution based on LOD level */
    int32 GetVoxelResolution() const;

private:
    /** Generate padded density field exactly like Rust implementation */
    bool GeneratePaddedDensityField(
        const UNoiseGenerator* NoiseGenerator,
        TArray<float>& OutDensityField,
        int32& OutPaddedSize,
        FVector& OutPaddedOrigin,
        float& OutVoxelSize
    );
};