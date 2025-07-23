#include "NoiseGenerator.h"
#include "SurfaceNetsUE.h"
#include "Engine/Engine.h"

UNoiseGenerator::UNoiseGenerator()
{
    PlanetRadius = 1000.0f;
    PlanetCenter = FVector::ZeroVector;
    // Better noise parameters for surface intersection
    NoiseScale = 0.001f;    // Smaller scale = larger features
    NoiseAmplitude = 50.0f; // Smaller amplitude relative to radius
    Octaves = 3;            // Fewer octaves for simpler terrain
    Lacunarity = 2.0f;
    Persistence = 0.5f;
    Seed = 1337;
}

float UNoiseGenerator::SampleDensity(const FVector& WorldPosition) const
{
    // Calculate distance from planet center
    float DistanceFromCenter = (WorldPosition - PlanetCenter).Size();
    
    // Base sphere (negative inside, positive outside for Surface Nets)
    float SphereDensity = DistanceFromCenter - PlanetRadius;
    
    // Add terrain noise
    float TerrainNoise = FractalNoise(WorldPosition);
    float TerrainHeight = TerrainNoise * NoiseAmplitude;
    
    // Combine sphere with terrain - negative values are "inside" the surface
    float FinalDensity = SphereDensity - TerrainHeight;
    
    // Debug sampling for some positions
    static int32 SampleCount = 0;
    if (SampleCount < 10)
    {
        UE_LOG(LogSurfaceNets, VeryVerbose, TEXT("Sample at %s: Distance=%f, Sphere=%f, Noise=%f, Final=%f"), 
               *WorldPosition.ToString(), DistanceFromCenter, SphereDensity, TerrainHeight, FinalDensity);
        SampleCount++;
    }
    
    return FinalDensity;
}

float UNoiseGenerator::SampleHeight(const FVector& SurfacePosition) const
{
    return FractalNoise(SurfacePosition) * NoiseAmplitude;
}

float UNoiseGenerator::FractalNoise(const FVector& Position) const
{
    float Value = 0.0f;
    float Amplitude = 1.0f;
    float Frequency = NoiseScale;
    
    for (int32 i = 0; i < Octaves; i++)
    {
        Value += SimplexNoise(Position * Frequency) * Amplitude;
        Frequency *= Lacunarity;
        Amplitude *= Persistence;
    }
    
    return Value;
}

float UNoiseGenerator::SimplexNoise(const FVector& Position) const
{
    // Simple 3D noise implementation
    FVector P = Position;
    P.X += Seed * 0.1f;
    P.Y += Seed * 0.2f;
    P.Z += Seed * 0.3f;
    
    // Grid coordinates
    int32 X0 = FMath::FloorToInt(P.X);
    int32 Y0 = FMath::FloorToInt(P.Y);
    int32 Z0 = FMath::FloorToInt(P.Z);
    int32 X1 = X0 + 1;
    int32 Y1 = Y0 + 1;
    int32 Z1 = Z0 + 1;
    
    // Interpolation weights
    float Sx = SmoothStep(P.X - X0);
    float Sy = SmoothStep(P.Y - Y0);
    float Sz = SmoothStep(P.Z - Z0);
    
    // Hash values at cube corners
    float N000 = Hash(X0, Y0, Z0);
    float N001 = Hash(X0, Y0, Z1);
    float N010 = Hash(X0, Y1, Z0);
    float N011 = Hash(X0, Y1, Z1);
    float N100 = Hash(X1, Y0, Z0);
    float N101 = Hash(X1, Y0, Z1);
    float N110 = Hash(X1, Y1, Z0);
    float N111 = Hash(X1, Y1, Z1);
    
    // Trilinear interpolation
    float Ix00 = Lerp(N000, N100, Sx);
    float Ix01 = Lerp(N001, N101, Sx);
    float Ix10 = Lerp(N010, N110, Sx);
    float Ix11 = Lerp(N011, N111, Sx);
    
    float Iy0 = Lerp(Ix00, Ix10, Sy);
    float Iy1 = Lerp(Ix01, Ix11, Sy);
    
    return Lerp(Iy0, Iy1, Sz);
}

float UNoiseGenerator::Hash(float x, float y, float z) const
{
    // Simple hash function for noise generation
    int32 ix = static_cast<int32>(x);
    int32 iy = static_cast<int32>(y);
    int32 iz = static_cast<int32>(z);
    
    uint32 hash = ix * 374761393U + iy * 668265263U + iz * 2147483647U;
    hash ^= hash >> 16;
    hash *= 0x7feb352dU;
    hash ^= hash >> 15;
    hash *= 0x846ca68bU;
    hash ^= hash >> 16;
    
    return (hash / 4294967295.0f) * 2.0f - 1.0f;
}

float UNoiseGenerator::SmoothStep(float t) const
{
    return t * t * (3.0f - 2.0f * t);
}

float UNoiseGenerator::Lerp(float a, float b, float t) const
{
    return a + t * (b - a);
}