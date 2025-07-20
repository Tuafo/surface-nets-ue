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
    TArray<FVector>& OutNormals)
{
    OutVertices.Empty();
    OutTriangles.Empty();
    OutNormals.Empty();
    
    TMap<FIntVector, int32> VertexMap;
    
    // Generate vertices for all cubes that contain the surface
    for (int32 z = 0; z < GridSize - 1; z++)
    {
        for (int32 y = 0; y < GridSize - 1; y++)
        {
            for (int32 x = 0; x < GridSize - 1; x++)
            {
                if (ContainsSurface(DensityField, GridSize, x, y, z))
                {
                    FVector VertexPos = CalculateVertexPosition(DensityField, GridSize, x, y, z, VoxelSize, Origin);
                    int32 VertexIndex = OutVertices.Num();
                    OutVertices.Add(VertexPos);
                    VertexMap.Add(FIntVector(x, y, z), VertexIndex);
                }
            }
        }
    }
    
    // Generate triangles using a more robust approach for spherical surfaces
    // We'll create triangles by connecting adjacent vertices in a way that creates a closed surface
    for (auto& VertexPair : VertexMap)
    {
        FIntVector Pos = VertexPair.Key;
        int32 CurrentVertex = VertexPair.Value;
        
        // Try to form triangles with neighboring vertices
        TArray<int32> Neighbors;
        
        // Check all 26 possible neighbors (3x3x3 - 1 for center)
        for (int32 dz = -1; dz <= 1; dz++)
        {
            for (int32 dy = -1; dy <= 1; dy++)
            {
                for (int32 dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0 && dz == 0) continue;
                    
                    FIntVector NeighborPos = Pos + FIntVector(dx, dy, dz);
                    if (int32* NeighborIndex = VertexMap.Find(NeighborPos))
                    {
                        Neighbors.Add(*NeighborIndex);
                    }
                }
            }
        }
        
        // Create triangles from the current vertex to pairs of neighbors
        // This creates a fan-like triangulation around each vertex
        for (int32 i = 0; i < Neighbors.Num(); i++)
        {
            for (int32 j = i + 1; j < Neighbors.Num(); j++)
            {
                // Only create triangles if vertices form a reasonable triangle
                FVector V0 = OutVertices[CurrentVertex];
                FVector V1 = OutVertices[Neighbors[i]];
                FVector V2 = OutVertices[Neighbors[j]];
                
                // Check if triangle has reasonable area and isn't degenerate
                FVector Edge1 = V1 - V0;
                FVector Edge2 = V2 - V0;
                float TriangleArea = FVector::CrossProduct(Edge1, Edge2).Size();
                
                if (TriangleArea > 0.01f * VoxelSize * VoxelSize) // Minimum area threshold
                {
                    // Add triangle with consistent winding
                    OutTriangles.Add(CurrentVertex);
                    OutTriangles.Add(Neighbors[i]);
                    OutTriangles.Add(Neighbors[j]);
                }
            }
        }
    }
    
    // Remove duplicate and degenerate triangles
    RemoveDuplicateTriangles(OutTriangles);
    
    // Calculate vertex normals
    OutNormals.SetNum(OutVertices.Num());
    for (int32 i = 0; i < OutNormals.Num(); i++)
    {
        OutNormals[i] = FVector::ZeroVector;
    }
    
    // Accumulate face normals to vertex normals
    for (int32 i = 0; i < OutTriangles.Num(); i += 3)
    {
        int32 V0 = OutTriangles[i];
        int32 V1 = OutTriangles[i + 1];
        int32 V2 = OutTriangles[i + 2];
        
        if (V0 < OutVertices.Num() && V1 < OutVertices.Num() && V2 < OutVertices.Num())
        {
            FVector Edge1 = OutVertices[V1] - OutVertices[V0];
            FVector Edge2 = OutVertices[V2] - OutVertices[V0];
            FVector FaceNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
            
            OutNormals[V0] += FaceNormal;
            OutNormals[V1] += FaceNormal;
            OutNormals[V2] += FaceNormal;
        }
    }
    
    // Normalize accumulated normals with gradient fallback
    for (int32 i = 0; i < OutNormals.Num(); i++)
    {
        if (OutNormals[i].IsNearlyZero())
        {
            // Use gradient-based normal for isolated vertices
            FVector WorldPos = OutVertices[i];
            FVector GridPos = (WorldPos - Origin) / VoxelSize;
            
            FVector GradientNormal = CalculateGradient(DensityField, GridSize, 
                FMath::RoundToInt(GridPos.X), 
                FMath::RoundToInt(GridPos.Y), 
                FMath::RoundToInt(GridPos.Z));
            
            OutNormals[i] = GradientNormal.GetSafeNormal();
        }
        else
        {
            OutNormals[i] = OutNormals[i].GetSafeNormal();
        }
        
        // Final fallback
        if (OutNormals[i].IsNearlyZero())
        {
            OutNormals[i] = FVector::UpVector;
        }
    }
}

FVector FSurfaceNets::CalculateVertexPosition(
    const TArray<float>& DensityField,
    int32 GridSize,
    int32 x, int32 y, int32 z,
    float VoxelSize,
    const FVector& Origin)
{
    // Simple Surface Nets: place vertex at cube center, then adjust based on density gradient
    FVector CubeCenter = Origin + FVector(x + 0.5f, y + 0.5f, z + 0.5f) * VoxelSize;
    
    // Calculate gradient for smoothing
    FVector Gradient = CalculateGradient(DensityField, GridSize, x, y, z);
    
    // Adjust position slightly based on gradient (Surface Nets smoothing)
    float SmoothingFactor = 0.1f;
    CubeCenter += Gradient * SmoothingFactor * VoxelSize;
    
    return CubeCenter;
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

void FSurfaceNets::ConnectToNeighbor(
    const TArray<int32>& VertexGrid, 
    int32 GridSize, 
    int32 x1, int32 y1, int32 z1,
    int32 x2, int32 y2, int32 z2,
    TArray<int32>& OutTriangles)
{
    int32 V1 = GetVertexIndex(VertexGrid, GridSize, x1, y1, z1);
    int32 V2 = GetVertexIndex(VertexGrid, GridSize, x2, y2, z2);
    
    if (V1 == -1 || V2 == -1) return;
    
    // Find a third vertex to form a triangle
    // Try different combinations of neighbors
    TArray<FIntVector> PotentialThirdVertices = {
        FIntVector(x1+1, y1, z1), FIntVector(x1-1, y1, z1),
        FIntVector(x1, y1+1, z1), FIntVector(x1, y1-1, z1),
        FIntVector(x1, y1, z1+1), FIntVector(x1, y1, z1-1),
        FIntVector(x2+1, y2, z2), FIntVector(x2-1, y2, z2),
        FIntVector(x2, y2+1, z2), FIntVector(x2, y2-1, z2),
        FIntVector(x2, y2, z2+1), FIntVector(x2, y2, z2-1)
    };
    
    for (const FIntVector& ThirdPos : PotentialThirdVertices)
    {
        int32 V3 = GetVertexIndex(VertexGrid, GridSize, ThirdPos.X, ThirdPos.Y, ThirdPos.Z);
        if (V3 != -1 && V3 != V1 && V3 != V2)
        {
            // Create triangle with consistent winding order
            OutTriangles.Add(V1);
            OutTriangles.Add(V2);
            OutTriangles.Add(V3);
            break; // Only create one triangle per connection
        }
    }
}

void FSurfaceNets::RemoveDuplicateTriangles(TArray<int32>& Triangles)
{
    TSet<FIntVector> UniqueTriangles;
    TArray<int32> FilteredTriangles;
    
    for (int32 i = 0; i < Triangles.Num(); i += 3)
    {
        if (i + 2 >= Triangles.Num()) break;
        
        int32 V0 = Triangles[i];
        int32 V1 = Triangles[i + 1];
        int32 V2 = Triangles[i + 2];
        
        // Skip degenerate triangles
        if (V0 == V1 || V1 == V2 || V0 == V2) continue;
        
        // Sort vertices to create canonical representation
        TArray<int32> SortedVerts = {V0, V1, V2};
        SortedVerts.Sort();
        
        FIntVector TriangleKey(SortedVerts[0], SortedVerts[1], SortedVerts[2]);
        
        if (!UniqueTriangles.Contains(TriangleKey))
        {
            UniqueTriangles.Add(TriangleKey);
            FilteredTriangles.Add(V0);
            FilteredTriangles.Add(V1);
            FilteredTriangles.Add(V2);
        }
    }
    
    Triangles = FilteredTriangles;
}