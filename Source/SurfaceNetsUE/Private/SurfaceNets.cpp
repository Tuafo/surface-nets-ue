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
    TArray<FVector>& OutNormals)
{
    OutVertices.Empty();
    OutTriangles.Empty();
    OutNormals.Empty();
    
    if (GridSize < 2)
    {
        return;
    }
    
    // Phase 1: Estimate surface (find all cubes that contain the surface)
    TMap<FIntVector, int32> VertexMap;
    TArray<int32> VertexGrid;
    VertexGrid.Init(-1, GridSize * GridSize * GridSize);
    
    for (int32 z = 0; z < GridSize - 1; z++)
    {
        for (int32 y = 0; y < GridSize - 1; y++)
        {
            for (int32 x = 0; x < GridSize - 1; x++)
            {
                if (ContainsSurface(DensityField, GridSize, x, y, z))
                {
                    FVector VertexPos = CalculateVertexPosition(DensityField, GridSize, x, y, z, VoxelSize, Origin);
                    FVector Normal = CalculateGradient(DensityField, GridSize, x, y, z);
                    
                    int32 VertexIndex = OutVertices.Num();
                    OutVertices.Add(VertexPos);
                    OutNormals.Add(Normal.GetSafeNormal());
                    
                    FIntVector CubePos(x, y, z);
                    VertexMap.Add(CubePos, VertexIndex);
                    
                    // Store in grid for quick lookup
                    int32 GridIndex = x + y * GridSize + z * GridSize * GridSize;
                    VertexGrid[GridIndex] = VertexIndex;
                }
            }
        }
    }
    
    // Phase 2: Make all quads
    MakeAllQuads(DensityField, GridSize, VertexMap, VertexGrid, OutTriangles);
}

void FSurfaceNets::MakeAllQuads(
    const TArray<float>& DensityField,
    int32 GridSize,
    const TMap<FIntVector, int32>& VertexMap,
    const TArray<int32>& VertexGrid,
    TArray<int32>& OutTriangles)
{
    // XYZ strides for navigation
    const FIntVector XYZStrides[3] = {
        FIntVector(1, 0, 0),    // X stride
        FIntVector(0, 1, 0),    // Y stride  
        FIntVector(0, 0, 1)     // Z stride
    };
    
    for (auto& Pair : VertexMap)
    {
        const FIntVector& CubePos = Pair.Key;
        int32 x = CubePos.X;
        int32 y = CubePos.Y;
        int32 z = CubePos.Z;
        
        // Do edges parallel with the X axis
        if (y > 0 && z > 0 && x < GridSize - 2)
        {
            MaybeCreateQuad(
                DensityField, GridSize, VertexGrid,
                CubePos,
                CubePos + XYZStrides[0],
                XYZStrides[1],
                XYZStrides[2],
                OutTriangles,
                TArray<FVector>() // Empty array
            );
        }
        
        // Do edges parallel with the Y axis
        if (x > 0 && z > 0 && y < GridSize - 2)
        {
            MaybeCreateQuad(
                DensityField, GridSize, VertexGrid,
                CubePos,
                CubePos + XYZStrides[1],
                XYZStrides[2],
                XYZStrides[0],
                OutTriangles,
                TArray<FVector>() // Empty array
            );
        }
        
        // Do edges parallel with the Z axis
        if (x > 0 && y > 0 && z < GridSize - 2)
        {
            MaybeCreateQuad(
                DensityField, GridSize, VertexGrid,
                CubePos,
                CubePos + XYZStrides[2],
                XYZStrides[0],
                XYZStrides[1],
                OutTriangles,
                TArray<FVector>() // Empty array
            );
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
    TArray<int32>& OutTriangles,
    const TArray<FVector>& Vertices)
{
    // Get density values at the two cube positions
    float D1 = GetDensity(DensityField, GridSize, P1.X, P1.Y, P1.Z);
    float D2 = GetDensity(DensityField, GridSize, P2.X, P2.Y, P2.Z);
    
    // Determine if we need a face and its orientation
    bool bNegativeFace;
    if (D1 < 0.0f && D2 >= 0.0f)
    {
        bNegativeFace = false;
    }
    else if (D1 >= 0.0f && D2 < 0.0f)
    {
        bNegativeFace = true;
    }
    else
    {
        return; // No face needed
    }
    
    // Get the four vertices of the quad
    int32 V1 = GetVertexIndex(VertexGrid, GridSize, P1.X, P1.Y, P1.Z);
    int32 V2 = GetVertexIndex(VertexGrid, GridSize, P1.X - AxisB.X, P1.Y - AxisB.Y, P1.Z - AxisB.Z);
    int32 V3 = GetVertexIndex(VertexGrid, GridSize, P1.X - AxisC.X, P1.Y - AxisC.Y, P1.Z - AxisC.Z);
    int32 V4 = GetVertexIndex(VertexGrid, GridSize, P1.X - AxisB.X - AxisC.X, P1.Y - AxisB.Y - AxisC.Y, P1.Z - AxisB.Z - AxisC.Z);
    
    // Validate all vertices exist
    if (V1 == -1 || V2 == -1 || V3 == -1 || V4 == -1)
    {
        return;
    }
    
    // Create two triangles based on the face orientation
    // REVERSED winding order for Unreal Engine (clockwise for front-facing)
    if (bNegativeFace)
    {
        // Negative face
        OutTriangles.Add(V1);
        OutTriangles.Add(V2);
        OutTriangles.Add(V4);
        
        OutTriangles.Add(V1);
        OutTriangles.Add(V4);
        OutTriangles.Add(V3);
    }
    else
    {
        // Positive face
        OutTriangles.Add(V1);
        OutTriangles.Add(V4);
        OutTriangles.Add(V2);
        
        OutTriangles.Add(V1);
        OutTriangles.Add(V3);
        OutTriangles.Add(V4);
    }
}

FVector FSurfaceNets::CalculateVertexPosition(
    const TArray<float>& DensityField,
    int32 GridSize,
    int32 x, int32 y, int32 z,
    float VoxelSize,
    const FVector& Origin)
{
    // Get the signed distance values at each corner of this cube
    float CornerDists[8];
    int32 NumNegative = 0;
    
    for (int32 i = 0; i < 8; i++)
    {
        const FIntVector& Corner = CubeCorners[i];
        float Dist = GetDensity(DensityField, GridSize, x + Corner.X, y + Corner.Y, z + Corner.Z);
        CornerDists[i] = Dist;
        if (Dist < 0.0f)
        {
            NumNegative++;
        }
    }
    
    if (NumNegative == 0 || NumNegative == 8)
    {
        // No surface crossing, fallback to cube center
        return Origin + FVector(x + 0.5f, y + 0.5f, z + 0.5f) * VoxelSize;
    }
    
    // Calculate centroid of edge intersections
    FVector CentroidOffset = CalculateCentroidOfEdgeIntersections(CornerDists);
    
    return Origin + (FVector(x, y, z) + CentroidOffset) * VoxelSize;
}

FVector FSurfaceNets::CalculateCentroidOfEdgeIntersections(const float CornerDists[8])
{
    FVector Sum = FVector::ZeroVector;
    int32 Count = 0;
    
    // Check all 12 edges of the cube
    for (int32 i = 0; i < 12; i++)
    {
        int32 Corner1 = CubeEdges[i][0];
        int32 Corner2 = CubeEdges[i][1];
        float Value1 = CornerDists[Corner1];
        float Value2 = CornerDists[Corner2];
        
        // Check if edge crosses the isosurface (different signs)
        if ((Value1 < 0.0f) != (Value2 < 0.0f))
        {
            FVector Intersection = EstimateSurfaceEdgeIntersection(Corner1, Corner2, Value1, Value2);
            Sum += Intersection;
            Count++;
        }
    }
    
    if (Count > 0)
    {
        return Sum / Count;
    }
    
    // Fallback to cube center
    return FVector(0.5f, 0.5f, 0.5f);
}

FVector FSurfaceNets::EstimateSurfaceEdgeIntersection(int32 Corner1, int32 Corner2, float Value1, float Value2)
{
    // Linear interpolation to find zero crossing
    float T = -Value1 / (Value2 - Value1);
    T = FMath::Clamp(T, 0.0f, 1.0f);
    
    return FMath::Lerp(CubeCornerVectors[Corner1], CubeCornerVectors[Corner2], T);
}

FVector FSurfaceNets::CalculateGradient(
    const TArray<float>& DensityField,
    int32 GridSize,
    int32 x, int32 y, int32 z)
{
    // Use central differencing to calculate gradient
    FVector Gradient;
    
    Gradient.X = GetDensity(DensityField, GridSize, x + 1, y, z) - GetDensity(DensityField, GridSize, x - 1, y, z);
    Gradient.Y = GetDensity(DensityField, GridSize, x, y + 1, z) - GetDensity(DensityField, GridSize, x, y - 1, z);
    Gradient.Z = GetDensity(DensityField, GridSize, x, y, z + 1) - GetDensity(DensityField, GridSize, x, y, z - 1);
    
    return Gradient;
}

float FSurfaceNets::GetDensity(const TArray<float>& DensityField, int32 GridSize, int32 x, int32 y, int32 z)
{
    if (x < 0 || x >= GridSize || y < 0 || y >= GridSize || z < 0 || z >= GridSize)
    {
        return 1.0f; // Outside bounds is considered positive (exterior)
    }
    
    int32 Index = x + y * GridSize + z * GridSize * GridSize;
    return DensityField[Index];
}

bool FSurfaceNets::ContainsSurface(const TArray<float>& DensityField, int32 GridSize, int32 x, int32 y, int32 z)
{
    // Check if any corner has a different sign than the others
    bool bHasPositive = false;
    bool bHasNegative = false;
    
    for (int32 i = 0; i < 8; i++)
    {
        const FIntVector& Corner = CubeCorners[i];
        float Density = GetDensity(DensityField, GridSize, x + Corner.X, y + Corner.Y, z + Corner.Z);
        
        if (Density < 0.0f)
        {
            bHasNegative = true;
        }
        else
        {
            bHasPositive = true;
        }
        
        if (bHasPositive && bHasNegative)
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