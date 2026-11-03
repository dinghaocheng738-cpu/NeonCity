using UnrealBuildTool;

public class NeonCityOcean : ModuleRules
{
    public NeonCityOcean(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]{"Core","RHI","RenderCore"});
        if(Target.bBuildEditor) PrivateDependencyModuleNames.Add("Json");
        RuntimeDependencies.Add("$(ProjectDir)/Docs/ThirdParty/Tidewater-LICENSE.txt",StagedFileType.NonUFS);
    }
}
