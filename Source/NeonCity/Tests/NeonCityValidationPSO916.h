#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ShaderPipelineCache.h"

// Automation's TestExit forces process termination before normal cache
// shutdown. Save once after the final timing sample, only in explicit PSO
// collection runs. Never enable broken periodic auto-save during gameplay.
inline bool NeonCitySaveValidationPSO916()
{
 return !FParse::Param(FCommandLine::Get(),TEXT("logPSO")) ||
  FShaderPipelineCache::SavePipelineFileCache(FPipelineFileCacheManager::SaveMode::BoundPSOsOnly);
}
#endif
