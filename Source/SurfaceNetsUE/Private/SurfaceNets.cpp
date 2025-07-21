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
    
    // Step 1: Generate vertices for all cubes that contain the surface
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
    
    // Step 2: Generate triangles using proper Surface Nets connectivity
    // For each cube with a vertex, check adjacent cubes and create triangles
    for (const auto& CubeEntry : VertexMap)
    {
        FIntVector CubePos = CubeEntry.Key;
        int32 CenterVertex = CubeEntry.Value;
        
        // Check all 6 face neighbors (+X, +Y, +Z directions)
        TArray<FIntVector> NeighborOffsets = {
            FIntVector(1, 0, 0),  // +X
            FIntVector(0, 1, 0),  // +Y
            FIntVector(0, 0, 1)   // +Z
        };
        
        for (const FIntVector& Offset : NeighborOffsets)
        {
            FIntVector NeighborPos = CubePos + Offset;
            
            // Check if neighbor has a vertex
            if (VertexMap.Contains(NeighborPos))
            {
                int32 NeighborVertex = VertexMap[NeighborPos];
                
                // Create triangles for the shared face between these two cubes
                CreateTrianglesForSharedFace(CubePos, NeighborPos, CenterVertex, NeighborVertex, VertexMap, OutTriangles);
            }
        }
    }
    
    // Remove duplicate and degenerate triangles
    RemoveDuplicateTriangles(OutTriangles);
    
    // Calculate vertex normals using gradient-based approach for better results
    OutNormals.SetNum(OutVertices.Num());
    for (int32 i = 0; i < OutNormals.Num(); i++)
    {
        // Find the cube position for this vertex
        FVector WorldPos = OutVertices[i];
        FVector GridPos = (WorldPos - Origin) / VoxelSize;
        
        int32 CubeX = FMath::RoundToInt(GridPos.X);
        int32 CubeY = FMath::RoundToInt(GridPos.Y);
        int32 CubeZ = FMath::RoundToInt(GridPos.Z);
        
        // Use gradient-based normal calculation
        FVector GradientNormal = CalculateGradient(DensityField, GridSize, CubeX, CubeY, CubeZ);
        OutNormals[i] = GradientNormal.GetSafeNormal();
        
        // Fallback for zero normals
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

void FSurfaceNets::CreateTrianglesForSharedFace(
    const FIntVector& Cube1,
    const FIntVector& Cube2,
    int32 Vertex1,
    int32 Vertex2,
    const TMap<FIntVector, int32>& VertexMap,
    TArray<int32>& OutTriangles)
{
    // This function creates triangles between two adjacent vertices
    // We need to find other nearby vertices to form triangles
    
    // Determine the axis along which the cubes are adjacent
    FIntVector Direction = Cube2 - Cube1;
    
    // Find perpendicular directions to create triangles
    TArray<FIntVector> PerpendicularDirs;
    
    if (FMath::Abs(Direction.X) > 0) // X-axis adjacency
    {
        PerpendicularDirs.Add(FIntVector(0, 1, 0));  // +Y
        PerpendicularDirs.Add(FIntVector(0, -1, 0)); // -Y
        PerpendicularDirs.Add(FIntVector(0, 0, 1));  // +Z
        PerpendicularDirs.Add(FIntVector(0, 0, -1)); // -Z
    }
    else if (FMath::Abs(Direction.Y) > 0) // Y-axis adjacency
    {
        PerpendicularDirs.Add(FIntVector(1, 0, 0));  // +X
        PerpendicularDirs.Add(FIntVector(-1, 0, 0)); // -X
        PerpendicularDirs.Add(FIntVector(0, 0, 1));  // +Z
        PerpendicularDirs.Add(FIntVector(0, 0, -1)); // -Z
    }
    else if (FMath::Abs(Direction.Z) > 0) // Z-axis adjacency
    {
        PerpendicularDirs.Add(FIntVector(1, 0, 0));  // +X
        PerpendicularDirs.Add(FIntVector(-1, 0, 0)); // -X
        PerpendicularDirs.Add(FIntVector(0, 1, 0));  // +Y
        PerpendicularDirs.Add(FIntVector(0, -1, 0)); // -Y
    }
    
    // Look for vertices in perpendicular directions from both cubes
    for (const FIntVector& PerpDir : PerpendicularDirs)
    {
        // Check if there are vertices that form a triangle
        FIntVector ThirdCube1 = Cube1 + PerpDir;
        FIntVector ThirdCube2 = Cube2 + PerpDir;
        
        // Try to form triangle with third vertex from Cube1 direction
        if (VertexMap.Contains(ThirdCube1))
        {
            int32 ThirdVertex = VertexMap[ThirdCube1];
            if (ThirdVertex != Vertex1 && ThirdVertex != Vertex2)
            {
                // Create triangle with proper winding order
                OutTriangles.Add(Vertex1);
                OutTriangles.Add(Vertex2);
                OutTriangles.Add(ThirdVertex);
            }
        }
        
        // Try to form triangle with third vertex from Cube2 direction
        if (VertexMap.Contains(ThirdCube2))
        {
            int32 ThirdVertex = VertexMap[ThirdCube2];
            if (ThirdVertex != Vertex1 && ThirdVertex != Vertex2)
            {
                // Create triangle with proper winding order (reversed for backface)
                OutTriangles.Add(Vertex1);
                OutTriangles.Add(ThirdVertex);
                OutTriangles.Add(Vertex2);
            }
        }
        
        // Try to form quad if both perpendicular vertices exist
        if (VertexMap.Contains(ThirdCube1) && VertexMap.Contains(ThirdCube2))
        {
            int32 ThirdVertex1 = VertexMap[ThirdCube1];
            int32 ThirdVertex2 = VertexMap[ThirdCube2];
            
            if (ThirdVertex1 != Vertex1 && ThirdVertex1 != Vertex2 && 
                ThirdVertex2 != Vertex1 && ThirdVertex2 != Vertex2 && 
                ThirdVertex1 != ThirdVertex2)
            {
                // Create two triangles to form a quad
                // Triangle 1: Vertex1, Vertex2, ThirdVertex1
                OutTriangles.Add(Vertex1);
                OutTriangles.Add(Vertex2);
                OutTriangles.Add(ThirdVertex1);
                
                // Triangle 2: Vertex2, ThirdVertex2, ThirdVertex1
                OutTriangles.Add(Vertex2);
                OutTriangles.Add(ThirdVertex2);
                OutTriangles.Add(ThirdVertex1);
            }
        }
    }
}

void FSurfaceNets::CreateQuadBetweenCubes(
    const FIntVector& Cube1,
    const FIntVector& Cube2,
    const TMap<FIntVector, int32>& VertexMap,
    TArray<int32>& OutTriangles)
{
    // Get the vertices for the two cubes
    const int32* V1 = VertexMap.Find(Cube1);
    const int32* V2 = VertexMap.Find(Cube2);
    
    if (!V1 || !V2)
    {
        return; // One or both cubes don't have vertices
    }
    
    FIntVector Diff = Cube2 - Cube1;
    
    // Find two more vertices that complete the quad
    TArray<int32> QuadVertices;
    QuadVertices.Add(*V1);
    QuadVertices.Add(*V2);
    
    if (Diff.X == 1 && Diff.Y == 0 && Diff.Z == 0) // +X face
    {
        // Look for vertices that share the X face
        FIntVector V3Pos = Cube1 + FIntVector(0, 1, 0);
        FIntVector V4Pos = Cube1 + FIntVector(0, 0, 1);
        
        const int32* V3 = VertexMap.Find(V3Pos);
        const int32* V4 = VertexMap.Find(V4Pos);
        
        if (V3 && V4)
        {
            QuadVertices.Add(*V3);
            QuadVertices.Add(*V4);
        }
    }
    else if (Diff.Y == 1 && Diff.X == 0 && Diff.Z == 0) // +Y face
    {
        // Look for vertices that share the Y face
        FIntVector V3Pos = Cube1 + FIntVector(1, 0, 0);
        FIntVector V4Pos = Cube1 + FIntVector(0, 0, 1);
        
        const int32* V3 = VertexMap.Find(V3Pos);
        const int32* V4 = VertexMap.Find(V4Pos);
        
        if (V3 && V4)
        {
            QuadVertices.Add(*V3);
            QuadVertices.Add(*V4);
        }
    }
    else if (Diff.Z == 1 && Diff.X == 0 && Diff.Y == 0) // +Z face
    {
        // Look for vertices that share the Z face
        FIntVector V3Pos = Cube1 + FIntVector(1, 0, 0);
        FIntVector V4Pos = Cube1 + FIntVector(0, 1, 0);
        
        const int32* V3 = VertexMap.Find(V3Pos);
        const int32* V4 = VertexMap.Find(V4Pos);
        
        if (V3 && V4)
        {
            QuadVertices.Add(*V3);
            QuadVertices.Add(*V4);
        }
    }
    
    // If we have exactly 4 vertices, create two triangles with consistent winding
    if (QuadVertices.Num() == 4)
    {
        // Triangle 1: V0, V1, V2
        OutTriangles.Add(QuadVertices[0]);
        OutTriangles.Add(QuadVertices[1]);
        OutTriangles.Add(QuadVertices[2]);
        
        // Triangle 2: V0, V2, V3
        OutTriangles.Add(QuadVertices[0]);
        OutTriangles.Add(QuadVertices[2]);
        OutTriangles.Add(QuadVertices[3]);
    }
    else if (QuadVertices.Num() == 2)
    {
        // If we only have 2 vertices, try to find any third vertex to create a triangle
        // This handles edge cases where the quad approach doesn't work
        
        // Look for nearby vertices to complete at least one triangle
        TArray<FIntVector> SearchPositions = {
            Cube1 + FIntVector(1, 0, 0), Cube1 + FIntVector(-1, 0, 0),
            Cube1 + FIntVector(0, 1, 0), Cube1 + FIntVector(0, -1, 0),
            Cube1 + FIntVector(0, 0, 1), Cube1 + FIntVector(0, 0, -1),
            Cube2 + FIntVector(1, 0, 0), Cube2 + FIntVector(-1, 0, 0),
            Cube2 + FIntVector(0, 1, 0), Cube2 + FIntVector(0, -1, 0),
            Cube2 + FIntVector(0, 0, 1), Cube2 + FIntVector(0, 0, -1)
        };
        
        for (const FIntVector& SearchPos : SearchPositions)
        {
            const int32* V3 = VertexMap.Find(SearchPos);
            if (V3 && *V3 != *V1 && *V3 != *V2)
            {
                // Create triangle with consistent winding
                OutTriangles.Add(*V1);
                OutTriangles.Add(*V2);
                OutTriangles.Add(*V3);
                break; // Only create one triangle per pair
            }
        }
    }
}