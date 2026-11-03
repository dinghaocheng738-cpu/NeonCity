using UnrealBuildTool;
public class NeonCityArtEditor : ModuleRules
{
    public NeonCityArtEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","NeonCity"});
        PrivateDependencyModuleNames.AddRange(new[]{"InputCore","Chaos","PhysicsCore"});
        PrivateDependencyModuleNames.AddRange(new[]{"UnrealEd","AnimGraph","AnimGraphRuntime","BlueprintGraph","KismetCompiler","Kismet","AssetRegistry","Landscape","Foliage","RHI","RenderCore","MaterialEditor","MeshDescription","StaticMeshDescription","NeonCityOcean","Niagara"});
        PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("Niagara"), "Internal"));
        PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("NiagaraShader"), "Internal"));
        PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("Engine"), "Internal"));
        PrivateDependencyModuleNames.Add("NiagaraShader");
    }
}
