#include "OceanExperienceLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraSpriteRendererProperties.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "Stateless/Modules/NiagaraStatelessModule_InitializeParticle.h"
#include "Stateless/Modules/NiagaraStatelessModule_AddVelocity.h"
#include "Stateless/Modules/NiagaraStatelessModule_GravityForce.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleSpriteSize.h"
#include "Stateless/Modules/NiagaraStatelessModule_DynamicMaterialParameters.h"
#include "Stateless/Modules/NiagaraStatelessModule_Drag.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

UNiagaraSystem* UOceanExperienceLibrary::CreateRegulatorExhaust(UMaterialInterface* Material)
{
 if(!Material)return nullptr;
 const TCHAR* Path=TEXT("/Game/NeonCity/Art/OceanExperience/FX/NS_RegulatorExhaust");
 auto* System=LoadObject<UNiagaraSystem>(nullptr,Path);
 if(!System)
 {
  auto* Template=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurstLightweight"));
  if(!Template)return nullptr;
  System=DuplicateObject<UNiagaraSystem>(Template,CreatePackage(Path),TEXT("NS_RegulatorExhaust"));
  System->SetFlags(RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(System);
 }
 if(System->GetEmitterHandles().Num()!=1)return nullptr;
 auto* Emitter=System->GetEmitterHandles()[0].GetStatelessEmitter();if(!Emitter)return nullptr;
 auto* Init=Cast<UNiagaraStatelessModule_InitializeParticle>(Emitter->GetModule(UNiagaraStatelessModule_InitializeParticle::StaticClass()));
 auto* Velocity=Cast<UNiagaraStatelessModule_AddVelocity>(Emitter->GetModule(UNiagaraStatelessModule_AddVelocity::StaticClass()));
 auto* Gravity=Cast<UNiagaraStatelessModule_GravityForce>(Emitter->GetModule(UNiagaraStatelessModule_GravityForce::StaticClass()));
 if(!Init || !Velocity || !Gravity)return nullptr;
 auto* Size=Cast<UNiagaraStatelessModule_ScaleSpriteSize>(Emitter->GetModule(UNiagaraStatelessModule_ScaleSpriteSize::StaticClass()));
 if(!Size)return nullptr;
 for(const auto& Module:Emitter->GetModules())if(Module->CanDisableModule())Module->SetIsModuleEnabled(Module==Velocity || Module==Gravity || Module==Size);
 // Lightweight Niagara does not export Particles.NormalizedAge to the sprite
 // renderer. Explicitly carry the normalized lifetime through DynamicParameter.x;
 // ParticleRelativeTime otherwise remains zero and erases the whole plume.
 auto* Dynamic=Cast<UNiagaraStatelessModule_DynamicMaterialParameters>(Emitter->GetModule(UNiagaraStatelessModule_DynamicMaterialParameters::StaticClass()));
 if(!Dynamic)return nullptr;
 Dynamic->SetIsModuleEnabled(true);Dynamic->bParameter0Enabled=true;
 auto& Age=Dynamic->Parameter0.XChannelDistribution;Age.Mode=ENiagaraDistributionMode::UniformCurve;
 Age.ChannelCurves.SetNum(1);Age.ChannelCurves[0].Reset();Age.ChannelCurves[0].AddKey(0,0);Age.ChannelCurves[0].AddKey(1,1);Age.UpdateValuesFromDistribution();
 Init->LifetimeDistribution=FNiagaraDistributionRangeFloat(2.6f,4.1f);
 Init->InitialPositionDistribution=FNiagaraDistributionPosition(FVector3f::ZeroVector);
 Init->ColorDistribution=FNiagaraDistributionColor(FLinearColor::White);
 Init->SpriteSizeDistribution.Mode=ENiagaraDistributionMode::UniformRange;
 Init->SpriteSizeDistribution.Min=FVector2f(.7f);Init->SpriteSizeDistribution.Max=FVector2f(3.2f);
 Size->ScaleDistribution.Mode=ENiagaraDistributionMode::UniformCurve;
 Size->ScaleDistribution.ChannelCurves.SetNum(1);
 auto& Curve=Size->ScaleDistribution.ChannelCurves[0];Curve.Reset();
 Curve.AddKey(0,.72f);Curve.AddKey(.18f,1.f);Curve.AddKey(.7f,1.32f);Curve.AddKey(1,1.52f);
 Size->ScaleDistribution.UpdateValuesFromDistribution();
 Velocity->VelocityType=ENSM_VelocityType::InCone;Velocity->CoordinateSpace=ENiagaraCoordinateSpace::World;
 Velocity->ConeRotationType=ENSM_ConeRotationType::Direction;
 FNiagaraVariable Direction(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.ReleaseDirection"));Direction.SetValue(FVector3f(0,0,1));
 FNiagaraVariable Speed(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.ReleaseSpeed"));Speed.SetValue(55.f);
 System->GetExposedParameters().AddParameter(Direction,true);System->GetExposedParameters().AddParameter(Speed,true);
 Velocity->ConeDirection.Mode=ENiagaraDistributionMode::Binding;Velocity->ConeDirection.ParameterBinding=Direction;
 Velocity->ConeVelocityDistribution=FNiagaraDistributionRangeFloat(.7f,1.2f);
 Velocity->ConeVelocityScale.Mode=ENiagaraDistributionMode::Binding;Velocity->ConeVelocityScale.ParameterBinding=Speed;
 Velocity->ConeAngle=23;Velocity->InnerCone=0;
 Gravity->GravityDistribution.InitConstant(FVector3f(0,0,10));
 for(int32 I=0;I<Emitter->GetNumSpawnInfos();++I)
 {
  auto* Spawn=Emitter->GetSpawnInfoByIndex(I);Spawn->bEnabled=I==0;
  Spawn->Type=ENiagaraStatelessSpawnInfoType::Burst;Spawn->SpawnTime=0;Spawn->Amount=FNiagaraDistributionRangeInt(7);
 }
 if(Emitter->GetNumSpawnInfos()==0)Emitter->AddSpawnInfo().Amount=FNiagaraDistributionRangeInt(5);
 auto* StateProperty=FindFProperty<FStructProperty>(Emitter->GetClass(),TEXT("EmitterState"));if(!StateProperty)return nullptr;
 auto* State=StateProperty->ContainerPtrToValuePtr<FNiagaraEmitterStateData>(Emitter);
 State->LoopBehavior=ENiagaraLoopBehavior::Once;State->LoopDurationMode=ENiagaraLoopDurationMode::Fixed;
 State->LoopDuration=FNiagaraDistributionRangeFloat(.1f);State->InactiveResponse=ENiagaraEmitterInactiveResponse::Complete;
 const FBox Bounds(FVector(-1200,-1200,-15),FVector(1200,1200,700));
 auto* BoundsProperty=FindFProperty<FStructProperty>(Emitter->GetClass(),TEXT("FixedBounds"));
 if(BoundsProperty)*BoundsProperty->ContainerPtrToValuePtr<FBox>(Emitter)=Bounds;
 System->SetFixedBounds(Bounds);
 FNiagaraVariable Surface(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SurfaceZ"));Surface.SetValue(-130.f);
 System->GetExposedParameters().AddParameter(Surface,true);
 for(auto* R:Emitter->GetRenderers())if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(R))
 {
  Sprite->Material=Material;
  Sprite->FacingMode=ENiagaraSpriteFacingMode::FaceCamera;
  Sprite->Alignment=ENiagaraSpriteAlignment::Unaligned;
  FNiagaraMaterialAttributeBinding Binding;Binding.MaterialParameterName=TEXT("SurfaceZ");Binding.NiagaraVariable=Surface;
  Binding.CacheValues(Emitter);Sprite->MaterialParameters.AttributeBindings={Binding};
  Sprite->PostEditChange();
 }
 Emitter->PostEditChange();System->PostEditChange();System->RequestCompile(true);System->MarkPackageDirty();return System;
}
UNiagaraSystem* UOceanExperienceLibrary::CreateDPVWash(UMaterialInterface* Material)
{
 if(!Material)return nullptr;
 const TCHAR* Path=TEXT("/Game/NeonCity/Art/OceanExperience/FX/NS_DPVWash");
 auto* System=LoadObject<UNiagaraSystem>(nullptr,Path);
 if(!System)
 {
  auto* Source=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/FX/NS_RegulatorExhaust"));if(!Source)return nullptr;
  System=DuplicateObject<UNiagaraSystem>(Source,CreatePackage(Path),TEXT("NS_DPVWash"));System->SetFlags(RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(System);
 }
 auto* Emitter=System->GetEmitterHandles()[0].GetStatelessEmitter();if(!Emitter)return nullptr;
 auto* Init=Cast<UNiagaraStatelessModule_InitializeParticle>(Emitter->GetModule(UNiagaraStatelessModule_InitializeParticle::StaticClass()));
 auto* Velocity=Cast<UNiagaraStatelessModule_AddVelocity>(Emitter->GetModule(UNiagaraStatelessModule_AddVelocity::StaticClass()));
 auto* Gravity=Cast<UNiagaraStatelessModule_GravityForce>(Emitter->GetModule(UNiagaraStatelessModule_GravityForce::StaticClass()));
 if(!Init || !Velocity || !Gravity)return nullptr;
 Init->LifetimeDistribution=FNiagaraDistributionRangeFloat(.5f,.95f);
 Init->SpriteSizeDistribution.Mode=ENiagaraDistributionMode::NonUniformRange;Init->SpriteSizeDistribution.Min=FVector2f(1,6);Init->SpriteSizeDistribution.Max=FVector2f(3,14);
 Init->InitialPositionDistribution.Mode=ENiagaraDistributionMode::NonUniformRange;Init->InitialPositionDistribution.Values={FVector3f(-6,-6,-6),FVector3f(6,6,6)};
 Velocity->ConeAngle=16;Gravity->GravityDistribution.InitConstant(FVector3f::ZeroVector);
 for(int32 I=0;I<Emitter->GetNumSpawnInfos();++I)Emitter->GetSpawnInfoByIndex(I)->Amount=FNiagaraDistributionRangeInt(10);
 for(auto* R:Emitter->GetRenderers())if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(R)){Sprite->Material=Material;Sprite->Alignment=ENiagaraSpriteAlignment::VelocityAligned;Sprite->PostEditChange();}
 Emitter->PostEditChange();System->PostEditChange();System->RequestCompile(true);System->MarkPackageDirty();return System;
}
UNiagaraSystem* UOceanExperienceLibrary::CreateBowContactSpray(UMaterialInterface* Material)
{ return CreateMarineSprayLayer(Material,0); }
UNiagaraSystem* UOceanExperienceLibrary::CreateMarineSnow(UMaterialInterface* Material)
{
 if(!Material)return nullptr;
 const TCHAR* Path=TEXT("/Game/NeonCity/Art/OceanExperience/FX/NS_MarineSnow");
 auto* System=LoadObject<UNiagaraSystem>(nullptr,Path);
 if(!System)
 {
  auto* Template=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurstLightweight"));
  if(!Template)return nullptr;
  System=DuplicateObject<UNiagaraSystem>(Template,CreatePackage(Path),TEXT("NS_MarineSnow"));
  System->SetFlags(RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(System);
 }
 if(System->GetEmitterHandles().Num()!=1)return nullptr;
 auto* Emitter=System->GetEmitterHandles()[0].GetStatelessEmitter();if(!Emitter)return nullptr;
 auto* Init=Cast<UNiagaraStatelessModule_InitializeParticle>(Emitter->GetModule(UNiagaraStatelessModule_InitializeParticle::StaticClass()));
 auto* Velocity=Cast<UNiagaraStatelessModule_AddVelocity>(Emitter->GetModule(UNiagaraStatelessModule_AddVelocity::StaticClass()));
 auto* Dynamic=Cast<UNiagaraStatelessModule_DynamicMaterialParameters>(Emitter->GetModule(UNiagaraStatelessModule_DynamicMaterialParameters::StaticClass()));
 if(!Init || !Velocity || !Dynamic)return nullptr;
 for(const auto& Module:Emitter->GetModules())if(Module->CanDisableModule())Module->SetIsModuleEnabled(Module==Velocity || Module==Dynamic);
 Dynamic->bParameter0Enabled=true;
 auto& Age=Dynamic->Parameter0.XChannelDistribution;Age.Mode=ENiagaraDistributionMode::UniformCurve;
 Age.ChannelCurves.SetNum(1);Age.ChannelCurves[0].Reset();Age.ChannelCurves[0].AddKey(0,0);Age.ChannelCurves[0].AddKey(1,1);Age.UpdateValuesFromDistribution();
 Init->LifetimeDistribution=FNiagaraDistributionRangeFloat(16,24);
 Init->ColorDistribution=FNiagaraDistributionColor(FLinearColor::White);
 Init->InitialPositionDistribution.Mode=ENiagaraDistributionMode::NonUniformRange;
 Init->InitialPositionDistribution.Values={FVector3f(-600),FVector3f(600)};
 Init->SpriteSizeDistribution.Mode=ENiagaraDistributionMode::UniformRange;
 Init->SpriteSizeDistribution.Min=FVector2f(.18f);Init->SpriteSizeDistribution.Max=FVector2f(.75f);
 Init->SpriteRotationDistribution=FNiagaraDistributionRangeFloat(-180,180);
 Velocity->VelocityType=ENSM_VelocityType::Linear;Velocity->CoordinateSpace=ENiagaraCoordinateSpace::World;
 Velocity->LinearVelocityDistribution.Mode=ENiagaraDistributionMode::NonUniformRange;
 Velocity->LinearVelocityDistribution.Min=FVector3f(-.6f,-.4f,-.65f);Velocity->LinearVelocityDistribution.Max=FVector3f(1.2f,.9f,-.12f);
 Velocity->LinearVelocityScale=FNiagaraDistributionRangeFloat(1);
 if(Emitter->GetNumSpawnInfos()==0)Emitter->AddSpawnInfo();
 for(int32 I=0;I<Emitter->GetNumSpawnInfos();++I)
 {
  auto* Spawn=Emitter->GetSpawnInfoByIndex(I);Spawn->bEnabled=I==0;
  Spawn->Type=ENiagaraStatelessSpawnInfoType::Burst;Spawn->SpawnTime=0;Spawn->Amount=FNiagaraDistributionRangeInt(140);
 }
 auto* P=FindFProperty<FStructProperty>(Emitter->GetClass(),TEXT("EmitterState"));if(!P)return nullptr;
 auto* State=P->ContainerPtrToValuePtr<FNiagaraEmitterStateData>(Emitter);
 State->LoopBehavior=ENiagaraLoopBehavior::Once;State->LoopDurationMode=ENiagaraLoopDurationMode::Fixed;
 State->LoopDuration=FNiagaraDistributionRangeFloat(.05f);State->InactiveResponse=ENiagaraEmitterInactiveResponse::Complete;
 const FBox Bounds(FVector(-650),FVector(650));
 if(auto* B=FindFProperty<FStructProperty>(Emitter->GetClass(),TEXT("FixedBounds")))*B->ContainerPtrToValuePtr<FBox>(Emitter)=Bounds;
 System->SetFixedBounds(Bounds);
 FNiagaraVariable Surface(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SurfaceZ"));Surface.SetValue(-130.f);
 System->GetExposedParameters().AddParameter(Surface,true);
 for(auto* R:Emitter->GetRenderers())if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(R))
 {
  Sprite->Material=Material;Sprite->FacingMode=ENiagaraSpriteFacingMode::FaceCamera;
  Sprite->Alignment=ENiagaraSpriteAlignment::Unaligned;
  FNiagaraMaterialAttributeBinding Binding;Binding.MaterialParameterName=TEXT("SurfaceZ");Binding.NiagaraVariable=Surface;
  Binding.CacheValues(Emitter);Sprite->MaterialParameters.AttributeBindings={Binding};Sprite->PostEditChange();
 }
 Emitter->PostEditChange();System->PostEditChange();System->RequestCompile(true);System->MarkPackageDirty();return System;
}
UNiagaraSystem* UOceanExperienceLibrary::CreateMarineSprayLayer(UMaterialInterface* Material,int32 Kind)
{
 if(!Material || Kind<0 || Kind>4)return nullptr;
 const TCHAR* Names[]={TEXT("NS_BowContactSpray"),TEXT("NS_BowMist"),TEXT("NS_BowLigaments"),TEXT("NS_BowWhitewater"),TEXT("NS_BowSheet")};
 const FString Path=FString(TEXT("/Game/NeonCity/Art/OceanExperience/FX/"))+Names[Kind];
 auto* System=LoadObject<UNiagaraSystem>(nullptr,*Path);
 if(!System)
 {
  auto* Template=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurstLightweight"));
  if(!Template)return nullptr;
  System=DuplicateObject<UNiagaraSystem>(Template,CreatePackage(*Path),Names[Kind]);
  System->SetFlags(RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(System);
 }
 if(System->GetEmitterHandles().Num()!=1)return nullptr;
 auto* Emitter=System->GetEmitterHandles()[0].GetStatelessEmitter();if(!Emitter)return nullptr;
 auto* Init=Cast<UNiagaraStatelessModule_InitializeParticle>(Emitter->GetModule(UNiagaraStatelessModule_InitializeParticle::StaticClass()));
 auto* Velocity=Cast<UNiagaraStatelessModule_AddVelocity>(Emitter->GetModule(UNiagaraStatelessModule_AddVelocity::StaticClass()));
 auto* Gravity=Cast<UNiagaraStatelessModule_GravityForce>(Emitter->GetModule(UNiagaraStatelessModule_GravityForce::StaticClass()));
 if(!Init || !Velocity || !Gravity)return nullptr;
 auto* Drag=Cast<UNiagaraStatelessModule_Drag>(Emitter->GetModule(UNiagaraStatelessModule_Drag::StaticClass()));
 auto* Size=Cast<UNiagaraStatelessModule_ScaleSpriteSize>(Emitter->GetModule(UNiagaraStatelessModule_ScaleSpriteSize::StaticClass()));
 auto* Dynamic=Cast<UNiagaraStatelessModule_DynamicMaterialParameters>(Emitter->GetModule(UNiagaraStatelessModule_DynamicMaterialParameters::StaticClass()));
 if(!Drag || !Size || !Dynamic)return nullptr;
 for(const auto& Module:Emitter->GetModules())if(Module->CanDisableModule())Module->SetIsModuleEnabled(Module==Velocity || Module==Gravity || Module==Drag || Module==Size || Module==Dynamic);
 Dynamic->bParameter0Enabled=true;
 auto& Age=Dynamic->Parameter0.XChannelDistribution;Age.Mode=ENiagaraDistributionMode::UniformCurve;
 Age.ChannelCurves.SetNum(1);Age.ChannelCurves[0].Reset();Age.ChannelCurves[0].AddKey(0,0);Age.ChannelCurves[0].AddKey(1,1);Age.UpdateValuesFromDistribution();
 // Tidewater Spray.js per-kind drag, gravity and growth, adapted to cm and
 // native lightweight Niagara. No CPU particle tick or per-drop component.
 auto& Seed=Dynamic->Parameter0.YChannelDistribution;Seed.Mode=ENiagaraDistributionMode::UniformRange;Seed.Values={0.f,1.f};
 const float LifeMin[]={.65f,1.3f,.65f,.6f,.3f},LifeMax[]={1.2f,2.f,1.2f,1.f,.5f};
 const float Tau[]={3.f,.35f,5.f,1.8f,2.5f},G[]={980.f,30.f,980.f,850.f,980.f};
 const float Grow[]={0.f,.44f,0.f,.3f,.35f};
 // Resolved clumps represent clusters of small drops. Sub-centimetre quads
 // disappeared below one pixel at the ordinary boat camera after temporal AA.
 const FVector2f MinSize[]={FVector2f(1.f,2.f),{24,24},{1.8f,6.f},{12,18},{16,24}};
 const FVector2f MaxSize[]={FVector2f(2.5f,5.f),{50,50},{4.f,14.f},{30,44},{32,50}};
 Init->LifetimeDistribution=FNiagaraDistributionRangeFloat(LifeMin[Kind],LifeMax[Kind]);
 Init->SpriteSizeDistribution.Mode=ENiagaraDistributionMode::NonUniformRange;
 Init->SpriteSizeDistribution.Min=MinSize[Kind];Init->SpriteSizeDistribution.Max=MaxSize[Kind];
 Init->SpriteRotationDistribution=FNiagaraDistributionRangeFloat(-30,30);
 Init->InitialPositionDistribution.Mode=ENiagaraDistributionMode::NonUniformRange;
 Init->InitialPositionDistribution.Values={FVector3f(-16,-8,-2),FVector3f(16,8,2)};
 Size->ScaleDistribution.Mode=ENiagaraDistributionMode::UniformCurve;Size->ScaleDistribution.ChannelCurves.SetNum(1);
 auto& Curve=Size->ScaleDistribution.ChannelCurves[0];Curve.Reset();Curve.AddKey(0,.85f);Curve.AddKey(.12f,1);Curve.AddKey(1,1+Grow[Kind]);Size->ScaleDistribution.UpdateValuesFromDistribution();
 Drag->DragDistribution=FNiagaraDistributionRangeFloat(1.f/Tau[Kind]);
 // Author the cone explicitly: the template's orientation is not part of the
 // contact model. World-space direction also keeps old spray independent of
 // the rotating hull and makes gravity remain vertical.
 Velocity->VelocityType=ENSM_VelocityType::InCone;Velocity->CoordinateSpace=ENiagaraCoordinateSpace::World;
 Velocity->ConeRotationType=ENSM_ConeRotationType::Direction;
 FNiagaraVariable Direction(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.LaunchDirection"));Direction.SetValue(FVector3f(0,0,1));
 System->GetExposedParameters().AddParameter(Direction,true);
 Velocity->ConeDirection.Mode=ENiagaraDistributionMode::Binding;Velocity->ConeDirection.ParameterBinding=Direction;
 Velocity->ConeVelocityDistribution=FNiagaraDistributionRangeFloat(.7f,1.3f);Velocity->ConeAngle=18;Velocity->InnerCone=0;
 Velocity->ConeVelocityScale.Mode=ENiagaraDistributionMode::Binding;
 FNiagaraVariable Speed(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.LaunchSpeed"));Speed.SetValue(300.f);
 Velocity->ConeVelocityScale.ParameterBinding=Speed;System->GetExposedParameters().AddParameter(Speed,true);
 FNiagaraVariable SunDirection(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.SpraySunDirection"));SunDirection.SetValue(FVector3f(0,0,1));
 FNiagaraVariable SunRadiance(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.SpraySunRadiance"));SunRadiance.SetValue(FVector3f::ZeroVector);
 System->GetExposedParameters().AddParameter(SunDirection,true);System->GetExposedParameters().AddParameter(SunRadiance,true);
 Gravity->GravityDistribution.InitConstant(FVector3f(0,0,-G[Kind]));
 FNiagaraVariable Count(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.DropCount"));Count.SetValue(48);
 System->GetExposedParameters().AddParameter(Count,true);
 if(Emitter->GetNumSpawnInfos()==0)Emitter->AddSpawnInfo();
 for(int32 I=0;I<Emitter->GetNumSpawnInfos();++I)
 {
  auto* Spawn=Emitter->GetSpawnInfoByIndex(I);Spawn->bEnabled=I==0;
  Spawn->Type=ENiagaraStatelessSpawnInfoType::Burst;Spawn->SpawnTime=0;
  Spawn->Amount.Mode=ENiagaraDistributionMode::Binding;Spawn->Amount.ParameterBinding=Count;
 }
 auto* StateProperty=FindFProperty<FStructProperty>(Emitter->GetClass(),TEXT("EmitterState"));if(!StateProperty)return nullptr;
 auto* State=StateProperty->ContainerPtrToValuePtr<FNiagaraEmitterStateData>(Emitter);
 State->LoopBehavior=ENiagaraLoopBehavior::Once;State->LoopDurationMode=ENiagaraLoopDurationMode::Fixed;
 State->LoopDuration=FNiagaraDistributionRangeFloat(.05f);State->InactiveResponse=ENiagaraEmitterInactiveResponse::Complete;
 const FBox Bounds(FVector(-2200),FVector(2200));
 if(auto* P=FindFProperty<FStructProperty>(Emitter->GetClass(),TEXT("FixedBounds")))*P->ContainerPtrToValuePtr<FBox>(Emitter)=Bounds;
 System->SetFixedBounds(Bounds);
 FNiagaraVariable Surface(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SurfaceZ"));Surface.SetValue(-130.f);
 System->GetExposedParameters().AddParameter(Surface,true);
 for(auto* R:Emitter->GetRenderers())if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(R))
 {
  Sprite->Material=Material;
  Sprite->FacingMode=ENiagaraSpriteFacingMode::FaceCamera;
  Sprite->Alignment=Kind==1?ENiagaraSpriteAlignment::Unaligned:ENiagaraSpriteAlignment::VelocityAligned;
  FNiagaraMaterialAttributeBinding Binding;Binding.MaterialParameterName=TEXT("SurfaceZ");Binding.NiagaraVariable=Surface;
  Binding.CacheValues(Emitter);Sprite->MaterialParameters.AttributeBindings={Binding};
  for(const auto& Parameter:{SunDirection,SunRadiance})
  {
   FNiagaraMaterialAttributeBinding LightBinding;
   LightBinding.MaterialParameterName=Parameter==SunDirection?TEXT("SpraySunDirection"):TEXT("SpraySunRadiance");
   LightBinding.NiagaraVariable=Parameter;LightBinding.CacheValues(Emitter);Sprite->MaterialParameters.AttributeBindings.Add(LightBinding);
  }
  Sprite->PostEditChange();
 }
 Emitter->PostEditChange();System->PostEditChange();System->RequestCompile(true);System->MarkPackageDirty();return System;
}
bool UOceanExperienceLibrary::AuthorRegulatorSocket(USkeletalMesh* Suit)
{
 if(!Suit)return false;const auto& Ref=Suit->GetRefSkeleton();const int32 Index=Ref.FindBoneIndex(TEXT("head"));if(Index<0)return false;
 FTransform Head=Ref.GetRefBonePose()[Index];
 for(int32 Parent=Ref.GetParentIndex(Index);Parent!=INDEX_NONE;Parent=Ref.GetParentIndex(Parent))Head=Head*Ref.GetRefBonePose()[Parent];
 USkeletalMeshSocket* Socket=nullptr;
 for(const auto& S:Suit->GetMeshOnlySocketList())if(S->SocketName==TEXT("DiveExhaust")){Socket=S;break;}
 if(!Socket){Socket=NewObject<USkeletalMeshSocket>(Suit);Suit->GetMeshOnlySocketList().Add(Socket);}
 Socket->SocketName=TEXT("DiveExhaust");Socket->BoneName=TEXT("head");
 // Blender's reviewed lower-mask exhaust at (0,-.19,1.586)m, converted to UE's left-handed centimetres.
 Socket->RelativeLocation=Head.InverseTransformPosition(FVector(0,19,158.6));Socket->RelativeRotation=FRotator::ZeroRotator;
 Socket->bForceAlwaysAnimated=true;Suit->MarkPackageDirty();return true;
}
bool UOceanExperienceLibrary::ConfigureDrysuitLods(USkeletalMesh* Suit)
{
 if(!Suit || Suit->GetLODNum()!=4)return false;
 Suit->Modify();
 const float Fractions[]={1.f,.45f,.18f,.06f};
 const float Screens[]={1.f,.28f,.14f,.045f};
 for(int32 I=0;I<4;++I)
 {
  auto* LOD=Suit->GetLODInfo(I);LOD->ScreenSize.Default=Screens[I];
  LOD->ReductionSettings.NumOfTrianglesPercentage=Fractions[I];
  LOD->ReductionSettings.bRemapMorphTargets=false;
 }
 return true;
}
