# Surface Nets Algorithm Implementation

## Overview

The Surface Nets algorithm generates smooth meshes from volumetric data (voxels). Unlike Marching Cubes, which can produce sharp features, Surface Nets creates smoother surfaces by positioning vertices within voxel cubes rather than on edges.

## Algorithm Steps

### 1. Density Sampling
For each voxel position (x, y, z), sample the density function:
```cpp
float density = NoiseGenerator->SampleDensity(worldPosition);
```

The density function returns:
- Negative values: Inside the surface (solid)
- Positive values: Outside the surface (air)
- Zero: Exactly on the surface

### 2. Surface Detection
Check each cube (8 voxels) to see if it contains the surface:
```cpp
bool ContainsSurface(int x, int y, int z)
{
    // Check if density values have different signs
    bool hasPositive = false, hasNegative = false;
    for (int i = 0; i < 8; i++) {
        float density = GetDensity(x + corner[i].x, y + corner[i].y, z + corner[i].z);
        if (density > 0) hasPositive = true;
        if (density < 0) hasNegative = true;
    }
    return hasPositive && hasNegative;
}
```

### 3. Vertex Placement
For cubes containing the surface, calculate a smooth vertex position:
```cpp
FVector CalculateVertexPosition(int x, int y, int z)
{
    // Start with cube center
    FVector position = CubeCenter(x, y, z);
    
    // Calculate gradient for smoothing
    FVector gradient = CalculateGradient(x, y, z);
    
    // Adjust position based on gradient
    position += gradient * smoothingFactor;
    
    return position;
}
```

### 4. Mesh Generation
Connect adjacent vertices to form triangles:
- Check neighboring cubes that also contain vertices
- Create quads between adjacent vertices
- Split quads into triangles

### 5. Normal Calculation
Calculate normals using density field gradients:
```cpp
FVector CalculateGradient(int x, int y, int z)
{
    FVector gradient;
    gradient.X = GetDensity(x+1, y, z) - GetDensity(x-1, y, z);
    gradient.Y = GetDensity(x, y+1, z) - GetDensity(x, y-1, z);
    gradient.Z = GetDensity(x, y, z+1) - GetDensity(x, y, z-1);
    return gradient.GetSafeNormal();
}
```

## Advantages

1. **Smooth Surfaces**: Natural smoothing reduces aliasing
2. **Topologically Correct**: Always produces manifold meshes
3. **Adaptive**: Works well with variable resolution
4. **Stable**: Consistent vertex placement

## Comparison with Marching Cubes

| Feature | Surface Nets | Marching Cubes |
|---------|--------------|----------------|
| Surface Quality | Smooth | Can be jagged |
| Vertex Count | Lower | Higher |
| Implementation | Simpler | More complex |
| Sharp Features | Smoothed | Preserved |

## Parameters

### Smoothing Factor
Controls how much vertex positions are adjusted:
- 0.0: No smoothing (cube centers)
- 0.1-0.3: Good balance
- 1.0: Maximum smoothing

### Voxel Resolution
Affects detail level:
- Higher resolution: More detail, more vertices
- Lower resolution: Less detail, better performance

### Density Threshold
The iso-value for surface extraction:
- 0.0: Standard (negative = inside)
- Adjust for different surface definitions

## Performance Considerations

1. **Grid Size**: O(n³) complexity for n³ voxels
2. **Memory**: Store density values efficiently
3. **Caching**: Reuse calculated gradients
4. **LOD**: Use different resolutions at different distances

## Implementation Notes

- Use spatial hashing for vertex lookup
- Consider dual contouring for sharp features
- Implement proper threading for large grids
- Cache mesh data for static regions