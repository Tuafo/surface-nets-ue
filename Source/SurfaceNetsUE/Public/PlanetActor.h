#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "OctreeComponent.h"
#include "NoiseGenerator.h"
#include "PlanetActor.generated.h"

// Forward declarations
struct FPlanetChunk;

UCLASS(BlueprintType, Blueprintable)
class SURFACENETSUE_API APlanetActor : public AActor
{
    GENERATED_BODY()

public:
    APlanetActor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /** Octree component for LOD management */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet")
    UOctreeComponent* OctreeComponent;

    /** Noise generator for terrain */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet")
    UNoiseGenerator* NoiseGenerator;

    /** Root mesh component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet")
    USceneComponent* RootSceneComponent;

    /** Pool of procedural mesh components for rendering chunks */
    UPROPERTY()
    TArray<UProceduralMeshComponent*> MeshComponentPool;

    /** Material to apply to planet chunks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    UMaterialInterface* PlanetMaterial;

    /** How often to update LOD (in seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    float LODUpdateInterval = 0.1f;

    /** Timer for LOD updates */
    float LODUpdateTimer = 0.0f;

    /** Currently active chunk-to-mesh mappings */
    TMap<FPlanetChunk*, UProceduralMeshComponent*> ActiveChunkMeshes;

public:
    /** Update planet LOD based on camera position */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    void UpdatePlanetLOD(const FVector& CameraPosition);

    /** Initialize the planet */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    void InitializePlanet();

protected:
    /** Get or create a mesh component from the pool */
    UProceduralMeshComponent* GetMeshFromPool();

    /** Return a mesh component to the pool */
    void ReturnMeshToPool(UProceduralMeshComponent* MeshComponent);

    /** Update mesh rendering for active chunks */
    void UpdateChunkMeshes();

    /** Generate mesh for a chunk */
    void GenerateChunkMesh(FPlanetChunk* Chunk, UProceduralMeshComponent* MeshComponent);
};