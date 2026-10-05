using UnrealBuildTool;

public class CurioIslesTarget : TargetRules
{
	public CurioIslesTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CurioIsles");
	}
}
