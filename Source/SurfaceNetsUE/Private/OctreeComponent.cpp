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

    // Create root node
    FOctreeKey RootKey(MaxDepth, FIntVector::ZeroValue);
    int32 RootIndex = CreateNode(RootKey, WorldCenter, RootSize);
    RootIndices.Add(RootIndex);

    UE_LOG(LogTemp, Log, TEXT("Initialized octree with root size: %f"), RootSize);
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
    
    FOctreeNode NewNode(Key, Center, Size);
    NewNode.ParentIndex = ParentIndex;
    
    Nodes.Add(NewNode);
    KeyToNodeIndex.Add(Key, NodeIndex);
    
    return NodeIndex;
}

void UOctreeComponent::UpdateLOD(const FVector& CameraPosition)
{
    if (RootIndices.Num() == 0)
    {
        return;
    }

    // Use a two-pass approach to avoid array reallocation issues
    // First pass: collect nodes that need subdivision/merging
    TArray<int32> NodesToSubdivide;
    TArray<int32> NodesToMerge;
    
    // Process nodes breadth-first to determine what operations are needed
    TQueue<int32> NodesToProcess;
    
    // Start with root nodes
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
        
        // Update distance
        Node.DistanceFromCamera = FVector::Dist(Node.Center, CameraPosition);

        // Check operations needed
        bool bShouldSubdivide = ShouldSubdivide(Node, CameraPosition);
        bool bShouldMerge = ShouldMerge(Node, CameraPosition);

        if (bShouldSubdivide && !Node.bHasChildren && Node.Key.Level > 0)
        {
            NodesToSubdivide.Add(NodeIndex);
        }
        else if (bShouldMerge && Node.bHasChildren)
        {
            NodesToMerge.Add(NodeIndex);
        }

        // Add children to processing queue
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

    // Second pass: perform operations (subdivisions first, then merges)
    for (int32 NodeIndex : NodesToSubdivide)
    {
        SubdivideNode(NodeIndex);
    }

    for (int32 NodeIndex : NodesToMerge)
    {
        MergeNode(NodeIndex);
    }

    // Third pass: update node activity (safe to do after all structural changes)
    TQueue<int32> ActivityUpdateQueue;
    for (int32 RootIndex : RootIndices)
    {
        ActivityUpdateQueue.Enqueue(RootIndex);
    }

    while (!ActivityUpdateQueue.IsEmpty())
    {
        int32 NodeIndex;
        ActivityUpdateQueue.Dequeue(NodeIndex);

        if (!Nodes.IsValidIndex(NodeIndex))
        {
            continue;
        }

        UpdateNodeActivity(NodeIndex, CameraPosition);

        const FOctreeNode& Node = Nodes[NodeIndex];
        if (Node.bHasChildren)
        {
            for (int32 ChildIndex : Node.ChildIndices)
            {
                if (ChildIndex != -1)
                {
                    ActivityUpdateQueue.Enqueue(ChildIndex);
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

    // Make a copy of the node data to avoid reference invalidation
    FOctreeNode NodeCopy = Nodes[NodeIndex];
    
    if (NodeCopy.bHasChildren || NodeCopy.Key.Level <= 0)
    {
        return;
    }

    // Create 8 child nodes
    TArray<FOctreeKey> ChildKeys = NodeCopy.Key.GetChildren();
    float ChildSize = NodeCopy.Size * 0.5f;

    TArray<int32> NewChildIndices;
    NewChildIndices.Reserve(8);

    for (int32 i = 0; i < 8; i++)
    {
        FOctreeKey ChildKey = ChildKeys[i];
        
        // Calculate child center
        FVector ChildOffset(
            (i & 1) ? ChildSize * 0.5f : -ChildSize * 0.5f,
            (i & 2) ? ChildSize * 0.5f : -ChildSize * 0.5f,
            (i & 4) ? ChildSize * 0.5f : -ChildSize * 0.5f
        );
        FVector ChildCenter = NodeCopy.Center + ChildOffset;

        int32 ChildIndex = CreateNode(ChildKey, ChildCenter, ChildSize, NodeIndex);
        NewChildIndices.Add(ChildIndex);
    }

    // Now safely update the original node (reacquire reference after potential reallocation)
    if (Nodes.IsValidIndex(NodeIndex))
    {
        FOctreeNode& Node = Nodes[NodeIndex];
        for (int32 i = 0; i < 8; i++)
        {
            Node.ChildIndices[i] = NewChildIndices[i];
        }
        Node.bHasChildren = true;
        Node.bIsActive = false; // Parent becomes inactive when subdivided
    }
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

    // Remove child nodes (but don't actually remove from array to avoid index shifting)
    for (int32 ChildIndex : Node.ChildIndices)
    {
        if (ChildIndex != -1 && Nodes.IsValidIndex(ChildIndex))
        {
            FOctreeNode& ChildNode = Nodes[ChildIndex];
            
            // Recursively merge children if they have children
            if (ChildNode.bHasChildren)
            {
                MergeNode(ChildIndex);
            }
            
            // Remove from key mapping
            KeyToNodeIndex.Remove(ChildNode.Key);
            
            // Mark the child node as invalid (clear it)
            Nodes[ChildIndex] = FOctreeNode();
        }
    }

    // Reset parent node
    Node.ChildIndices.Init(-1, 8);
    Node.bHasChildren = false;
    Node.bIsActive = true; // Parent becomes active when merged
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

    Node.Chunk = NewChunk; // Store the chunk in the node
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

// Updated function - returns shared pointers instead of raw pointers (C++ only)
TArray<TSharedPtr<FPlanetChunk>> UOctreeComponent::GetActiveChunks() const
{
    TArray<TSharedPtr<FPlanetChunk>> ActiveChunks;
    
    for (const FOctreeNode& Node : Nodes)
    {
        if (Node.bIsActive && Node.Chunk.IsValid())
        {
            ActiveChunks.Add(Node.Chunk);
        }
    }
    
    return ActiveChunks;
}

// New Blueprint-accessible functions
int32 UOctreeComponent::GetActiveChunkCount() const
{
    int32 Count = 0;
    for (const FOctreeNode& Node : Nodes)
    {
        if (Node.bIsActive && Node.Chunk.IsValid())
        {
            Count++;
        }
    }
    return Count;
}

TArray<FVector> UOctreeComponent::GetActiveChunkPositions() const
{
    TArray<FVector> Positions;
    
    for (const FOctreeNode& Node : Nodes)
    {
        if (Node.bIsActive && Node.Chunk.IsValid())
        {
            Positions.Add(Node.Chunk->Position);
        }
    }
    
    return Positions;
}