#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NoiseGenerator.generated.h"

/**
 * Noise generator for procedural planet terrain
 */
UCLASS(BlueprintType, Blueprintable)
class SURFACENETSUE_API UNoiseGenerator : public UObject
{
    GENERATED_BODY()

public:
    UNoiseGenerator();

    /** Planet radius */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    float PlanetRadius = 1000.0f;
    
    /** Noise scale for terrain features */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
    float NoiseScale = 0.01f;
    
    /** Amplitude of noise */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
    float NoiseAmplitude = 100.0f;
    
    /** Number of octaves for fractal noise */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
    int32 Octaves = 4;
    
    /** Lacunarity for octaves */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
    float Lacunarity = 2.0f;
    
    /** Persistence for octaves */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
    float Persistence = 0.5f;
    
    /** Noise seed for reproducible results */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
    int32 Seed = 1337;

    /** Sample density at world position for Surface Nets */
    UFUNCTION(BlueprintCallable, Category = "Noise")
    float SampleDensity(const FVector& WorldPosition) const;
    
    /** Sample height at surface position */
    UFUNCTION(BlueprintCallable, Category = "Noise")
    float SampleHeight(const FVector& SurfacePosition) const;

private:
    /** Generate fractal noise */
    float FractalNoise(const FVector& Position) const;
    
    /** Simple 3D Perlin-like noise */
    float SimplexNoise(const FVector& Position) const;
    
    /** Hash function for noise generation */
    float Hash(float x, float y, float z) const;
    
    /** Smooth interpolation function */
    float SmoothStep(float t) const;
    
    /** Linear interpolation */
    float Lerp(float a, float b, float t) const;
};