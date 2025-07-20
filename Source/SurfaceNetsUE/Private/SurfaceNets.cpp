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
    TArray<FVector> TempVertices;
    
    // Generate vertices for each cube that contains the surface
    for (int32 z = 0; z < GridSize - 1; z++)
    {
        for (int32 y = 0; y < GridSize - 1; y++)
        {
            for (int32 x = 0; x < GridSize - 1; x++)
            {
                if (ContainsSurface(DensityField, GridSize, x, y, z))
                {
                    FVector VertexPos = CalculateVertexPosition(DensityField, GridSize, x, y, z, VoxelSize, Origin);
                    int32 VertexIndex = TempVertices.Num();
                    TempVertices.Add(VertexPos);
                    VertexMap.Add(FIntVector(x, y, z), VertexIndex);
                }
            }
        }
    }
    
    // Generate faces between adjacent cubes
    for (int32 z = 0; z < GridSize - 1; z++)
    {
        for (int32 y = 0; y < GridSize - 1; y++)
        {
            for (int32 x = 0; x < GridSize - 1; x++)
            {
                if (VertexMap.Contains(FIntVector(x, y, z)))
                {
                    int32 CurrentVertex = VertexMap[FIntVector(x, y, z)];
                    
                    // Check adjacent cubes and create quads
                    // +X direction
                    if (x < GridSize - 2 && VertexMap.Contains(FIntVector(x + 1, y, z)))
                    {
                        // Check for vertical neighbors to form quad
                        if (y < GridSize - 2 && 
                            VertexMap.Contains(FIntVector(x, y + 1, z)) &&
                            VertexMap.Contains(FIntVector(x + 1, y + 1, z)))
                        {
                            int32 V0 = CurrentVertex;
                            int32 V1 = VertexMap[FIntVector(x + 1, y, z)];
                            int32 V2 = VertexMap[FIntVector(x + 1, y + 1, z)];
                            int32 V3 = VertexMap[FIntVector(x, y + 1, z)];
                            
                            // Create two triangles for the quad
                            OutTriangles.Add(V0);
                            OutTriangles.Add(V1);
                            OutTriangles.Add(V2);
                            
                            OutTriangles.Add(V0);
                            OutTriangles.Add(V2);
                            OutTriangles.Add(V3);
                        }
                    }
                    
                    // +Z direction
                    if (z < GridSize - 2 && VertexMap.Contains(FIntVector(x, y, z + 1)))
                    {
                        // Check for horizontal neighbors to form quad
                        if (x < GridSize - 2 && 
                            VertexMap.Contains(FIntVector(x + 1, y, z)) &&
                            VertexMap.Contains(FIntVector(x + 1, y, z + 1)))
                        {
                            int32 V0 = CurrentVertex;
                            int32 V1 = VertexMap[FIntVector(x + 1, y, z)];
                            int32 V2 = VertexMap[FIntVector(x + 1, y, z + 1)];
                            int32 V3 = VertexMap[FIntVector(x, y, z + 1)];
                            
                            // Create two triangles for the quad
                            OutTriangles.Add(V0);
                            OutTriangles.Add(V3);
                            OutTriangles.Add(V2);
                            
                            OutTriangles.Add(V0);
                            OutTriangles.Add(V2);
                            OutTriangles.Add(V1);
                        }
                    }
                }
            }
        }
    }
    
    // Copy vertices to output
    OutVertices = TempVertices;
    
    // Calculate normals
    OutNormals.SetNum(OutVertices.Num());
    for (int32 i = 0; i < OutVertices.Num(); i++)
    {
        // Find the corresponding grid position for normal calculation
        FVector WorldPos = OutVertices[i];
        FVector GridPos = (WorldPos - Origin) / VoxelSize;
        
        FVector Normal = CalculateGradient(DensityField, GridSize, 
            FMath::RoundToInt(GridPos.X), 
            FMath::RoundToInt(GridPos.Y), 
            FMath::RoundToInt(GridPos.Z));
        
        OutNormals[i] = Normal.GetSafeNormal();
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