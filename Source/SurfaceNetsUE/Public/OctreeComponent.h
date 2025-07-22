#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OctreeComponent.generated.h"

// Forward declarations
struct FPlanetChunk;

USTRUCT(BlueprintType)
struct SURFACENETSUE_API FOctreeKey
{
    GENERATED_BODY()

    /** Level in the octree (0 = highest detail) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree")
    int32 Level = 0;

    /** Integer coordinates at this level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree")
    FIntVector Coordinates = FIntVector::ZeroValue;

    FOctreeKey() = default;
    FOctreeKey(int32 InLevel, const FIntVector& InCoordinates)
        : Level(InLevel), Coordinates(InCoordinates) {}

    /** Get parent key (one level up) */
    FOctreeKey GetParent() const
    {
        return FOctreeKey(Level + 1, Coordinates >> 1);
    }

    /** Get child keys (one level down) */
    TArray<FOctreeKey> GetChildren() const
    {
        TArray<FOctreeKey> Children;
        Children.Reserve(8);
        
        for (int32 i = 0; i < 8; i++)
        {
            FIntVector ChildOffset(
                (i & 1) ? 1 : 0,
                (i & 2) ? 1 : 0,
                (i & 4) ? 1 : 0
            );
            FIntVector ChildCoord = (Coordinates << 1) + ChildOffset;
            Children.Add(FOctreeKey(Level - 1, ChildCoord));
        }
        return Children;
    }

    bool operator==(const FOctreeKey& Other) const
    {
        return Level == Other.Level && Coordinates == Other.Coordinates;
    }

    friend uint32 GetTypeHash(const FOctreeKey& Key)
    {
        return HashCombine(GetTypeHash(Key.Level), GetTypeHash(Key.Coordinates));
    }
};

USTRUCT(BlueprintType)
struct SURFACENETSUE_API FOctreeNode
{
    GENERATED_BODY()

    /** Key identifying this node */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree")
    FOctreeKey Key;

    /** World space center of this node */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree")
    FVector Center = FVector::ZeroVector;

    /** Size of this node */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree")
    float Size = 0.0f;

    /** Whether this node has children */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree")
    bool bHasChildren = false;

    /** Whether this node is active (should generate mesh) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree")
    bool bIsActive = false;

    /** Distance from camera for LOD calculations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree")
    float DistanceFromCamera = 0.0f;

    /** Planet chunk associated with this node */
    TSharedPtr<FPlanetChunk> Chunk;

    /** Child node indices (8 for octree) */
    TArray<int32> ChildIndices;

    /** Parent node index (-1 if root) */
    int32 ParentIndex = -1;

    FOctreeNode() 
    {
        ChildIndices.Init(-1, 8);
    }

    FOctreeNode(const FOctreeKey& InKey, const FVector& InCenter, float InSize)
        : Key(InKey), Center(InCenter), Size(InSize)
    {
        ChildIndices.Init(-1, 8);
    }
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURFACENETSUE_API UOctreeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOctreeComponent();

    /** Maximum depth of the octree */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree LOD")
    int32 MaxDepth = 5;

    /** Size of the root octree node */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree LOD")
    float RootSize = 2048.0f;

    /** Distance at which nodes should subdivide */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree LOD")
    float SubdivisionDistance = 500.0f; 

    /** Multiplier for merge distance (merge at SubdivisionDistance * MergeDistanceMultiplier) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Octree LOD")
    float MergeDistanceMultiplier = 3.0f;

    /** Update LOD based on camera position */
    UFUNCTION(BlueprintCallable, Category = "Octree LOD")
    void UpdateLOD(const FVector& CameraPosition);

    /** Get all active chunks that should render (C++ only - returns shared pointers) */
    TArray<TSharedPtr<FPlanetChunk>> GetActiveChunks() const;

    /** Get number of active chunks (Blueprint accessible) */
    UFUNCTION(BlueprintCallable, Category = "Octree LOD")
    int32 GetActiveChunkCount() const;

    /** Get active chunk positions (Blueprint accessible) */
    UFUNCTION(BlueprintCallable, Category = "Octree LOD")
    TArray<FVector> GetActiveChunkPositions() const;

    /** Initialize the octree */
    UFUNCTION(BlueprintCallable, Category = "Octree LOD")
    void InitializeOctree(const FVector& WorldCenter);

    /** Clear all octree data */
    UFUNCTION(BlueprintCallable, Category = "Octree LOD")
    void ClearOctree();

protected:
    virtual void BeginPlay() override;

    /** All nodes in the octree */
    UPROPERTY()
    TArray<FOctreeNode> Nodes;

    /** Map from octree key to node index */
    TMap<FOctreeKey, int32> KeyToNodeIndex;

    /** Root node indices */
    TArray<int32> RootIndices;

    /** World center of the octree */
    FVector WorldCenter = FVector::ZeroVector;

    /** Create a new node */
    int32 CreateNode(const FOctreeKey& Key, const FVector& Center, float Size, int32 ParentIndex = -1);

    /** Subdivide a node into 8 children */
    void SubdivideNode(int32 NodeIndex);

    /** Merge a node's children back into the parent */
    void MergeNode(int32 NodeIndex);

    /** Check if a node should subdivide based on distance */
    bool ShouldSubdivide(const FOctreeNode& Node, const FVector& CameraPosition) const;

    /** Check if a node should merge based on distance */
    bool ShouldMerge(const FOctreeNode& Node, const FVector& CameraPosition) const;

    /** Update node activity status */
    void UpdateNodeActivity(int32 NodeIndex, const FVector& CameraPosition);

    /** Get world position for octree coordinates */
    FVector GetWorldPosition(const FOctreeKey& Key, float NodeSize) const;

    /** Calculate required chunk size for distance */
    float CalculateRequiredChunkSize(float Distance) const;

    /** Create or get chunk for node */
    TSharedPtr<FPlanetChunk> GetOrCreateChunk(FOctreeNode& Node);
};