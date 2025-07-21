#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/ConvexVolume.h"
#include "PlanetChunk.h"
#include "OctreeComponent.generated.h"

class UNoiseGenerator;
class UProceduralMeshComponent;

/**
 * Octree node for spatial subdivision and LOD management
 */
struct SURFACENETSUE_API FOctreeNode
{
public:
    FOctreeNode();
    
    // Delete copy constructor and assignment operator due to TUniquePtr member
    FOctreeNode(const FOctreeNode&) = delete;
    FOctreeNode& operator=(const FOctreeNode&) = delete;
    
    // Allow move constructor and assignment
    FOctreeNode(FOctreeNode&&) = default;
    FOctreeNode& operator=(FOctreeNode&&) = default;
    
    /** Center position of this node */
    FVector Center;
    
    /** Size of this node */
    float Size;
    
    /** LOD level (0 = highest detail) */
    int32 LODLevel;
    
    /** Children nodes (null if leaf) */
    TArray<TSharedPtr<FOctreeNode>> Children;
    
    /** Associated planet chunk (for leaf nodes) */
    TUniquePtr<FPlanetChunk> Chunk;
    
    /** Whether this node is a leaf */
    bool bIsLeaf;
    
    /** Whether this node should be rendered */
    bool bShouldRender;
    
    /** Distance from camera */
    float DistanceFromCamera;
    
    /** Subdivide this node into 8 children */
    void Subdivide();
    
    /** Merge children back into this node */
    void Merge();
    
    /** Update LOD based on camera position */
    void UpdateLOD(const FVector& CameraPosition, const TArray<float>& LODDistances);
    
    /** Check if this node is in camera frustum */
    bool IsInFrustum(const FConvexVolume& Frustum) const;
};

/**
 * Component that manages octree-based LOD for planet generation
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURFACENETSUE_API UOctreeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOctreeComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Maximum octree depth */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree")
    int32 MaxDepth = 6;
    
    /** Root size of the octree */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree")
    float RootSize = 4000.0f;
    
    /** LOD distances for different levels */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
    TArray<float> LODDistances = {100.0f, 300.0f, 800.0f, 2000.0f, 5000.0f};
    
    /** Update frequency in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float UpdateFrequency = 0.1f;
    
    /** Enable frustum culling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bEnableFrustumCulling = true;

    /** Initialize the octree */
    UFUNCTION(BlueprintCallable, Category = "Octree")
    void InitializeOctree(const FVector& Center);
    
    /** Update LOD based on camera position */
    UFUNCTION(BlueprintCallable, Category = "Octree")
    void UpdateLOD(const FVector& CameraPosition);
    
    /** Get all visible chunks for rendering (C++ only) */
    void GetVisibleChunks(TArray<FPlanetChunk*>& OutChunks);
    
    /** Get number of visible chunks (Blueprint accessible) */
    UFUNCTION(BlueprintCallable, Category = "Octree")
    int32 GetVisibleChunkCount() const;
    
    /** Get player location for LOD calculations */
    UFUNCTION(BlueprintCallable, Category = "Octree")
    FVector GetPlayerLocation() const;
    
    /** Set noise generator */
    UFUNCTION(BlueprintCallable, Category = "Octree")
    void SetNoiseGenerator(UNoiseGenerator* InNoiseGenerator);

private:
    /** Root node of the octree */
    TSharedPtr<FOctreeNode> RootNode;
    
    /** Noise generator for terrain */
    UPROPERTY()
    UNoiseGenerator* NoiseGenerator;
    
    /** Timer for LOD updates */
    float UpdateTimer;
    
    /** Last player position */
    FVector LastPlayerPosition;
    
    /** Recursively update LOD for node and children */
    void UpdateNodeLOD(TSharedPtr<FOctreeNode> Node, const FVector& CameraPosition);
    
    /** Recursively collect visible chunks */
    void CollectVisibleChunks(TSharedPtr<FOctreeNode> Node, TArray<FPlanetChunk*>& OutChunks);
    
    /** Generate mesh for a chunk */
    void GenerateChunkMesh(FPlanetChunk* Chunk);
    
    /** Get camera frustum for culling */
    bool GetCameraFrustum(FConvexVolume& OutFrustum) const;
};