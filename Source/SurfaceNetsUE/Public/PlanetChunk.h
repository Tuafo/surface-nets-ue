#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

/**
 * Represents a single chunk of planet terrain
 */
struct SURFACENETSUE_API FPlanetChunk
{
public:
    FPlanetChunk();
    FPlanetChunk(const FVector& InPosition, int32 InLODLevel, float InSize);
    
    /** Position of this chunk in world space */
    FVector Position;
    
    /** LOD level (0 = highest detail) */
    int32 LODLevel;
    
    /** Size of this chunk */
    float Size;
    
    /** Voxel resolution for this chunk */
    int32 VoxelResolution;
    
    /** Generated mesh data */
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    
    /** Whether this chunk has been generated */
    bool bIsGenerated;
    
    /** Whether this chunk is currently being generated */
    bool bIsGenerating;
    
    /** Distance from camera for LOD calculations */
    float DistanceFromCamera;
    
    /** Generate mesh data for this chunk */
    void GenerateMesh(const class UNoiseGenerator* NoiseGenerator);
    
    /** Clear mesh data to free memory */
    void ClearMesh();
    
    /** Get voxel resolution based on LOD level */
    int32 GetVoxelResolution() const;
    
    /** Check if this chunk should be subdivided based on distance */
    bool ShouldSubdivide(float CameraDistance, float SubdivisionDistance) const;
    
    /** Check if this chunk should be merged based on distance */
    bool ShouldMerge(float CameraDistance, float MergeDistance) const;
};