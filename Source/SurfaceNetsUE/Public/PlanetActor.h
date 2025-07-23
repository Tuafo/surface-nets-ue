#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "PlanetChunk.h"  // Include the complete definition
#include "PlanetActor.generated.h"

class UNoiseGenerator;

UCLASS(BlueprintType, Blueprintable)
class SURFACENETSUE_API APlanetActor : public AActor
{
    GENERATED_BODY()
    
public:    
    APlanetActor();

protected:
    virtual void BeginPlay() override;

public:
    /** Planet radius in world units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    float PlanetRadius = 1000.0f;
    
    /** Size of each chunk in world units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    float ChunkSize = 64.0f;
    
    /** Number of chunks per axis (creates ChunksPerAxis^3 total chunks) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    int32 ChunksPerAxis = 16;
    
    /** Number of voxels per chunk (base resolution) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    int32 VoxelsPerChunk = 16;
    
    /** Enable collision for generated meshes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    bool bEnableCollision = false;
    
    /** Material to apply to planet surface */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
    UMaterialInterface* PlanetMaterial = nullptr;
    
    /** Noise generator for terrain */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet")
    UNoiseGenerator* NoiseGenerator = nullptr;

    /** Initialize or reinitialize the planet with current parameters */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    void InitializePlanet();

private:
    /** Generated planet chunks */
    TArray<TUniquePtr<FPlanetChunk>> PlanetChunks;
    
    /** Mesh components for rendering chunks */
    UPROPERTY()
    TArray<UProceduralMeshComponent*> MeshComponents;
    
    /** Create a new mesh component */
    UProceduralMeshComponent* CreateMeshComponent();
    
    /** Generate all chunks for the planet */
    void GenerateAllChunks();
    
    /** Generate a single chunk at the specified grid position */
    bool GenerateChunk(int32 X, int32 Y, int32 Z, const FVector& ChunkCenter);
};