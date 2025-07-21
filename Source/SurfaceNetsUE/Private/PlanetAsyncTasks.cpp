#include "PlanetAsyncTasks.h"
#include "NoiseGenerator.h"
#include "SurfaceNetsUE.h"

// Static member definitions
TArray<TUniquePtr<FAsyncTask<FPlanetChunkGenerationTask>>> FPlanetAsyncTaskManager::ActiveTasks;
TArray<TFunction<void(TSharedPtr<FPlanetChunk>)>> FPlanetAsyncTaskManager::CompletionCallbacks;
FCriticalSection FPlanetAsyncTaskManager::TaskManagerCS;

FPlanetChunkGenerationTask::FPlanetChunkGenerationTask(
    TSharedPtr<FPlanetChunk> InChunk,
    const UNoiseGenerator* InNoiseGenerator)
    : Chunk(InChunk)
    , NoiseGenerator(InNoiseGenerator)
    , bIsComplete(false)
{
}

void FPlanetChunkGenerationTask::DoWork()
{
    if (!Chunk.IsValid() || !NoiseGenerator)
    {
        UE_LOG(LogSurfaceNets, Warning, TEXT("Invalid chunk or noise generator in async task"));
        bIsComplete = true;
        return;
    }
    
    UE_LOG(LogSurfaceNets, Verbose, TEXT("Starting async chunk generation at %s"), 
        *Chunk->Position.ToString());
    
    // Generate the mesh data
    Chunk->GenerateMesh(NoiseGenerator);
    
    UE_LOG(LogSurfaceNets, Verbose, TEXT("Completed async chunk generation at %s with %d vertices"), 
        *Chunk->Position.ToString(), Chunk->Vertices.Num());
    
    bIsComplete = true;
}

void FPlanetAsyncTaskManager::GenerateChunkAsync(
    TSharedPtr<FPlanetChunk> Chunk,
    const UNoiseGenerator* NoiseGenerator,
    TFunction<void(TSharedPtr<FPlanetChunk>)> OnComplete)
{
    if (!Chunk.IsValid() || !NoiseGenerator)
    {
        UE_LOG(LogSurfaceNets, Warning, TEXT("Cannot start async task with invalid chunk or noise generator"));
        return;
    }
    
    FScopeLock Lock(&TaskManagerCS);
    
    // Mark chunk as generating
    Chunk->bIsGenerating = true;
    
    // Create async task
    auto AsyncTask = MakeUnique<FAsyncTask<FPlanetChunkGenerationTask>>(Chunk, NoiseGenerator);
    
    // Start the task
    AsyncTask->StartBackgroundTask();
    
    // Store task and callback
    ActiveTasks.Add(MoveTemp(AsyncTask));
    CompletionCallbacks.Add(OnComplete);
    
    UE_LOG(LogSurfaceNets, Verbose, TEXT("Started async generation for chunk at %s"), 
        *Chunk->Position.ToString());
}

void FPlanetAsyncTaskManager::ProcessCompletedTasks()
{
    FScopeLock Lock(&TaskManagerCS);
    
    // Process completed tasks from back to front for safe removal
    for (int32 i = ActiveTasks.Num() - 1; i >= 0; i--)
    {
        auto& Task = ActiveTasks[i];
        if (Task->IsDone())
        {
            // Get the completed chunk
            TSharedPtr<FPlanetChunk> CompletedChunk = Task->GetTask().GetChunk();
            
            if (CompletedChunk.IsValid())
            {
                CompletedChunk->bIsGenerating = false;
                
                // Call completion callback if provided
                if (i < CompletionCallbacks.Num() && CompletionCallbacks[i])
                {
                    CompletionCallbacks[i](CompletedChunk);
                }
            }
            
            // Remove completed task
            ActiveTasks.RemoveAt(i);
            CompletionCallbacks.RemoveAt(i);
        }
    }
}

void FPlanetAsyncTaskManager::ClearAllTasks()
{
    FScopeLock Lock(&TaskManagerCS);
    
    // Wait for all tasks to complete or abandon them
    for (auto& Task : ActiveTasks)
    {
        if (!Task->IsDone())
        {
            // For critical shutdown, we might need to wait
            Task->EnsureCompletion();
        }
    }
    
    ActiveTasks.Empty();
    CompletionCallbacks.Empty();
    
    UE_LOG(LogSurfaceNets, Log, TEXT("Cleared all async tasks"));
}

int32 FPlanetAsyncTaskManager::GetActiveTaskCount()
{
    FScopeLock Lock(&TaskManagerCS);
    return ActiveTasks.Num();
}