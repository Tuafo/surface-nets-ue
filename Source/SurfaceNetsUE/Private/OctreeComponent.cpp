#include "OctreeComponent.h"
#include "NoiseGenerator.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "SurfaceNetsUE.h"

FOctreeNode::FOctreeNode()
    : Center(FVector::ZeroVector)
    , Size(0.0f)
    , LODLevel(0)
    , bIsLeaf(true)
    , bShouldRender(false)
    , DistanceFromCamera(0.0f)
{
    Children.SetNum(8);
}

void FOctreeNode::Subdivide()
{
    if (!bIsLeaf) return;
    
    bIsLeaf = false;
    float ChildSize = Size * 0.5f;
    float Offset = ChildSize * 0.5f;
    
    // Create 8 children
    for (int32 i = 0; i < 8; i++)
    {
        Children[i] = MakeShared<FOctreeNode>();
        FOctreeNode* Child = Children[i].Get();
        
        Child->Size = ChildSize;
        Child->LODLevel = LODLevel + 1;
        Child->bIsLeaf = true;
        
        // Calculate child center
        FVector ChildOffset(
            (i & 1) ? Offset : -Offset,
            (i & 2) ? Offset : -Offset,
            (i & 4) ? Offset : -Offset
        );
        Child->Center = Center + ChildOffset;
    }
    
    // Clear chunk since this is no longer a leaf
    Chunk.Reset();
}

void FOctreeNode::Merge()
{
    if (bIsLeaf) return;
    
    // Clear all children
    for (auto& Child : Children)
    {
        Child.Reset();
    }
    
    bIsLeaf = true;
    bShouldRender = true;
    
    // Create new chunk for this merged node
    Chunk = MakeUnique<FPlanetChunk>(Center, LODLevel, Size);
}

void FOctreeNode::UpdateLOD(const FVector& CameraPosition, const TArray<float>& LODDistances)
{
    DistanceFromCamera = FVector::Dist(Center, CameraPosition);
    
    // Determine if we should subdivide or merge based on distance
    bool bShouldSubdivide = false;
    if (LODLevel < LODDistances.Num() && DistanceFromCamera < LODDistances[LODLevel])
    {
        bShouldSubdivide = true;
    }
    
    if (bShouldSubdivide && bIsLeaf)
    {
        Subdivide();
    }
    else if (!bShouldSubdivide && !bIsLeaf)
    {
        Merge();
    }
    
    // Update children recursively
    if (!bIsLeaf)
    {
        for (auto& Child : Children)
        {
            if (Child.IsValid())
            {
                Child->UpdateLOD(CameraPosition, LODDistances);
            }
        }
    }
    
    // Update render flag
    bShouldRender = bIsLeaf;
}

bool FOctreeNode::IsInFrustum(const FConvexVolume& Frustum) const
{
    // Simple sphere-frustum test
    FVector MinBounds = Center - FVector(Size * 0.5f);
    FVector MaxBounds = Center + FVector(Size * 0.5f);
    FBoxSphereBounds Bounds(FBox(MinBounds, MaxBounds));
    
    return Frustum.IntersectSphere(Bounds.Origin, Bounds.SphereRadius);
}

UOctreeComponent::UOctreeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    MaxDepth = 6;
    RootSize = 4000.0f;
    UpdateFrequency = 0.1f;
    bEnableFrustumCulling = true;
    UpdateTimer = 0.0f;
    NoiseGenerator = nullptr;
    LastPlayerPosition = FVector::ZeroVector;
    
    // Default LOD distances
    LODDistances = {100.0f, 300.0f, 800.0f, 2000.0f, 5000.0f};
}

void UOctreeComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize octree at world origin
    InitializeOctree(FVector::ZeroVector);
}

void UOctreeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    UpdateTimer += DeltaTime;
    
    if (UpdateTimer >= UpdateFrequency)
    {
        UpdateTimer = 0.0f;
        
        // Get player position using simplified approach
        FVector PlayerPosition = GetPlayerLocation();
        
        // Only update if player moved significantly
        if (FVector::Dist(PlayerPosition, LastPlayerPosition) > 10.0f)
        {
            UpdateLOD(PlayerPosition);
            LastPlayerPosition = PlayerPosition;
        }
    }
}

void UOctreeComponent::InitializeOctree(const FVector& Center)
{
    RootNode = MakeShared<FOctreeNode>();
    RootNode->Center = Center;
    RootNode->Size = RootSize;
    RootNode->LODLevel = 0;
    RootNode->bIsLeaf = true;
    RootNode->bShouldRender = true;
    
    UE_LOG(LogSurfaceNets, Log, TEXT("Octree initialized at %s with size %f"), *Center.ToString(), RootSize);
}

void UOctreeComponent::UpdateLOD(const FVector& CameraPosition)
{
    if (!RootNode.IsValid())
    {
        return;
    }
    
    UpdateNodeLOD(RootNode, CameraPosition);
}

void UOctreeComponent::GetVisibleChunks(TArray<FPlanetChunk*>& OutChunks)
{
    OutChunks.Empty();
    
    if (!RootNode.IsValid())
    {
        return;
    }
    
    CollectVisibleChunks(RootNode, OutChunks);
}

int32 UOctreeComponent::GetVisibleChunkCount() const
{
    TArray<FPlanetChunk*> VisibleChunks;
    if (!RootNode.IsValid())
    {
        return 0;
    }
    
    // We need a mutable version for the const function
    const_cast<UOctreeComponent*>(this)->CollectVisibleChunks(RootNode, VisibleChunks);
    return VisibleChunks.Num();
}

FVector UOctreeComponent::GetPlayerLocation() const
{
    // Simplified player location getter - much more reliable than complex camera calculations
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                return Pawn->GetActorLocation();
            }
        }
    }
    
    // Fallback to world origin
    return FVector::ZeroVector;
}

void UOctreeComponent::SetNoiseGenerator(UNoiseGenerator* InNoiseGenerator)
{
    NoiseGenerator = InNoiseGenerator;
}

void UOctreeComponent::UpdateNodeLOD(TSharedPtr<FOctreeNode> Node, const FVector& CameraPosition)
{
    if (!Node.IsValid())
    {
        return;
    }
    
    Node->UpdateLOD(CameraPosition, LODDistances);
    
    // Generate mesh for leaf nodes
    if (Node->bIsLeaf && Node->bShouldRender)
    {
        if (!Node->Chunk.IsValid())
        {
            Node->Chunk = MakeUnique<FPlanetChunk>(Node->Center, Node->LODLevel, Node->Size);
        }
        
        if (!Node->Chunk->bIsGenerated && !Node->Chunk->bIsGenerating && NoiseGenerator)
        {
            GenerateChunkMesh(Node->Chunk.Get());
        }
    }
}

void UOctreeComponent::CollectVisibleChunks(TSharedPtr<FOctreeNode> Node, TArray<FPlanetChunk*>& OutChunks)
{
    if (!Node.IsValid() || !Node->bShouldRender)
    {
        return;
    }
    
    // Frustum culling
    if (bEnableFrustumCulling)
    {
        FConvexVolume Frustum;
        if (GetCameraFrustum(Frustum) && !Node->IsInFrustum(Frustum))
        {
            return;
        }
    }
    
    if (Node->bIsLeaf && Node->Chunk.IsValid() && Node->Chunk->bIsGenerated)
    {
        OutChunks.Add(Node->Chunk.Get());
    }
    else if (!Node->bIsLeaf)
    {
        for (auto& Child : Node->Children)
        {
            CollectVisibleChunks(Child, OutChunks);
        }
    }
}

void UOctreeComponent::GenerateChunkMesh(FPlanetChunk* Chunk)
{
    if (!Chunk || !NoiseGenerator)
    {
        return;
    }
    
    // Generate mesh data
    Chunk->GenerateMesh(NoiseGenerator);
    
    UE_LOG(LogSurfaceNets, Verbose, TEXT("Generated mesh for chunk at %s with %d vertices"), 
        *Chunk->Position.ToString(), Chunk->Vertices.Num());
}

bool UOctreeComponent::GetCameraFrustum(FConvexVolume& OutFrustum) const
{
    // Simplified approach: disable frustum culling for now and rely on distance-based LOD
    // Using player location is simpler and more efficient for planet generation
    return false;
}