#pragma once
#include "Engine/Engine.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "Misc/CommandLine.h"
// A single normal game world avoids keeping an unused editor render scene in
// VRAM. This opt-in validation never changes production rendering or controls.
inline UWorld* NeonCityValidationWorld()
{
#if WITH_EDITOR
 if(GEditor && GEditor->PlayWorld)return GEditor->PlayWorld.Get();
#endif
 if(!GEngine || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")))return nullptr;
 for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::Game)return Context.World();
 return nullptr;
}
