#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "PlanetChunk.generated.h"

/**
 * Represents a single chunk of planet terrain with octree integration
 */
USTRUCT(BlueprintType)
struct SURFACENETSUE_API FPlanetChunk
{
    GENERATED_BODY()

public:
    FPlanetChunk();
    FPlanetChunk(const FVector& InPosition, int32 InLODLevel, float InSize);
    
    /** Position of this chunk in world space */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Chunk")
    FVector Position;
    
    /** LOD level (0 = highest detail) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Chunk")
    int32 LODLevel;
    
    /** Size of this chunk */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Chunk")
    float Size;
    
    /** Voxel resolution for this chunk */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Chunk")
    int32 VoxelResolution;
    
    /** Base voxel resolution per LOD level */
    static constexpr int32 BASE_VOXEL_RESOLUTION = 32;
    
    /** Padding size for seamless chunk boundaries */
    static constexpr int32 CHUNK_PADDING = 1;
    
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
    
    /** Generate mesh data for this chunk with proper padding */
    void GenerateMesh(const class UNoiseGenerator* NoiseGenerator);
    
    /** Clear mesh data to free memory */
    void ClearMesh();
    
    /** Get voxel resolution based on LOD level */
    int32 GetVoxelResolution() const;
    
    /** Get padded voxel resolution (includes padding on all sides) */
    int32 GetPaddedVoxelResolution() const;
    
    /** Check if this chunk should be subdivided based on distance */
    bool ShouldSubdivide(float CameraDistance, float SubdivisionDistance) const;
    
    /** Check if this chunk should be merged based on distance */
    bool ShouldMerge(float CameraDistance, float MergeDistance) const;

private:
    /** Generate padded density field for seamless mesh generation */
    void GeneratePaddedDensityField(
        const class UNoiseGenerator* NoiseGenerator,
        TArray<float>& OutDensityField,
        int32& OutPaddedSize,
        FVector& OutPaddedOrigin,
        float& OutVoxelSize
    );
};