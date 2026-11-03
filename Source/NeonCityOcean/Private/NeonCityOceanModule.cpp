#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

class FNeonCityOceanModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        // Global compute shaders must register before the engine loads shader maps.
        AddShaderSourceDirectoryMapping(TEXT("/NeonCity"),FPaths::Combine(FPaths::ProjectDir(),TEXT("Shaders")));
    }
};
IMPLEMENT_MODULE(FNeonCityOceanModule,NeonCityOcean)
