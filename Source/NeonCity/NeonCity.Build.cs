// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NeonCity : ModuleRules
{
	public NeonCity(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // MIT notice accompanies binaries containing the adapted marine/fishing algorithms.
        RuntimeDependencies.Add("$(ProjectDir)/Docs/ThirdParty/Tidewater-LICENSE.txt", StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(ProjectDir)/Docs/ThirdParty/TidewaterAssetAudit/Rocketbox/LICENSE.md", StagedFileType.NonUFS);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
            "ChaosVehicles",
            "PhysicsCore",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore", "NeonCityOcean", "Landscape", "RHI", "RenderCore", "Niagara", "Chaos", "AnimationCore" });

        // Editor-only PIE regression checks; no UnrealEd dependency in game builds.
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AssetRegistry", "AnimGraphRuntime", "Json", "ImageCore", "RHI", "AudioMixer", "MeshDescription", "StaticMeshDescription" });
        }

		PublicIncludePaths.AddRange(new string[] {
			"NeonCity",
			"NeonCity/Variant_Platforming",
			"NeonCity/Variant_Platforming/Animation",
			"NeonCity/Variant_Combat",
			"NeonCity/Variant_Combat/AI",
			"NeonCity/Variant_Combat/Animation",
			"NeonCity/Variant_Combat/Gameplay",
			"NeonCity/Variant_Combat/Interfaces",
			"NeonCity/Variant_Combat/UI",
			"NeonCity/Variant_SideScrolling",
			"NeonCity/Variant_SideScrolling/AI",
			"NeonCity/Variant_SideScrolling/Gameplay",
			"NeonCity/Variant_SideScrolling/Interfaces",
			"NeonCity/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
