using UnrealBuildTool;

public class CurioIslesEditorTarget : TargetRules
{
	public CurioIslesEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CurioIsles");
	}
}
