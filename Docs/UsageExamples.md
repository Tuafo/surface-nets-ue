# Usage Examples

## Basic Planet Setup

### 1. Creating a Simple Planet

```cpp
// In C++
UWorld* World = GetWorld();
APlanetActor* Planet = World->SpawnActor<APlanetActor>();

// Configure basic parameters
Planet->PlanetRadius = 1000.0f;
Planet->NoiseGenerator->NoiseScale = 0.01f;
Planet->NoiseGenerator->NoiseAmplitude = 100.0f;

// Initialize the planet
Planet->InitializePlanet();
```

### 2. Blueprint Setup

1. Drag `APlanetActor` into your level
2. In the Details panel, configure:
   - **Planet Radius**: 1000.0
   - **Noise Generator → Noise Scale**: 0.01
   - **Noise Generator → Noise Amplitude**: 100.0
   - **Octree Component → LOD Distances**: [100, 300, 800, 2000, 5000]

### 3. Runtime Parameter Adjustment

```cpp
// Blueprint callable functions
UFUNCTION(BlueprintCallable, Category = "Planet")
void UpdatePlanetParameters(float NewRadius, float NewNoiseScale, float NewAmplitude)
{
    if (PlanetActor && PlanetActor->NoiseGenerator)
    {
        PlanetActor->PlanetRadius = NewRadius;
        PlanetActor->NoiseGenerator->PlanetRadius = NewRadius;
        PlanetActor->NoiseGenerator->NoiseScale = NewNoiseScale;
        PlanetActor->NoiseGenerator->NoiseAmplitude = NewAmplitude;
        
        // Reinitialize to apply changes
        PlanetActor->InitializePlanet();
    }
}
```

## Advanced Configuration

### Custom Noise Generator

```cpp
UCLASS(BlueprintType, Blueprintable)
class YOURPROJECT_API UCustomNoiseGenerator : public UNoiseGenerator
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom")
    float CustomParameter = 1.0f;

    virtual float SampleDensity(const FVector& WorldPosition) const override
    {
        // Custom density function
        float baseDistance = WorldPosition.Size() - PlanetRadius;
        
        // Add multiple noise layers
        float noise1 = FractalNoise(WorldPosition * 0.01f) * 100.0f;
        float noise2 = FractalNoise(WorldPosition * 0.05f) * 20.0f;
        float noise3 = FractalNoise(WorldPosition * 0.1f) * 5.0f;
        
        float totalNoise = noise1 + noise2 + noise3;
        
        return -(baseDistance + totalNoise * CustomParameter);
    }
};
```

### Performance Tuning

```cpp
// Optimize for different scenarios

// For close-up view (high detail)
OctreeComponent->MaxDepth = 8;
OctreeComponent->LODDistances = {50, 150, 400, 1000, 2500};
OctreeComponent->UpdateFrequency = 0.05f;

// For distant view (performance)
OctreeComponent->MaxDepth = 5;
OctreeComponent->LODDistances = {200, 600, 1500, 4000, 10000};
OctreeComponent->UpdateFrequency = 0.2f;

// For very large planets
OctreeComponent->RootSize = 10000.0f;
OctreeComponent->bEnableFrustumCulling = true;
```

### Material Setup

```cpp
// Assign material in code
if (UMaterial* PlanetMat = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_Planet")))
{
    PlanetActor->PlanetMaterial = PlanetMat;
}

// Or in Blueprint constructor
static ConstructorHelpers::FObjectFinder<UMaterial> PlanetMaterialAsset(
    TEXT("/Game/Materials/M_Planet"));
if (PlanetMaterialAsset.Succeeded())
{
    PlanetMaterial = PlanetMaterialAsset.Object;
}
```

## LOD Configuration Examples

### Scenario 1: Spaceship Approaching Planet
```cpp
// Long-range distances for space exploration
TArray<float> SpaceLODDistances = {500.0f, 2000.0f, 8000.0f, 20000.0f, 50000.0f};
OctreeComponent->LODDistances = SpaceLODDistances;
OctreeComponent->MaxDepth = 7;
```

### Scenario 2: Character Walking on Surface
```cpp
// Short-range distances for ground exploration  
TArray<float> GroundLODDistances = {25.0f, 75.0f, 200.0f, 500.0f, 1500.0f};
OctreeComponent->LODDistances = GroundLODDistances;
OctreeComponent->MaxDepth = 9;
```

### Scenario 3: Atmospheric Flight
```cpp
// Medium-range distances for flight
TArray<float> FlightLODDistances = {100.0f, 300.0f, 800.0f, 2000.0f, 6000.0f};
OctreeComponent->LODDistances = FlightLODDistances;
OctreeComponent->MaxDepth = 6;
```

## Debugging and Monitoring

### Performance Monitoring

```cpp
// Monitor chunk count and performance
UFUNCTION(BlueprintCallable, Category = "Debug")
void LogPlanetStats()
{
    if (OctreeComponent)
    {
        TArray<FPlanetChunk*> VisibleChunks;
        OctreeComponent->GetVisibleChunks(VisibleChunks);
        
        UE_LOG(LogTemp, Warning, TEXT("Visible Chunks: %d"), VisibleChunks.Num());
        UE_LOG(LogTemp, Warning, TEXT("Active Mesh Components: %d"), UsedMeshComponents.Num());
        UE_LOG(LogTemp, Warning, TEXT("Available Mesh Components: %d"), AvailableMeshComponents.Num());
    }
}
```

### Visual Debug Info

```cpp
// Draw debug information
UFUNCTION(BlueprintCallable, Category = "Debug")
void DrawDebugInfo()
{
    if (OctreeComponent && GetWorld())
    {
        TArray<FPlanetChunk*> VisibleChunks;
        OctreeComponent->GetVisibleChunks(VisibleChunks);
        
        for (FPlanetChunk* Chunk : VisibleChunks)
        {
            if (Chunk)
            {
                // Draw chunk bounds
                FVector ChunkMin = Chunk->Position - FVector(Chunk->Size * 0.5f);
                FVector ChunkMax = Chunk->Position + FVector(Chunk->Size * 0.5f);
                
                DrawDebugBox(GetWorld(), Chunk->Position, 
                    FVector(Chunk->Size * 0.5f), FColor::Green, false, 1.0f);
                
                // Draw LOD level
                DrawDebugString(GetWorld(), Chunk->Position, 
                    FString::Printf(TEXT("LOD: %d"), Chunk->LODLevel), 
                    nullptr, FColor::White, 1.0f);
            }
        }
    }
}
```

## Common Issues and Solutions

### Issue: Planet not visible
**Solution:**
```cpp
// Check camera distance from planet center
FVector CameraPos = GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();
float Distance = FVector::Dist(CameraPos, PlanetActor->GetActorLocation());
UE_LOG(LogTemp, Warning, TEXT("Distance to planet: %f"), Distance);

// Ensure distance is within LOD range
if (Distance > OctreeComponent->LODDistances.Last())
{
    // Camera too far, adjust LOD distances or move closer
}
```

### Issue: Poor performance
**Solution:**
```cpp
// Reduce LOD detail
OctreeComponent->MaxDepth = FMath::Min(OctreeComponent->MaxDepth - 1, 4);

// Increase update frequency
OctreeComponent->UpdateFrequency = FMath::Max(OctreeComponent->UpdateFrequency * 1.5f, 0.5f);

// Enable frustum culling
OctreeComponent->bEnableFrustumCulling = true;
```

### Issue: Terrain too smooth/rough
**Solution:**
```cpp
// Adjust noise parameters
NoiseGenerator->NoiseAmplitude = DesiredRoughness; // Higher = rougher
NoiseGenerator->Octaves = DesiredDetail; // More octaves = more detail
NoiseGenerator->NoiseScale = DesiredFeatureSize; // Smaller = larger features
```