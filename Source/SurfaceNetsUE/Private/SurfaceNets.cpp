#include "SurfaceNets.h"
#include "SurfaceNetsUE.h"

// Static data
const FIntVector FSurfaceNets::CubeCorners[8] = {
    FIntVector(0, 0, 0), FIntVector(1, 0, 0), FIntVector(0, 1, 0), FIntVector(1, 1, 0),
    FIntVector(0, 0, 1), FIntVector(1, 0, 1), FIntVector(0, 1, 1), FIntVector(1, 1, 1)
};

const FVector FSurfaceNets::CubeCornerVectors[8] = {
    FVector(0.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(1.0f, 1.0f, 0.0f),
    FVector(0.0f, 0.0f, 1.0f), FVector(1.0f, 0.0f, 1.0f), FVector(0.0f, 1.0f, 1.0f), FVector(1.0f, 1.0f, 1.0f)
};

const int32 FSurfaceNets::CubeEdges[12][2] = {
    {0, 1}, {0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 3}, {2, 6}, {3, 7}, {4, 5}, {4, 6}, {5, 7}, {6, 7}
};

void FSurfaceNets::GenerateMesh(
    const TArray<float>& DensityField,
    int32 GridSize,
    float VoxelSize,
    const FVector& Origin,
    TArray<FVector>& OutVertices,
    TArray<int32>& OutTriangles,
    TArray<FVector>& OutNormals,
    const FIntVector& MinBounds,
    const FIntVector& MaxBounds)
{
    // Clear output arrays
    OutVertices.Empty();
    OutTriangles.Empty();
    OutNormals.Empty();
    
    if (GridSize < 2)
    {
        return;
    }
    
    // Use provided bounds or default to full grid like Rust implementation
    FIntVector ActualMinBounds = (MinBounds == FIntVector(0, 0, 0) && MaxBounds == FIntVector(0, 0, 0)) 
        ? FIntVector(0, 0, 0) 
        : MinBounds;
    FIntVector ActualMaxBounds = (MinBounds == FIntVector(0, 0, 0) && MaxBounds == FIntVector(0, 0, 0)) 
        ? FIntVector(GridSize - 1, GridSize - 1, GridSize - 1) 
        : MaxBounds;
    
    // Early exit if no surface found (like Rust optimization)
    if (!HasSurfaceInChunk(DensityField))
    {
        return;
    }
    
    // Vertex grid for tracking surface vertices (equivalent to stride_to_index in Rust)
    TArray<int32> VertexGrid;
    VertexGrid.Init(-1, GridSize * GridSize * GridSize);
    
    // Phase 1: Estimate surface (find all cubes that contain the surface)
    EstimateSurface(DensityField, GridSize, ActualMinBounds, ActualMaxBounds, 
                   VertexGrid, OutVertices, OutNormals, VoxelSize, Origin);
    
    // Phase 2: Make all quads (with seamless boundary handling like Rust)
    MakeAllQuads(DensityField, GridSize, ActualMinBounds, ActualMaxBounds, VertexGrid, OutTriangles);
}

bool FSurfaceNets::HasSurfaceInChunk(const TArray<float>& DensityField)
{
    bool HasPositive = false;
    bool HasNegativeOrZero = false;
    
    for (float Value : DensityField)
    {
        if (Value > 0.0f)
        {
            HasPositive = true;
        }
        else
        {
            HasNegativeOrZero = true;
        }
        
        // Early exit if we found both (surface exists)
        if (HasPositive && HasNegativeOrZero)
        {
            return true;
        }
    }
    
    return false;
}

void FSurfaceNets::EstimateSurface(
    const TArray<float>& DensityField,
    int32 GridSize,
    const FIntVector& MinBounds,
    const FIntVector& MaxBounds,
    TArray<int32>& VertexGrid,
    TArray<FVector>& OutVertices,
    TArray<FVector>& OutNormals,
    float VoxelSize,
    const FVector& Origin)
{
    for (int32 x = MinBounds.X; x < MaxBounds.X; x++)
    {
        for (int32 y = MinBounds.Y; y < MaxBounds.Y; y++)
        {
            for (int32 z = MinBounds.Z; z < MaxBounds.Z; z++)
            {
                if (ContainsSurface(DensityField, GridSize, x, y, z))
                {
                    // Calculate vertex position using Surface Nets method
                    FVector VertexPos = CalculateVertexPosition(DensityField, GridSize, x, y, z, VoxelSize, Origin);
                    
                    // Calculate normal using gradient
                    FVector Normal = CalculateGradient(DensityField, GridSize, x, y, z);
                    
                    int32 VertexIndex = OutVertices.Num();
                    OutVertices.Add(VertexPos);
                    OutNormals.Add(Normal);
                    
                    // Store vertex index in grid
                    int32 GridIndex = x + y * GridSize + z * GridSize * GridSize;
                    VertexGrid[GridIndex] = VertexIndex;
                }
            }
        }
    }
}

void FSurfaceNets::MakeAllQuads(
    const TArray<float>& DensityField,
    int32 GridSize,
    const FIntVector& MinBounds,
    const FIntVector& MaxBounds,
    const TArray<int32>& VertexGrid,
    TArray<int32>& OutTriangles)
{
    // Key change: Don't generate quads on positive boundaries (MaxBounds)
    // This ensures seamless stitching between chunks like in Rust implementation
    for (int32 x = MinBounds.X; x < MaxBounds.X; x++)
    {
        for (int32 y = MinBounds.Y; y < MaxBounds.Y; y++)
        {
            for (int32 z = MinBounds.Z; z < MaxBounds.Z; z++)
            {
                FIntVector CubePos(x, y, z);
                
                // Only generate quads in negative directions to avoid duplicates
                // This ensures seamless stitching between chunks
                
                // Check X-direction (negative)
                if (x > MinBounds.X)
                {
                    MaybeCreateQuad(DensityField, GridSize, VertexGrid, 
                                   CubePos, CubePos + FIntVector(-1, 0, 0),
                                   FIntVector(0, 1, 0), FIntVector(0, 0, 1), OutTriangles);
                }
                
                // Check Y-direction (negative)  
                if (y > MinBounds.Y)
                {
                    MaybeCreateQuad(DensityField, GridSize, VertexGrid,
                                   CubePos, CubePos + FIntVector(0, -1, 0),
                                   FIntVector(0, 0, 1), FIntVector(1, 0, 0), OutTriangles);
                }
                
                // Check Z-direction (negative)
                if (z > MinBounds.Z)
                {
                    MaybeCreateQuad(DensityField, GridSize, VertexGrid,
                                   CubePos, CubePos + FIntVector(0, 0, -1),
                                   FIntVector(1, 0, 0), FIntVector(0, 1, 0), OutTriangles);
                }
            }
        }
    }
}

void FSurfaceNets::MaybeCreateQuad(
    const TArray<float>& DensityField,
    int32 GridSize,
    const TArray<int32>& VertexGrid,
    const FIntVector& P1,
    const FIntVector& P2,
    const FIntVector& AxisB,
    const FIntVector& AxisC,
    TArray<int32>& OutTriangles)
{
    // Get vertex indices for the four corners of the potential quad
    int32 V1 = GetVertexIndex(VertexGrid, GridSize, P1.X, P1.Y, P1.Z);
    int32 V2 = GetVertexIndex(VertexGrid, GridSize, P2.X, P2.Y, P2.Z);
    int32 V3 = GetVertexIndex(VertexGrid, GridSize, P1.X + AxisB.X, P1.Y + AxisB.Y, P1.Z + AxisB.Z);
    int32 V4 = GetVertexIndex(VertexGrid, GridSize, P2.X + AxisB.X, P2.Y + AxisB.Y, P2.Z + AxisB.Z);
    
    // Check if all vertices exist
    if (V1 >= 0 && V2 >= 0 && V3 >= 0 && V4 >= 0)
    {
        // Create two triangles from the quad
        // Triangle 1: V1, V2, V3
        OutTriangles.Add(V1);
        OutTriangles.Add(V2);
        OutTriangles.Add(V3);
        
        // Triangle 2: V2, V4, V3
        OutTriangles.Add(V2);
        OutTriangles.Add(V4);
        OutTriangles.Add(V3);
    }
}

FVector FSurfaceNets::CalculateVertexPosition(
    const TArray<float>& DensityField,
    int32 GridSize,
    int32 x, int32 y, int32 z,
    float VoxelSize,
    const FVector& Origin)
{
    // Get the 8 corner values of the cube
    float CornerValues[8];
    for (int32 i = 0; i < 8; i++)
    {
        FIntVector Corner = FIntVector(x, y, z) + CubeCorners[i];
        CornerValues[i] = GetDensity(DensityField, GridSize, Corner.X, Corner.Y, Corner.Z);
    }
    
    // Calculate the centroid of edge intersections (Surface Nets approach)
    FVector Centroid = FVector::ZeroVector;
    int32 EdgeCount = 0;
    
    for (int32 i = 0; i < 12; i++)
    {
        int32 V0 = CubeEdges[i][0];
        int32 V1 = CubeEdges[i][1];
        
        float D0 = CornerValues[V0];
        float D1 = CornerValues[V1];
        
        // Check if edge crosses the surface (sign change)
        if ((D0 > 0.0f && D1 <= 0.0f) || (D0 <= 0.0f && D1 > 0.0f))
        {
            // Linear interpolation to find intersection point
            float T = D0 / (D0 - D1);
            T = FMath::Clamp(T, 0.0f, 1.0f);
            
            FVector EdgePoint = CubeCornerVectors[V0] + T * (CubeCornerVectors[V1] - CubeCornerVectors[V0]);
            Centroid += EdgePoint;
            EdgeCount++;
        }
    }
    
    if (EdgeCount > 0)
    {
        Centroid /= static_cast<float>(EdgeCount);
    }
    else
    {
        // Fallback to cube center
        Centroid = FVector(0.5f, 0.5f, 0.5f);
    }
    
    // Convert to world coordinates
    FVector WorldPos = Origin + FVector(
        (x + Centroid.X) * VoxelSize,
        (y + Centroid.Y) * VoxelSize,
        (z + Centroid.Z) * VoxelSize
    );
    
    return WorldPos;
}

FVector FSurfaceNets::CalculateGradient(
    const TArray<float>& DensityField,
    int32 GridSize,
    int32 x, int32 y, int32 z)
{
    FVector Gradient;
    
    // Central differencing for gradient calculation
    Gradient.X = GetDensity(DensityField, GridSize, x + 1, y, z) - GetDensity(DensityField, GridSize, x - 1, y, z);
    Gradient.Y = GetDensity(DensityField, GridSize, x, y + 1, z) - GetDensity(DensityField, GridSize, x, y - 1, z);
    Gradient.Z = GetDensity(DensityField, GridSize, x, y, z + 1) - GetDensity(DensityField, GridSize, x, y, z - 1);
    
    return -Gradient.GetSafeNormal(); // Negative for correct normal direction
}

float FSurfaceNets::GetDensity(const TArray<float>& DensityField, int32 GridSize, int32 x, int32 y, int32 z)
{
    // Bounds checking
    if (x < 0 || x >= GridSize || y < 0 || y >= GridSize || z < 0 || z >= GridSize)
    {
        return 1.0f; // Outside boundary is considered "air"
    }
    
    int32 Index = x + y * GridSize + z * GridSize * GridSize;
    return DensityField[Index];
}

bool FSurfaceNets::ContainsSurface(const TArray<float>& DensityField, int32 GridSize, int32 x, int32 y, int32 z)
{
    bool HasPositive = false;
    bool HasNegative = false;
    
    for (int32 i = 0; i < 8; i++)
    {
        FIntVector Corner = FIntVector(x, y, z) + CubeCorners[i];
        float Density = GetDensity(DensityField, GridSize, Corner.X, Corner.Y, Corner.Z);
        
        if (Density > 0.0f)
        {
            HasPositive = true;
        }
        else
        {
            HasNegative = true;
        }
        
        // Early exit if both found
        if (HasPositive && HasNegative)
        {
            return true;
        }
    }
    
    return false;
}

int32 FSurfaceNets::GetVertexIndex(const TArray<int32>& VertexGrid, int32 GridSize, int32 x, int32 y, int32 z)
{
    if (x < 0 || x >= GridSize || y < 0 || y >= GridSize || z < 0 || z >= GridSize)
    {
        return -1;
    }
    
    int32 Index = x + y * GridSize + z * GridSize * GridSize;
    return VertexGrid[Index];
}