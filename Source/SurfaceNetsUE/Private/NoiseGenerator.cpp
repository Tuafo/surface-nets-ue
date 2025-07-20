#include "NoiseGenerator.h"
#include "Engine/Engine.h"

UNoiseGenerator::UNoiseGenerator()
{
    PlanetRadius = 1000.0f;
    NoiseScale = 0.01f;
    NoiseAmplitude = 100.0f;
    Octaves = 4;
    Lacunarity = 2.0f;
    Persistence = 0.5f;
    Seed = 1337;
}

float UNoiseGenerator::SampleDensity(const FVector& WorldPosition) const
{
    // Calculate distance from planet center
    float DistanceFromCenter = WorldPosition.Size();
    
    // Base sphere
    float SphereDensity = DistanceFromCenter - PlanetRadius;
    
    // Add terrain noise
    float TerrainNoise = FractalNoise(WorldPosition);
    float TerrainHeight = TerrainNoise * NoiseAmplitude;
    
    // Combine sphere with terrain
    float Density = SphereDensity + TerrainHeight;
    
    return -Density; // Negative inside, positive outside
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
    // This is a simplified version - in production you'd want to use a proper noise library
    
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
    
    float Ixy0 = Lerp(Ix00, Ix10, Sy);
    float Ixy1 = Lerp(Ix01, Ix11, Sy);
    
    return Lerp(Ixy0, Ixy1, Sz);
}

float UNoiseGenerator::Hash(float x, float y, float z) const
{
    // Simple hash function for noise
    int32 n = (int32)(x * 374761393.0f + y * 668265263.0f + z * 1013904223.0f);
    n = (n << 13) ^ n;
    n = (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff;
    return (float)n / 1073741824.0f - 1.0f;
}

float UNoiseGenerator::SmoothStep(float t) const
{
    return t * t * (3.0f - 2.0f * t);
}

float UNoiseGenerator::Lerp(float a, float b, float t) const
{
    return a + t * (b - a);
}