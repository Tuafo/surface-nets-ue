#include "OctreeComponent.h"
#include "PlanetChunk.h"
#include "SurfaceNetsUE.h"
#include "Engine/World.h"

UOctreeComponent::UOctreeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UOctreeComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UOctreeComponent::InitializeOctree(const FVector& InWorldCenter)
{
    WorldCenter = InWorldCenter;
    ClearOctree();

    // Create root nodes - we'll create a single root for now
    FOctreeKey RootKey(MaxDepth, FIntVector::ZeroValue);
    int32 RootIndex = CreateNode(RootKey, WorldCenter, RootSize);
    RootIndices.Add(RootIndex);
}

void UOctreeComponent::ClearOctree()
{
    Nodes.Empty();
    KeyToNodeIndex.Empty();
    RootIndices.Empty();
}

int32 UOctreeComponent::CreateNode(const FOctreeKey& Key, const FVector& Center, float Size, int32 ParentIndex)
{
    int32 NodeIndex = Nodes.Num();
    
    FOctreeNode& NewNode = Nodes.AddDefaulted_GetRef();
    NewNode.Key = Key;
    NewNode.Center = Center;
    NewNode.Size = Size;
    NewNode.ParentIndex = ParentIndex;
    
    KeyToNodeIndex.Add(Key, NodeIndex);
    
    return NodeIndex;
}

void UOctreeComponent::UpdateLOD(const FVector& CameraPosition)
{
    if (RootIndices.IsEmpty())
    {
        return;
    }

    // Process all nodes starting from roots
    TQueue<int32> NodesToProcess;
    for (int32 RootIndex : RootIndices)
    {
        NodesToProcess.Enqueue(RootIndex);
    }

    while (!NodesToProcess.IsEmpty())
    {
        int32 NodeIndex;
        NodesToProcess.Dequeue(NodeIndex);
        
        if (!Nodes.IsValidIndex(NodeIndex))
        {
            continue;
        }

        FOctreeNode& Node = Nodes[NodeIndex];
        Node.DistanceFromCamera = FVector::Dist(CameraPosition, Node.Center);

        bool bShouldSubdivide = ShouldSubdivide(Node, CameraPosition);
        bool bShouldMerge = ShouldMerge(Node, CameraPosition);

        if (bShouldSubdivide && !Node.bHasChildren && Node.Key.Level > 0)
        {
            SubdivideNode(NodeIndex);
        }
        else if (bShouldMerge && Node.bHasChildren)
        {
            MergeNode(NodeIndex);
        }

        // Update activity and process children
        UpdateNodeActivity(NodeIndex, CameraPosition);

        if (Node.bHasChildren)
        {
            for (int32 ChildIndex : Node.ChildIndices)
            {
                if (ChildIndex != -1)
                {
                    NodesToProcess.Enqueue(ChildIndex);
                }
            }
        }
    }
}

void UOctreeComponent::SubdivideNode(int32 NodeIndex)
{
    if (!Nodes.IsValidIndex(NodeIndex))
    {
        return;
    }

    FOctreeNode& Node = Nodes[NodeIndex];
    if (Node.bHasChildren || Node.Key.Level <= 0)
    {
        return;
    }

    // Create 8 child nodes
    TArray<FOctreeKey> ChildKeys = Node.Key.GetChildren();
    float ChildSize = Node.Size * 0.5f;

    for (int32 i = 0; i < 8; i++)
    {
        FOctreeKey ChildKey = ChildKeys[i];
        
        // Calculate child center
        FVector ChildOffset(
            (i & 1) ? ChildSize * 0.5f : -ChildSize * 0.5f,
            (i & 2) ? ChildSize * 0.5f : -ChildSize * 0.5f,
            (i & 4) ? ChildSize * 0.5f : -ChildSize * 0.5f
        );
        FVector ChildCenter = Node.Center + ChildOffset;

        int32 ChildIndex = CreateNode(ChildKey, ChildCenter, ChildSize, NodeIndex);
        Node.ChildIndices[i] = ChildIndex;
    }

    Node.bHasChildren = true;
    Node.bIsActive = false; // Parent becomes inactive when subdivided
}

void UOctreeComponent::MergeNode(int32 NodeIndex)
{
    if (!Nodes.IsValidIndex(NodeIndex))
    {
        return;
    }

    FOctreeNode& Node = Nodes[NodeIndex];
    if (!Node.bHasChildren)
    {
        return;
    }

    // Remove all children and their descendants
    TQueue<int32> NodesToRemove;
    for (int32 ChildIndex : Node.ChildIndices)
    {
        if (ChildIndex != -1)
        {
            NodesToRemove.Enqueue(ChildIndex);
        }
    }

    while (!NodesToRemove.IsEmpty())
    {
        int32 IndexToRemove;
        NodesToRemove.Dequeue(IndexToRemove);
        
        if (!Nodes.IsValidIndex(IndexToRemove))
        {
            continue;
        }

        FOctreeNode& NodeToRemove = Nodes[IndexToRemove];
        
        // Add children to removal queue
        if (NodeToRemove.bHasChildren)
        {
            for (int32 GrandChildIndex : NodeToRemove.ChildIndices)
            {
                if (GrandChildIndex != -1)
                {
                    NodesToRemove.Enqueue(GrandChildIndex);
                }
            }
        }

        // Remove from key map
        KeyToNodeIndex.Remove(NodeToRemove.Key);
        
        // Clear the node (we keep the array to avoid index shifting)
        NodeToRemove = FOctreeNode();
    }

    // Reset parent node
    Node.bHasChildren = false;
    Node.bIsActive = true;
    Node.ChildIndices.Init(-1, 8);
}

bool UOctreeComponent::ShouldSubdivide(const FOctreeNode& Node, const FVector& CameraPosition) const
{
    if (Node.Key.Level <= 0)
    {
        return false;
    }

    float RequiredSize = CalculateRequiredChunkSize(Node.DistanceFromCamera);
    return Node.Size > RequiredSize;
}

bool UOctreeComponent::ShouldMerge(const FOctreeNode& Node, const FVector& CameraPosition) const
{
    if (!Node.bHasChildren)
    {
        return false;
    }

    float RequiredSize = CalculateRequiredChunkSize(Node.DistanceFromCamera);
    float MergeThreshold = RequiredSize * MergeDistanceMultiplier;
    return Node.Size < MergeThreshold;
}

void UOctreeComponent::UpdateNodeActivity(int32 NodeIndex, const FVector& CameraPosition)
{
    if (!Nodes.IsValidIndex(NodeIndex))
    {
        return;
    }

    FOctreeNode& Node = Nodes[NodeIndex];
    
    // Node is active if it has no children (leaf node)
    Node.bIsActive = !Node.bHasChildren;
    
    if (Node.bIsActive)
    {
        // Ensure chunk exists for active nodes
        if (!Node.Chunk.IsValid())
        {
            Node.Chunk = GetOrCreateChunk(Node);
        }
    }
    else
    {
        // Clear chunk for inactive nodes
        Node.Chunk.Reset();
    }
}

TSharedPtr<FPlanetChunk> UOctreeComponent::GetOrCreateChunk(FOctreeNode& Node)
{
    if (Node.Chunk.IsValid())
    {
        return Node.Chunk;
    }

    // Create new chunk
    TSharedPtr<FPlanetChunk> NewChunk = MakeShared<FPlanetChunk>();
    NewChunk->Position = Node.Center;
    NewChunk->Size = Node.Size;
    NewChunk->LODLevel = MaxDepth - Node.Key.Level; // Convert to 0-based LOD
    NewChunk->VoxelResolution = NewChunk->GetVoxelResolution();
    NewChunk->DistanceFromCamera = Node.DistanceFromCamera;

    return NewChunk;
}

float UOctreeComponent::CalculateRequiredChunkSize(float Distance) const
{
    // Calculate chunk size based on distance
    // Closer = smaller chunks (higher detail)
    // Further = larger chunks (lower detail)
    float MinChunkSize = RootSize / FMath::Pow(2.0f, static_cast<float>(MaxDepth));
    float MaxChunkSize = RootSize;
    
    // Use exponential scaling based on subdivision distance
    float NormalizedDistance = Distance / SubdivisionDistance;
    float ChunkSize = MinChunkSize * FMath::Pow(2.0f, NormalizedDistance);
    
    return FMath::Clamp(ChunkSize, MinChunkSize, MaxChunkSize);
}

FVector UOctreeComponent::GetWorldPosition(const FOctreeKey& Key, float NodeSize) const
{
    // Convert octree coordinates to world position
    float LevelSize = RootSize / FMath::Pow(2.0f, static_cast<float>(Key.Level));
    FVector Offset = FVector(Key.Coordinates) * LevelSize;
    return WorldCenter + Offset;
}

TArray<FPlanetChunk*> UOctreeComponent::GetActiveChunks() const
{
    TArray<FPlanetChunk*> ActiveChunks;
    
    for (const FOctreeNode& Node : Nodes)
    {
        if (Node.bIsActive && Node.Chunk.IsValid())
        {
            ActiveChunks.Add(Node.Chunk.Get());
        }
    }
    
    return ActiveChunks;
}