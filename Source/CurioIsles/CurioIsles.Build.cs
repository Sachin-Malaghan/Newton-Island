using UnrealBuildTool;

public class CurioIsles : ModuleRules
{
	public CurioIsles(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// The simulation must give bit-identical results on every device. Game targets default to
		// /fp:fast on Windows (reordered, fused maths); force IEEE-precise floating point for this module.
		FPSemantics = FPSemanticsMode.Precise;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"SlateCore",
			"Slate",
			"RenderCore",
			"ApplicationCore"
		});

		// Private/Core is engine-agnostic C++ (also built by Tools/SimHarness).
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Private"));
	}
}
