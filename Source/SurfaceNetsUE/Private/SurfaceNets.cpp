#include "SurfaceNets.h"
#include "SurfaceNetsUE.h"

// Define cube face vertex indices for Surface Nets
const FIntVector FSurfaceNets::CubeFaces[6][4] = {
    // Face 0: +X
    { FIntVector(1,0,0), FIntVector(1,1,0), FIntVector(1,1,1), FIntVector(1,0,1) },
    // Face 1: -X  
    { FIntVector(0,0,0), FIntVector(0,0,1), FIntVector(0,1,1), FIntVector(0,1,0) },
    // Face 2: +Y
    { FIntVector(0,1,0), FIntVector(0,1,1), FIntVector(1,1,1), FIntVector(1,1,0) },
    // Face 3: -Y
    { FIntVector(0,0,0), FIntVector(1,0,0), FIntVector(1,0,1), FIntVector(0,0,1) },
    // Face 4: +Z
    { FIntVector(0,0,1), FIntVector(1,0,1), FIntVector(1,1,1), FIntVector(0,1,1) },
    // Face 5: -Z
    { FIntVector(0,0,0), FIntVector(0,1,0), FIntVector(1,1,0), FIntVector(1,0,0) }
};

void FSurfaceNets::GenerateMesh(
    const TArray<float>& DensityField,
    int32 GridSize,
    float VoxelSize,
    const FVector& Origin,
    TArray<FVector>& OutVertices,
    TArray<int32>& OutTriangles,
    TArray<FVector>& OutNormals
)
{
    OutVertices.Empty();
    OutTriangles.Empty();
    OutNormals.Empty();
    
    // Phase 1: Find all surface vertices (like estimate_surface in Rust)
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
                    
                    int32 GridIndex = z * GridSize * GridSize + y * GridSize + x;
                    VertexGrid[GridIndex] = VertexIndex;
                }
            }
        }
    }
    
    // Phase 2: Create quads (like make_all_quads in Rust)
    MakeAllQuads(DensityField, GridSize, VertexMap, VertexGrid, OutTriangles);
}

void FSurfaceNets::MakeAllQuads(
    const TArray<float>& DensityField,
    int32 GridSize,
    const TMap<FIntVector, int32>& VertexMap,
    const TArray<int32>& VertexGrid,
    TArray<int32>& OutTriangles
)
{
    // For each surface vertex, check 3 directions and create quads
    for (const auto& VertexPair : VertexMap)
    {
        const FIntVector& CubePos = VertexPair.Key;
        int32 x = CubePos.X;
        int32 y = CubePos.Y;
        int32 z = CubePos.Z;
        
        // X-axis edge (like the Rust implementation)
        if (y > 0 && z > 0 && x < GridSize - 2)
        {
            MaybeCreateQuad(
                DensityField, GridSize, VertexGrid,
                CubePos, 
                CubePos + FIntVector(1, 0, 0), // Adjacent cube in X direction
                FIntVector(0, -1, 0), // B axis (Y negative)
                FIntVector(0, 0, -1), // C axis (Z negative)
                OutTriangles
            );
        }
        
        // Y-axis edge
        if (x > 0 && z > 0 && y < GridSize - 2)
        {
            MaybeCreateQuad(
                DensityField, GridSize, VertexGrid,
                CubePos,
                CubePos + FIntVector(0, 1, 0), // Adjacent cube in Y direction
                FIntVector(-1, 0, 0), // B axis (X negative)
                FIntVector(0, 0, -1), // C axis (Z negative)
                OutTriangles
            );
        }
        
        // Z-axis edge
        if (x > 0 && y > 0 && z < GridSize - 2)
        {
            MaybeCreateQuad(
                DensityField, GridSize, VertexGrid,
                CubePos,
                CubePos + FIntVector(0, 0, 1), // Adjacent cube in Z direction
                FIntVector(-1, 0, 0), // B axis (X negative)
                FIntVector(0, -1, 0), // C axis (Y negative)
                OutTriangles
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
    TArray<int32>& OutTriangles
)
{
    // Check if there's a surface crossing between P1 and P2
    float D1 = GetDensity(DensityField, GridSize, P1.X, P1.Y, P1.Z);
    float D2 = GetDensity(DensityField, GridSize, P2.X, P2.Y, P2.Z);
    
    // Must have opposite signs (surface crossing)
    if ((D1 < 0.0f) == (D2 < 0.0f)) return;
    
    bool NegativeFace = (D1 > 0.0f && D2 < 0.0f);
    
    // Get the 4 quad vertices (matching Rust layout)
    int32 V1 = GetVertexIndex(VertexGrid, GridSize, P1.X, P1.Y, P1.Z);
    int32 V2 = GetVertexIndex(VertexGrid, GridSize, P1.X + AxisB.X, P1.Y + AxisB.Y, P1.Z + AxisB.Z);
    int32 V3 = GetVertexIndex(VertexGrid, GridSize, P1.X + AxisC.X, P1.Y + AxisC.Y, P1.Z + AxisC.Z);
    int32 V4 = GetVertexIndex(VertexGrid, GridSize, P1.X + AxisB.X + AxisC.X, P1.Y + AxisB.Y + AxisC.Y, P1.Z + AxisB.Z + AxisC.Z);
    
    // All vertices must exist
    if (V1 == -1 || V2 == -1 || V3 == -1 || V4 == -1) return;
    
    // Create 2 triangles with proper winding (from Rust implementation)
    if (NegativeFace)
    {
        // Triangle 1
        OutTriangles.Add(V1);
        OutTriangles.Add(V4);
        OutTriangles.Add(V2);
        
        // Triangle 2
        OutTriangles.Add(V1);
        OutTriangles.Add(V3);
        OutTriangles.Add(V4);
    }
    else
    {
        // Triangle 1
        OutTriangles.Add(V1);
        OutTriangles.Add(V2);
        OutTriangles.Add(V4);
        
        // Triangle 2
        OutTriangles.Add(V1);
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
    // Calculate centroid of edge intersections (like Rust implementation)
    FVector Sum = FVector::ZeroVector;
    int32 Count = 0;
    
    // Check all 12 cube edges for surface crossings
    const FIntVector CubeCorners[8] = {
        FIntVector(0,0,0), FIntVector(1,0,0), FIntVector(0,1,0), FIntVector(1,1,0),
        FIntVector(0,0,1), FIntVector(1,0,1), FIntVector(0,1,1), FIntVector(1,1,1)
    };
    
    const int32 CubeEdges[12][2] = {
        {0,1}, {2,3}, {4,5}, {6,7}, // X-axis edges
        {0,2}, {1,3}, {4,6}, {5,7}, // Y-axis edges
        {0,4}, {1,5}, {2,6}, {3,7}  // Z-axis edges
    };
    
    for (int32 i = 0; i < 12; i++)
    {
        FIntVector Corner1 = CubeCorners[CubeEdges[i][0]];
        FIntVector Corner2 = CubeCorners[CubeEdges[i][1]];
        
        float D1 = GetDensity(DensityField, GridSize, x + Corner1.X, y + Corner1.Y, z + Corner1.Z);
        float D2 = GetDensity(DensityField, GridSize, x + Corner2.X, y + Corner2.Y, z + Corner2.Z);
        
        // Check for surface crossing
        if ((D1 < 0.0f) != (D2 < 0.0f))
        {
            // Interpolate along edge
            float Interp1 = D1 / (D1 - D2);
            float Interp2 = 1.0f - Interp1;
            
            FVector EdgePoint = Interp2 * FVector(Corner1.X, Corner1.Y, Corner1.Z) + 
                               Interp1 * FVector(Corner2.X, Corner2.Y, Corner2.Z);
            
            Sum += EdgePoint;
            Count++;
        }
    }
    
    FVector RelativePos = (Count > 0) ? Sum / Count : FVector(0.5f, 0.5f, 0.5f);
    return Origin + FVector(x + RelativePos.X, y + RelativePos.Y, z + RelativePos.Z) * VoxelSize;
}

FVector FSurfaceNets::CalculateGradient(
    const TArray<float>& DensityField,
    int32 GridSize,
    int32 x, int32 y, int32 z)
{
    FVector Gradient = FVector::ZeroVector;
    
    // Calculate finite difference gradient
    Gradient.X = GetDensity(DensityField, GridSize, x + 1, y, z) - GetDensity(DensityField, GridSize, x - 1, y, z);
    Gradient.Y = GetDensity(DensityField, GridSize, x, y + 1, z) - GetDensity(DensityField, GridSize, x, y - 1, z);
    Gradient.Z = GetDensity(DensityField, GridSize, x, y, z + 1) - GetDensity(DensityField, GridSize, x, y, z - 1);
    
    return Gradient;
}

float FSurfaceNets::GetDensity(const TArray<float>& DensityField, int32 GridSize, int32 x, int32 y, int32 z)
{
    // Bounds checking
    if (x < 0 || x >= GridSize || y < 0 || y >= GridSize || z < 0 || z >= GridSize)
    {
        return 1.0f; // Default to solid outside bounds
    }
    
    int32 Index = x + y * GridSize + z * GridSize * GridSize;
    return DensityField[Index];
}

bool FSurfaceNets::ContainsSurface(const TArray<float>& DensityField, int32 GridSize, int32 x, int32 y, int32 z)
{
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
    
    // Check if there's a sign change (surface crossing)
    bool HasPositive = false, HasNegative = false;
    for (int32 i = 0; i < 8; i++)
    {
        if (Densities[i] > 0.0f) HasPositive = true;
        if (Densities[i] < 0.0f) HasNegative = true;
    }
    
    return HasPositive && HasNegative;
}

int32 FSurfaceNets::GetOrCreateVertex(int32 x, int32 y, int32 z, TMap<FIntVector, int32>& VertexMap)
{
    FIntVector Key(x, y, z);
    if (int32* ExistingIndex = VertexMap.Find(Key))
    {
        return *ExistingIndex;
    }
    
    int32 NewIndex = VertexMap.Num();
    VertexMap.Add(Key, NewIndex);
    return NewIndex;
}

int32 FSurfaceNets::GetVertexIndex(const TArray<int32>& VertexGrid, int32 GridSize, int32 x, int32 y, int32 z)
{
    if (x < 0 || x >= GridSize || y < 0 || y >= GridSize || z < 0 || z >= GridSize)
    {
        return -1;
    }
    
    int32 GridIndex = x + y * GridSize + z * GridSize * GridSize;
    return VertexGrid[GridIndex];
}
