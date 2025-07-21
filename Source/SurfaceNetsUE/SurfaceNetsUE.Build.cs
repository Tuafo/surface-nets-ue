using UnrealBuildTool;

public class SurfaceNetsUE : ModuleRules
{
	public SurfaceNetsUE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"ProceduralMeshComponent",
			"RenderCore",
			"RHI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { 
			"Slate",
			"SlateCore"
		});
		
		// For multi-threading support
		PublicDependencyModuleNames.Add("Core");
		
		// Enable optimization for Release builds
		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			OptimizeCode = CodeOptimization.Speed;
		}
	}
}