#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "PlanetActor.generated.h"

class UNoiseGenerator;
struct FPlanetChunk;

/**
 * Main planet actor that manages procedural sphere generation using Surface Nets
 * Based on the fast-surface-nets-rs implementation for seamless chunk boundaries
 */
UCLASS(BlueprintType, Blueprintable)
class SURFACENETSUE_API APlanetActor : public AActor
{
    GENERATED_BODY()
    
public:    
    APlanetActor();

protected:
    virtual void BeginPlay() override;

public:    
    virtual void Tick(float DeltaTime) override;

    /** Root scene component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootSceneComponent;
    
    /** Noise generator for terrain */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNoiseGenerator* NoiseGenerator;
    
    /** Procedural mesh components for chunks */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<UProceduralMeshComponent*> MeshComponents;

    /** Planet radius */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    float PlanetRadius = 1000.0f;
    
    /** Chunk size for dividing the sphere (recommended: 64.0f for good detail) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    float ChunkSize = 64.0f;
    
    /** Number of chunks per axis (creates ChunksPerAxis^3 total grid, but only sphere-intersecting chunks are generated) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    int32 ChunksPerAxis = 16;
    
    /** Base voxel resolution per chunk (will be padded to VoxelsPerChunk+2 for seamless boundaries) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    int32 VoxelsPerChunk = 16;
    
    /** Material to apply to planet chunks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
    UMaterialInterface* PlanetMaterial;
    
    /** Enable collision for planet chunks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    bool bEnableCollision = true;

    /** Initialize the planet with current parameters */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    void InitializePlanet();
    
    /** Generate all chunks for the sphere (only generates chunks that intersect sphere surface) */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    void GenerateAllChunks();

private:
    /** Array of planet chunks */
    TArray<TUniquePtr<FPlanetChunk>> PlanetChunks;
    
    /** Create a new mesh component at runtime */
    UProceduralMeshComponent* CreateMeshComponent();
    
    /** Generate chunk at specific grid position with given world center */
    void GenerateChunk(int32 X, int32 Y, int32 Z, const FVector& ChunkCenter);
};