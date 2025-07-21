using UnrealBuildTool;
using System.Collections.Generic;

public class SurfaceNetsUETarget : TargetRules
{
	public SurfaceNetsUETarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.AddRange( new string[] { "SurfaceNetsUE" } );
	}
}