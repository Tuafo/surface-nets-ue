#pragma once

#include "CoreMinimal.h"

/**
 * Surface Nets mesh generation algorithm implementation
 * Generates smooth meshes from voxel density fields using the fast surface nets approach
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
    /** Phase 2: Create all quads from surface vertices (equivalent to make_all_quads in Rust) */
    void MakeAllQuads(
        const TArray<float>& DensityField,
        int32 GridSize,
        const TMap<FIntVector, int32>& VertexMap,
        const TArray<int32>& VertexGrid,
        TArray<int32>& OutTriangles
    );
    
    /** Create a quad if there's a surface crossing between two adjacent cubes */
    void MaybeCreateQuad(
        const TArray<float>& DensityField,
        int32 GridSize,
        const TArray<int32>& VertexGrid,
        const FIntVector& P1,
        const FIntVector& P2,
        const FIntVector& AxisB,
        const FIntVector& AxisC,
        TArray<int32>& OutTriangles
    );

    void CreateQuadBetweenCubes(
        const FIntVector& Cube1,
        const FIntVector& Cube2,
        const TMap<FIntVector, int32>& VertexMap,
        TArray<int32>& OutTriangles
    );
    
    /** Calculate vertex position using Surface Nets smoothing (equivalent to estimate_surface in Rust) */
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
    
    /** Get vertex index for a cube, creating if necessary */
    int32 GetOrCreateVertex(int32 x, int32 y, int32 z, TMap<FIntVector, int32>& VertexMap);
    
    /** Get vertex index from vertex grid */
    int32 GetVertexIndex(const TArray<int32>& VertexGrid, int32 GridSize, int32 x, int32 y, int32 z);
    
    /** Cube face directions for mesh generation */
    static const FIntVector CubeFaces[6][4];
};