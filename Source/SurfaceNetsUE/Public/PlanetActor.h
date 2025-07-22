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

    /** How often to update LOD (in seconds) - INCREASED for performance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float LODUpdateInterval = 0.5f;

    /** Maximum number of chunks that can be active at once */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Performance", meta = (ClampMin = "10", ClampMax = "200"))
    int32 MaxActiveChunks = 50;

    /** Maximum number of new chunks to generate per frame */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Performance", meta = (ClampMin = "1", ClampMax = "10"))
    int32 MaxChunksPerFrame = 3;

    /** Enable performance monitoring */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Debug")
    bool bEnablePerformanceLogging = false;

    /** Timer for LOD updates */
    float LODUpdateTimer = 0.0f;

    /** Currently active chunk-to-mesh mappings */
    TMap<FPlanetChunk*, UProceduralMeshComponent*> ActiveChunkMeshes;

    /** Chunks waiting to be processed */
    TQueue<TSharedPtr<FPlanetChunk>> PendingChunks;

    /** Last camera position for movement detection */
    FVector LastCameraPosition = FVector::ZeroVector;

    /** Minimum camera movement distance to trigger LOD update */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Performance")
    float MinCameraMovementForUpdate = 100.0f;

public:
    /** Update planet LOD based on camera position */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    void UpdatePlanetLOD(const FVector& CameraPosition);

    /** Initialize the planet */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    void InitializePlanet();

    /** Enable/disable planet LOD updates */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    void SetLODUpdatesEnabled(bool bEnabled);

protected:
    /** Get or create a mesh component from the pool */
    UProceduralMeshComponent* GetMeshFromPool();

    /** Return a mesh component to the pool */
    void ReturnMeshToPool(UProceduralMeshComponent* MeshComponent);

    /** Update mesh rendering for active chunks */
    void UpdateChunkMeshes();

    /** Generate mesh for a chunk */
    void GenerateChunkMesh(FPlanetChunk* Chunk, UProceduralMeshComponent* MeshComponent);

    /** Process pending chunks gradually */
    void ProcessPendingChunks();

    /** Check if we should update LOD based on camera movement */
    bool ShouldUpdateLOD(const FVector& CameraPosition);

private:
    /** Whether LOD updates are enabled */
    bool bLODUpdatesEnabled = true;

    /** Performance tracking */
    float LastFrameTime = 0.0f;
    int32 ChunksGeneratedThisFrame = 0;
};