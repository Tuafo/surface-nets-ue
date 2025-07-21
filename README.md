# Surface Nets Planet Generation for Unreal Engine 5.6

A complete planet generation system using Surface Nets mesh generation with octree-based level of detail (LOD) for Unreal Engine 5.6.

## Features

- **Surface Nets Algorithm**: Smooth mesh generation from voxel density fields
- **Octree-based LOD**: Hierarchical spatial subdivision with automatic LOD switching
- **Real-time Planet Generation**: Spherical planets with procedural terrain
- **Performance Optimized**: Multi-threaded mesh generation and memory pooling
- **Blueprint Integration**: Runtime parameter adjustment and configuration
- **Collision Support**: Automatic collision mesh generation
- **Seamless Transitions**: Smooth LOD switching without visual artifacts

## Core Components

### APlanetActor
Main planet actor that manages the entire planet generation system.

**Key Features:**
- Mesh component pooling for performance
- Real-time mesh updates based on camera position
- Material and collision configuration
- Blueprint-exposed parameters

### UOctreeComponent
Manages the octree structure for spatial subdivision and LOD.

**Key Features:**
- Hierarchical octree with configurable depth
- Distance-based LOD switching
- Frustum culling integration
- Dynamic subdivision and merging

### FSurfaceNets
Implements the Surface Nets mesh generation algorithm.

**Key Features:**
- Smooth vertex positioning using gradient-based smoothing
- Efficient triangle generation
- Normal calculation for proper lighting
- Adaptive resolution based on LOD level

### UNoiseGenerator
Procedural noise generation for planet terrain.

**Key Features:**
- Fractal noise with configurable octaves
- Planet radius and amplitude controls
- Density field sampling for Surface Nets
- Blueprint-configurable parameters

### FPlanetChunk
Individual terrain chunk representation.

**Key Features:**
- LOD-based voxel resolution
- Mesh data caching
- Distance-based subdivision/merging decisions
- Efficient memory management

## Getting Started

### Prerequisites
- Unreal Engine 5.6
- Visual Studio 2022 (Windows) or Xcode (Mac)
- C++ development tools

### Setup Instructions

1. **Clone the Repository**
   ```bash
   git clone https://github.com/Tuafo/surface-nets-ue.git
   cd surface-nets-ue
   ```

2. **Generate Project Files**
   - Right-click on `SurfaceNetsUE.uproject`
   - Select "Generate Visual Studio project files"

3. **Build the Project**
   - Open `SurfaceNetsUE.sln` in Visual Studio
   - Build the solution in Development Editor configuration

4. **Launch the Editor**
   - Open `SurfaceNetsUE.uproject` in Unreal Engine 5.6

### Quick Start

1. **Create a Planet**
   - In the editor, drag `APlanetActor` into your level
   - Configure planet parameters in the Details panel

2. **Configure Noise Parameters**
   - Adjust `NoiseGenerator` settings for terrain variation
   - Modify `PlanetRadius`, `NoiseScale`, and `NoiseAmplitude`

3. **Tune LOD Settings**
   - Configure `OctreeComponent` LOD distances
   - Adjust `MaxDepth` and `RootSize` for your use case

4. **Play and Test**
   - Press Play to see real-time planet generation
   - Move around to observe LOD transitions

## Configuration

### Planet Parameters
- **PlanetRadius**: Base radius of the planet sphere
- **NoiseScale**: Scale of terrain noise features
- **NoiseAmplitude**: Height variation amplitude
- **Octaves**: Number of noise octaves for detail
- **Lacunarity**: Frequency multiplier between octaves
- **Persistence**: Amplitude multiplier between octaves

### LOD Configuration
- **MaxDepth**: Maximum octree subdivision depth (4-8 recommended)
- **RootSize**: Size of the root octree node
- **LODDistances**: Array of distances for LOD switching
- **UpdateFrequency**: How often to update LOD (in seconds)

### Performance Settings
- **MaxMeshComponents**: Maximum number of mesh components to pool
- **EnableCollision**: Whether to generate collision meshes
- **EnableFrustumCulling**: Enable camera frustum culling

## Architecture

### Surface Nets Algorithm
The Surface Nets algorithm generates smooth meshes from voxel data:

1. **Density Sampling**: Sample density field at voxel grid points
2. **Surface Detection**: Identify cubes containing the surface
3. **Vertex Placement**: Calculate smooth vertex positions using gradients
4. **Mesh Generation**: Generate triangles connecting adjacent vertices
5. **Normal Calculation**: Compute normals from density gradients

### Octree LOD System
The octree manages spatial subdivision:

1. **Initialization**: Create root node covering the entire planet
2. **Subdivision**: Split nodes based on camera distance
3. **Merging**: Combine distant nodes to reduce detail
4. **Frustum Culling**: Skip invisible nodes
5. **Mesh Updates**: Generate meshes for visible leaf nodes

### Memory Management
Efficient memory usage through:

- **Component Pooling**: Reuse ProceduralMeshComponent instances
- **Chunk Caching**: Cache generated mesh data
- **Dynamic Loading**: Load/unload chunks based on visibility
- **Smart Pointers**: Automatic memory cleanup for octree nodes

## Performance Considerations

### Target Performance
- **60 FPS**: Optimized for real-time use
- **Scalable LOD**: 4-5 LOD levels with smooth transitions
- **Memory Efficient**: Pooled components and cached data
- **Multi-threaded**: Asynchronous mesh generation (future enhancement)

### Optimization Tips
1. **Tune LOD Distances**: Adjust based on your camera movement speed
2. **Limit Max Depth**: Higher depth = more detail but worse performance
3. **Reduce Update Frequency**: Lower frequency for distant planets
4. **Use Frustum Culling**: Enable for large planets
5. **Monitor Memory**: Watch chunk count and mesh complexity

## Blueprint Integration

All major parameters are exposed to Blueprint:

### Planet Actor
- Planet radius and noise parameters
- Material assignment
- Collision configuration
- Performance settings

### Runtime Functions
- `InitializePlanet()`: Reinitialize with new parameters
- `UpdatePlanetMeshes()`: Force mesh update
- `SetNoiseGenerator()`: Change noise configuration

## Extending the System

### Custom Noise Functions
Create custom noise generators by inheriting from `UNoiseGenerator`:

```cpp
UCLASS(BlueprintType, Blueprintable)
class YOURPROJECT_API UCustomNoiseGenerator : public UNoiseGenerator
{
    GENERATED_BODY()

public:
    virtual float SampleDensity(const FVector& WorldPosition) const override;
};
```

### Custom Surface Algorithms
Implement alternative meshing algorithms by modifying `FSurfaceNets`:

```cpp
// Custom vertex placement
FVector CustomVertexPosition(const TArray<float>& DensityField, ...);

// Custom triangle generation
void GenerateCustomMesh(...);
```

### Advanced Features
Future enhancements could include:

- **Terrain Editing**: Real-time voxel modification
- **Multiple Biomes**: Different terrain types
- **Atmospheric Rendering**: Volumetric atmosphere
- **Planetary Physics**: Gravity and orbital mechanics
- **Multi-threading**: Parallel mesh generation

## Troubleshooting

### Common Issues

**Planet not appearing:**
- Check that `APlanetActor` is in the level
- Verify camera is within LOD range
- Ensure `NoiseGenerator` is configured

**Poor performance:**
- Reduce `MaxDepth` in octree settings
- Increase LOD distances
- Lower `UpdateFrequency`
- Enable frustum culling

**Visual artifacts:**
- Check mesh normals are being calculated
- Verify Surface Nets parameters
- Ensure proper LOD transitions

**Compilation errors:**
- Verify UE 5.6 installation
- Check module dependencies in Build.cs
- Regenerate project files

### Debug Logging
Enable verbose logging in `Project Settings > Engine > Logging`:
- Set `LogSurfaceNets` to `Verbose` for detailed output

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

### Code Style
- Follow Unreal Engine coding standards
- Use clear, descriptive variable names
- Add comments for complex algorithms
- Document public APIs

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Surface Nets algorithm based on research by Sarah Frisken
- Octree implementation inspired by UE4/5 hierarchical spatial data structures
- Noise generation techniques from various procedural generation sources

## Support

For questions and support:
- Create an issue on GitHub
- Check the documentation for common solutions
- Review the example configurations