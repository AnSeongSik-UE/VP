using UnrealBuildTool;

public class VPTrackerReceiver : ModuleRules
{
	public VPTrackerReceiver(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"Sockets",
			"Networking"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AnimGraphRuntime"
		});
	}
}
