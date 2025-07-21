#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SurfaceNetsGameMode.generated.h"

/**
 * Game mode for Surface Nets planet generation demo
 */
UCLASS(BlueprintType, Blueprintable)
class SURFACENETSUE_API ASurfaceNetsGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASurfaceNetsGameMode();

protected:
    virtual void BeginPlay() override;

public:
    /** Automatically spawn planet actor on begin play */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    bool bAutoSpawnPlanet = true;
    
    /** Planet actor class to spawn */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    TSubclassOf<class APlanetActor> PlanetActorClass;
    
    /** Spawn location for planet */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    FVector PlanetSpawnLocation = FVector::ZeroVector;

    /** Spawn a planet actor */
    UFUNCTION(BlueprintCallable, Category = "Planet")
    class APlanetActor* SpawnPlanet();

private:
    /** Reference to spawned planet */
    UPROPERTY()
    class APlanetActor* SpawnedPlanet;
};