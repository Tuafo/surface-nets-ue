#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "PlanetActor.generated.h"

class UOctreeComponent;
class UNoiseGenerator;
struct FPlanetChunk;

/**
 * Main planet actor that manages procedural planet generation
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
    
    /** Octree component for LOD management */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UOctreeComponent* OctreeComponent;
    
    /** Noise generator for terrain */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNoiseGenerator* NoiseGenerator;
    
    /** Procedural mesh components pool */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<UProceduralMeshComponent*> MeshComponents;

    /** Planet radius */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    float PlanetRadius = 1000.0f;
    
    /** Maximum number of mesh components to pool */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    int32 MaxMeshComponents = 100;
    
    /** Material to apply to planet chunks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
    UMaterialInterface* PlanetMaterial;
    
    /** Enable collision for planet chunks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    bool bEnableCollision = true;
    
    /** Update frequency for mesh updates */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float MeshUpdateFrequency = 0.2f;

    /** Initialize the planet */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    void InitializePlanet();
    
    /** Update planet meshes */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    void UpdatePlanetMeshes();
    
    /** Get or create a mesh component from the pool */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    UProceduralMeshComponent* GetMeshComponent();
    
    /** Return a mesh component to the pool */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    void ReturnMeshComponent(UProceduralMeshComponent* MeshComponent);

private:
    /** Timer for mesh updates */
    float MeshUpdateTimer;
    
    /** Pool of available mesh components */
    TArray<UProceduralMeshComponent*> AvailableMeshComponents;
    
    /** Currently used mesh components */
    TArray<UProceduralMeshComponent*> UsedMeshComponents;
    
    /** Chunks that need mesh updates */
    TArray<FPlanetChunk*> ChunksNeedingUpdate;
    
    /** Create a new mesh component */
    UProceduralMeshComponent* CreateMeshComponent();
    
    /** Update mesh for a specific chunk */
    void UpdateChunkMesh(FPlanetChunk* Chunk, UProceduralMeshComponent* MeshComponent);
    
    /** Clear unused mesh components */
    void ClearUnusedMeshComponents();
};