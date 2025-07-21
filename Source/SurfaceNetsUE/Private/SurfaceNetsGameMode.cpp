#include "SurfaceNetsGameMode.h"
#include "PlanetActor.h"
#include "SurfaceNetsUE.h"
#include "Engine/World.h"

ASurfaceNetsGameMode::ASurfaceNetsGameMode()
{
    bAutoSpawnPlanet = true;
    PlanetSpawnLocation = FVector::ZeroVector;
    SpawnedPlanet = nullptr;
    
    // Set default planet actor class
    PlanetActorClass = APlanetActor::StaticClass();
}

void ASurfaceNetsGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    if (bAutoSpawnPlanet)
    {
        SpawnPlanet();
    }
}

APlanetActor* ASurfaceNetsGameMode::SpawnPlanet()
{
    if (!PlanetActorClass)
    {
        UE_LOG(LogSurfaceNets, Warning, TEXT("No PlanetActorClass set in game mode"));
        return nullptr;
    }
    
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogSurfaceNets, Error, TEXT("No world available for planet spawning"));
        return nullptr;
    }
    
    // Destroy existing planet if any
    if (SpawnedPlanet && IsValid(SpawnedPlanet))
    {
        SpawnedPlanet->Destroy();
        SpawnedPlanet = nullptr;
    }
    
    // Spawn new planet
    FActorSpawnParameters SpawnParams;
    SpawnParams.bNoFail = true;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    SpawnedPlanet = World->SpawnActor<APlanetActor>(
        PlanetActorClass,
        PlanetSpawnLocation,
        FRotator::ZeroRotator,
        SpawnParams
    );
    
    if (SpawnedPlanet)
    {
        UE_LOG(LogSurfaceNets, Log, TEXT("Planet spawned successfully at %s"), *PlanetSpawnLocation.ToString());
    }
    else
    {
        UE_LOG(LogSurfaceNets, Error, TEXT("Failed to spawn planet actor"));
    }
    
    return SpawnedPlanet;
}