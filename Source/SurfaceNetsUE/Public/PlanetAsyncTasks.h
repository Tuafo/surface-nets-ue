#pragma once

#include "CoreMinimal.h"
#include "Async/AsyncWork.h"
#include "PlanetChunk.h"

class UNoiseGenerator;

/**
 * Async task for generating planet chunk meshes
 */
class SURFACENETSUE_API FPlanetChunkGenerationTask : public FNonAbandonableTask
{
public:
    FPlanetChunkGenerationTask(
        TSharedPtr<FPlanetChunk> InChunk,
        const UNoiseGenerator* InNoiseGenerator
    );
    
    /** Required by FNonAbandonableTask */
    void DoWork();
    
    /** Required by FNonAbandonableTask */
    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FPlanetChunkGenerationTask, STATGROUP_ThreadPoolAsyncTasks);
    }
    
    /** Check if task is complete */
    bool IsComplete() const { return bIsComplete; }
    
    /** Get the generated chunk */
    TSharedPtr<FPlanetChunk> GetChunk() const { return Chunk; }

private:
    /** Chunk to generate */
    TSharedPtr<FPlanetChunk> Chunk;
    
    /** Noise generator for terrain */
    const UNoiseGenerator* NoiseGenerator;
    
    /** Completion flag */
    std::atomic<bool> bIsComplete;
};

/**
 * Async task manager for planet chunk generation
 */
class SURFACENETSUE_API FPlanetAsyncTaskManager
{
public:
    /** Start generating a chunk asynchronously */
    static void GenerateChunkAsync(
        TSharedPtr<FPlanetChunk> Chunk,
        const UNoiseGenerator* NoiseGenerator,
        TFunction<void(TSharedPtr<FPlanetChunk>)> OnComplete = nullptr
    );
    
    /** Update and process completed tasks */
    static void ProcessCompletedTasks();
    
    /** Clear all pending tasks */
    static void ClearAllTasks();
    
    /** Get number of active tasks */
    static int32 GetActiveTaskCount();

private:
    /** Active async tasks */
    static TArray<TUniquePtr<FAsyncTask<FPlanetChunkGenerationTask>>> ActiveTasks;
    
    /** Completion callbacks */
    static TArray<TFunction<void(TSharedPtr<FPlanetChunk>)>> CompletionCallbacks;
    
    /** Critical section for thread safety */
    static FCriticalSection TaskManagerCS;
};