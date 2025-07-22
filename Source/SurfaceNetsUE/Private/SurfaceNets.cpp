#include "SurfaceNets.h"
#include "SurfaceNetsUE.h"

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
    // For each cube that has a vertex, check all three positive directions for adjacent cubes
    for (const auto& VertexPair : VertexMap)
    {
        FIntVector CubePos = VertexPair.Key;
        int32 VertexIndex = VertexPair.Value;
        
        // Check +X direction
        FIntVector NeighborX = CubePos + FIntVector(1, 0, 0);
        if (VertexMap.Contains(NeighborX))
        {
            CreateQuadBetweenCubes(CubePos, NeighborX, VertexMap, OutTriangles);
        }
        
        // Check +Y direction
        FIntVector NeighborY = CubePos + FIntVector(0, 1, 0);
        if (VertexMap.Contains(NeighborY))
        {
            CreateQuadBetweenCubes(CubePos, NeighborY, VertexMap, OutTriangles);
        }
        
        // Check +Z direction
        FIntVector NeighborZ = CubePos + FIntVector(0, 0, 1);
        if (VertexMap.Contains(NeighborZ))
        {
            CreateQuadBetweenCubes(CubePos, NeighborZ, VertexMap, OutTriangles);
        }
    }
}

void FSurfaceNets::CreateQuadBetweenCubes(
    const FIntVector& Cube1,
    const FIntVector& Cube2,
    const TMap<FIntVector, int32>& VertexMap,
    TArray<int32>& OutTriangles)
{
    // Get vertex indices for the two cubes
    const int32* Vertex1Ptr = VertexMap.Find(Cube1);
    const int32* Vertex2Ptr = VertexMap.Find(Cube2);
    
    if (!Vertex1Ptr || !Vertex2Ptr)
    {
        return;
    }
    
    int32 V1 = *Vertex1Ptr;
    int32 V2 = *Vertex2Ptr;
    
    // Determine the direction vector between cubes
    FIntVector Direction = Cube2 - Cube1;
    
    // Find the other two vertices needed to form a quad
    FIntVector Offset1, Offset2;
    
    if (Direction.X != 0) // X-direction
    {
        Offset1 = FIntVector(0, 1, 0);
        Offset2 = FIntVector(0, 0, 1);
    }
    else if (Direction.Y != 0) // Y-direction
    {
        Offset1 = FIntVector(1, 0, 0);
        Offset2 = FIntVector(0, 0, 1);
    }
    else if (Direction.Z != 0) // Z-direction
    {
        Offset1 = FIntVector(1, 0, 0);
        Offset2 = FIntVector(0, 1, 0);
    }
    else
    {
        return; // Invalid direction
    }
    
    // Look for the other two vertices of the quad
    FIntVector Cube3 = Cube1 + Offset1;
    FIntVector Cube4 = Cube2 + Offset1;
    
    const int32* Vertex3Ptr = VertexMap.Find(Cube3);
    const int32* Vertex4Ptr = VertexMap.Find(Cube4);
    
    if (Vertex3Ptr && Vertex4Ptr)
    {
        int32 V3 = *Vertex3Ptr;
        int32 V4 = *Vertex4Ptr;
        
        // Create two triangles to form the quad (proper winding order)
        OutTriangles.Add(V1);
        OutTriangles.Add(V3);
        OutTriangles.Add(V2);
        
        OutTriangles.Add(V2);
        OutTriangles.Add(V3);
        OutTriangles.Add(V4);
    }
    
    // Also check the other diagonal direction
    Cube3 = Cube1 + Offset2;
    Cube4 = Cube2 + Offset2;
    
    Vertex3Ptr = VertexMap.Find(Cube3);
    Vertex4Ptr = VertexMap.Find(Cube4);
    
    if (Vertex3Ptr && Vertex4Ptr)
    {
        int32 V3 = *Vertex3Ptr;
        int32 V4 = *Vertex4Ptr;
        
        // Create two triangles to form the quad (proper winding order)
        OutTriangles.Add(V1);
        OutTriangles.Add(V2);
        OutTriangles.Add(V3);
        
        OutTriangles.Add(V2);
        OutTriangles.Add(V4);
        OutTriangles.Add(V3);
    }
}

bool FSurfaceNets::ContainsSurface(const TArray<float>& DensityField, int32 GridSize, int32 x, int32 y, int32 z)
{
    if (x >= GridSize - 1 || y >= GridSize - 1 || z >= GridSize - 1)
    {
        return false;
    }
    
    // Check all 8 corners of the cube
    float Densities[8];
    Densities[0] = GetDensity(DensityField, GridSize, x,     y,     z);
    Densities[1] = GetDensity(DensityField, GridSize, x + 1, y,     z);
    Densities[2] = GetDensity(DensityField, GridSize, x + 1, y + 1, z);
    Densities[3] = GetDensity(DensityField, GridSize, x,     y + 1, z);
    Densities[4] = GetDensity(DensityField, GridSize, x,     y,     z + 1);
    Densities[5] = GetDensity(DensityField, GridSize, x + 1, y,     z + 1);
    Densities[6] = GetDensity(DensityField, GridSize, x + 1, y + 1, z + 1);
    Densities[7] = GetDensity(DensityField, GridSize, x,     y + 1, z + 1);
    
    // Check for sign changes (surface crossing)
    bool HasPositive = false;
    bool HasNegative = false;
    
    for (int32 i = 0; i < 8; i++)
    {
        if (Densities[i] > 0.0f)
        {
            HasPositive = true;
        }
        else if (Densities[i] < 0.0f)
        {
            HasNegative = true;
        }
        
        if (HasPositive && HasNegative)
        {
            return true;
        }
    }
    
    return false;
}

FVector FSurfaceNets::CalculateVertexPosition(
    const TArray<float>& DensityField,
    int32 GridSize,
    int32 x, int32 y, int32 z,
    float VoxelSize,
    const FVector& Origin)
{
    // Get the 8 corner densities
    float Densities[8];
    Densities[0] = GetDensity(DensityField, GridSize, x,     y,     z);
    Densities[1] = GetDensity(DensityField, GridSize, x + 1, y,     z);
    Densities[2] = GetDensity(DensityField, GridSize, x + 1, y + 1, z);
    Densities[3] = GetDensity(DensityField, GridSize, x,     y + 1, z);
    Densities[4] = GetDensity(DensityField, GridSize, x,     y,     z + 1);
    Densities[5] = GetDensity(DensityField, GridSize, x + 1, y,     z + 1);
    Densities[6] = GetDensity(DensityField, GridSize, x + 1, y + 1, z + 1);
    Densities[7] = GetDensity(DensityField, GridSize, x,     y + 1, z + 1);
    
    // Corner positions relative to cube
    FVector Corners[8] = {
        FVector(0, 0, 0), FVector(1, 0, 0), FVector(1, 1, 0), FVector(0, 1, 0),
        FVector(0, 0, 1), FVector(1, 0, 1), FVector(1, 1, 1), FVector(0, 1, 1)
    };
    
    // Calculate weighted average position (Surface Nets smoothing)
    FVector WeightedSum = FVector::ZeroVector;
    float TotalWeight = 0.0f;
    
    for (int32 i = 0; i < 8; i++)
    {
        // Weight by inverse distance to surface (closer to zero = higher weight)
        float Weight = 1.0f / (FMath::Abs(Densities[i]) + 0.001f);
        WeightedSum += Corners[i] * Weight;
        TotalWeight += Weight;
    }
    
    FVector LocalPos = WeightedSum / TotalWeight;
    
    // Convert to world position
    return Origin + FVector(x + LocalPos.X, y + LocalPos.Y, z + LocalPos.Z) * VoxelSize;
}

FVector FSurfaceNets::CalculateGradient(const TArray<float>& DensityField, int32 GridSize, int32 x, int32 y, int32 z)
{
    FVector Gradient;
    
    // Calculate gradient using central differences
    float StepSize = 1.0f;
    
    Gradient.X = GetDensity(DensityField, GridSize, x + 1, y, z) - GetDensity(DensityField, GridSize, x - 1, y, z);
    Gradient.Y = GetDensity(DensityField, GridSize, x, y + 1, z) - GetDensity(DensityField, GridSize, x, y - 1, z);
    Gradient.Z = GetDensity(DensityField, GridSize, x, y, z + 1) - GetDensity(DensityField, GridSize, x, y, z - 1);
    
    return -Gradient; // Negative for outward-pointing normals
}

float FSurfaceNets::GetDensity(const TArray<float>& DensityField, int32 GridSize, int32 x, int32 y, int32 z)
{
    // Clamp coordinates to valid range
    x = FMath::Clamp(x, 0, GridSize - 1);
    y = FMath::Clamp(y, 0, GridSize - 1);
    z = FMath::Clamp(z, 0, GridSize - 1);
    
    int32 Index = x + y * GridSize + z * GridSize * GridSize;
    
    if (Index >= 0 && Index < DensityField.Num())
    {
        return DensityField[Index];
    }
    
    return 1.0f; // Default to "outside" if out of bounds
}

int32 FSurfaceNets::GetVertexIndex(const TArray<int32>& VertexGrid, int32 GridSize, int32 x, int32 y, int32 z)
{
    if (x < 0 || y < 0 || z < 0 || x >= GridSize || y >= GridSize || z >= GridSize)
    {
        return -1;
    }
    
    int32 Index = x + y * GridSize + z * GridSize * GridSize;
    
    if (Index >= 0 && Index < VertexGrid.Num())
    {
        return VertexGrid[Index];
    }
    
    return -1;
}