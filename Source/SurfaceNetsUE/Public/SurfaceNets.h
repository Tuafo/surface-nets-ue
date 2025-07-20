#pragma once

#include "CoreMinimal.h"

/**
 * Surface Nets mesh generation algorithm implementation
 * Generates smooth meshes from voxel density fields
 */
struct SURFACENETSUE_API FSurfaceNets
{
public:
    /** Generate mesh from density field using Surface Nets algorithm */
    void GenerateMesh(
        const TArray<float>& DensityField,
        int32 GridSize,
        float VoxelSize,
        const FVector& Origin,
        TArray<FVector>& OutVertices,
        TArray<int32>& OutTriangles,
        TArray<FVector>& OutNormals
    );

private:
    /** Calculate vertex position using Surface Nets smoothing */
    FVector CalculateVertexPosition(
        const TArray<float>& DensityField,
        int32 GridSize,
        int32 x, int32 y, int32 z,
        float VoxelSize,
        const FVector& Origin
    );
    
    /** Calculate gradient at a point for normal calculation */
    FVector CalculateGradient(
        const TArray<float>& DensityField,
        int32 GridSize,
        int32 x, int32 y, int32 z
    );
    
    /** Get density value at grid coordinates (with bounds checking) */
    float GetDensity(const TArray<float>& DensityField, int32 GridSize, int32 x, int32 y, int32 z);
    
    /** Check if a cube contains the surface (density changes sign) */
    bool ContainsSurface(const TArray<float>& DensityField, int32 GridSize, int32 x, int32 y, int32 z);
    
    /** Generate triangles for a cube face */
    void GenerateCubeFace(
        int32 CubeX, int32 CubeY, int32 CubeZ,
        int32 Face,
        const TArray<int32>& VertexIndices,
        TArray<int32>& OutTriangles
    );
    
    /** Get vertex index for a cube, creating if necessary */
    int32 GetOrCreateVertex(int32 x, int32 y, int32 z, TMap<FIntVector, int32>& VertexMap);
    
    /** Cube face directions for mesh generation */
    static const FIntVector CubeFaces[6][4];
};