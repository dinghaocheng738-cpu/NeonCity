#include "WeaponFXLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraSpriteRendererProperties.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "Stateless/Modules/NiagaraStatelessModule_InitializeParticle.h"
#include "Stateless/Modules/NiagaraStatelessModule_AddVelocity.h"
#include "Stateless/Modules/NiagaraStatelessModule_GravityForce.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleSpriteSize.h"
#include "Stateless/Modules/NiagaraStatelessModule_DynamicMaterialParameters.h"
#include "Stateless/Modules/NiagaraStatelessModule_Drag.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace NeonCityWeaponFX
{
struct FBurst
{
 const TCHAR* Name;int32 Count;float LifeMin,LifeMax,SizeMin,SizeMax,Grow,SpeedMin,SpeedMax,Cone,UpBias,Gravity,DragPerSecond,Spread;
 bool bVelocityAligned;FLinearColor Color;float Bounds;
};
// cm, s. Smoke grows and slows; the fireball balloons fast; sparks fly far and fall.
const FBurst Bursts[]=
{
 {TEXT("NS_NC_TrailPuff"),2,3.2f,5.f,70,130,4.2f,10,60,180,25,0,1.2f,18,false,FLinearColor(.92f,.92f,.9f,.85f),1500},
 {TEXT("NS_NC_LaunchSmoke"),16,2.f,3.6f,90,170,3.f,150,520,180,60,0,2.2f,60,false,FLinearColor(.95f,.94f,.9f,.9f),2500},
 {TEXT("NS_NC_Fireball"),40,.35f,.85f,160,380,2.4f,250,1100,180,250,0,4.5f,120,false,FLinearColor(1.f,1.f,1.f,1.f),3000},
 {TEXT("NS_NC_BlastSmoke"),26,5.f,9.f,280,560,2.6f,120,420,180,260,-40,1.1f,160,false,FLinearColor(.3f,.29f,.28f,.9f),6000},
 {TEXT("NS_NC_Sparks"),70,.6f,1.6f,4,9,1.f,1400,3800,180,900,980,.8f,40,true,FLinearColor(1.f,1.f,1.f,1.f),6000},
};
}
using namespace NeonCityWeaponFX;

UNiagaraSystem* UWeaponFXLibrary::CreateWeaponBurst(UMaterialInterface* Material,int32 Kind)
{
 if(!Material || Kind<0 || Kind>=UE_ARRAY_COUNT(Bursts))return nullptr;
 const FBurst& S=Bursts[Kind];
 const FString Path=FString(TEXT("/Game/NeonCity/Art/WeaponFX/FX/"))+S.Name;
 auto* System=LoadObject<UNiagaraSystem>(nullptr,*Path);
 if(!System)
 {
  auto* Template=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurstLightweight"));
  if(!Template)return nullptr;
  System=DuplicateObject<UNiagaraSystem>(Template,CreatePackage(*Path),S.Name);
  System->SetFlags(RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(System);
 }
 if(System->GetEmitterHandles().Num()!=1)return nullptr;
 auto* Emitter=System->GetEmitterHandles()[0].GetStatelessEmitter();if(!Emitter)return nullptr;
 auto* Init=Cast<UNiagaraStatelessModule_InitializeParticle>(Emitter->GetModule(UNiagaraStatelessModule_InitializeParticle::StaticClass()));
 auto* Velocity=Cast<UNiagaraStatelessModule_AddVelocity>(Emitter->GetModule(UNiagaraStatelessModule_AddVelocity::StaticClass()));
 auto* Gravity=Cast<UNiagaraStatelessModule_GravityForce>(Emitter->GetModule(UNiagaraStatelessModule_GravityForce::StaticClass()));
 auto* Drag=Cast<UNiagaraStatelessModule_Drag>(Emitter->GetModule(UNiagaraStatelessModule_Drag::StaticClass()));
 auto* Size=Cast<UNiagaraStatelessModule_ScaleSpriteSize>(Emitter->GetModule(UNiagaraStatelessModule_ScaleSpriteSize::StaticClass()));
 auto* Dynamic=Cast<UNiagaraStatelessModule_DynamicMaterialParameters>(Emitter->GetModule(UNiagaraStatelessModule_DynamicMaterialParameters::StaticClass()));
 if(!Init || !Velocity || !Gravity || !Drag || !Size || !Dynamic)return nullptr;
 for(const auto& Module:Emitter->GetModules())if(Module->CanDisableModule())Module->SetIsModuleEnabled(Module==Velocity || Module==Gravity || Module==Drag || Module==Size || Module==Dynamic);
 // Lightweight particles do not hand NormalizedAge to the material: carry it (and a per-particle seed) through
 // DynamicParameter 0 (x = age 0..1, y = random 0..1) for the sprite materials' fades and colour ramps.
 Dynamic->bParameter0Enabled=true;
 auto& Age=Dynamic->Parameter0.XChannelDistribution;Age.Mode=ENiagaraDistributionMode::UniformCurve;
 Age.ChannelCurves.SetNum(1);Age.ChannelCurves[0].Reset();Age.ChannelCurves[0].AddKey(0,0);Age.ChannelCurves[0].AddKey(1,1);Age.UpdateValuesFromDistribution();
 auto& Seed=Dynamic->Parameter0.YChannelDistribution;Seed.Mode=ENiagaraDistributionMode::UniformRange;Seed.Values={0.f,1.f};
 Init->LifetimeDistribution=FNiagaraDistributionRangeFloat(S.LifeMin,S.LifeMax);
 Init->ColorDistribution=FNiagaraDistributionColor(S.Color);
 Init->SpriteSizeDistribution.Mode=ENiagaraDistributionMode::UniformRange;
 Init->SpriteSizeDistribution.Min=FVector2f(S.SizeMin);Init->SpriteSizeDistribution.Max=FVector2f(S.SizeMax);
 Init->SpriteRotationDistribution=FNiagaraDistributionRangeFloat(-180,180);
 Init->InitialPositionDistribution.Mode=ENiagaraDistributionMode::NonUniformRange;
 Init->InitialPositionDistribution.Values={FVector3f(-S.Spread),FVector3f(S.Spread)};
 Size->ScaleDistribution.Mode=ENiagaraDistributionMode::UniformCurve;Size->ScaleDistribution.ChannelCurves.SetNum(1);
 auto& Curve=Size->ScaleDistribution.ChannelCurves[0];Curve.Reset();
 Curve.AddKey(0,S.bVelocityAligned?1.f:.55f);Curve.AddKey(.25f,FMath::Lerp(1.f,S.Grow,.55f));Curve.AddKey(1,S.Grow);
 Size->ScaleDistribution.UpdateValuesFromDistribution();
 Drag->DragDistribution=FNiagaraDistributionRangeFloat(S.DragPerSecond);
 // velocity: a cone about up (180 deg = all round) plus a world-up bias for rising smoke
 Velocity->VelocityType=ENSM_VelocityType::InCone;Velocity->CoordinateSpace=ENiagaraCoordinateSpace::World;
 Velocity->ConeRotationType=ENSM_ConeRotationType::Direction;
 Velocity->ConeDirection.InitConstant(FVector3f(0,0,1));
 Velocity->ConeVelocityDistribution=FNiagaraDistributionRangeFloat(S.SpeedMin,S.SpeedMax);Velocity->ConeAngle=S.Cone;Velocity->InnerCone=0;
 Velocity->ConeVelocityScale=FNiagaraDistributionRangeFloat(1);
 Gravity->GravityDistribution.InitConstant(FVector3f(0,0,S.UpBias>0 && S.Gravity<=0?-S.Gravity+S.UpBias*S.DragPerSecond:-S.Gravity));
 if(Emitter->GetNumSpawnInfos()==0)Emitter->AddSpawnInfo();
 for(int32 I=0;I<Emitter->GetNumSpawnInfos();++I)
 {
  auto* Spawn=Emitter->GetSpawnInfoByIndex(I);Spawn->bEnabled=I==0;
  Spawn->Type=ENiagaraStatelessSpawnInfoType::Burst;Spawn->SpawnTime=0;Spawn->Amount=FNiagaraDistributionRangeInt(S.Count);
 }
 auto* StateProperty=FindFProperty<FStructProperty>(Emitter->GetClass(),TEXT("EmitterState"));if(!StateProperty)return nullptr;
 auto* State=StateProperty->ContainerPtrToValuePtr<FNiagaraEmitterStateData>(Emitter);
 State->LoopBehavior=ENiagaraLoopBehavior::Once;State->LoopDurationMode=ENiagaraLoopDurationMode::Fixed;
 State->LoopDuration=FNiagaraDistributionRangeFloat(.05f);State->InactiveResponse=ENiagaraEmitterInactiveResponse::Complete;
 const FBox Bounds(FVector(-S.Bounds),FVector(S.Bounds));
 if(auto* P=FindFProperty<FStructProperty>(Emitter->GetClass(),TEXT("FixedBounds")))*P->ContainerPtrToValuePtr<FBox>(Emitter)=Bounds;
 System->SetFixedBounds(Bounds);
 for(auto* R:Emitter->GetRenderers())if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(R))
 {
  Sprite->Material=Material;Sprite->FacingMode=ENiagaraSpriteFacingMode::FaceCamera;
  Sprite->Alignment=S.bVelocityAligned?ENiagaraSpriteAlignment::VelocityAligned:ENiagaraSpriteAlignment::Unaligned;
  Sprite->SortMode=ENiagaraSortMode::ViewDistance;Sprite->PostEditChange();
 }
 Emitter->PostEditChange();System->PostEditChange();System->RequestCompile(true);System->WaitForCompilationComplete(false,false);System->MarkPackageDirty();return System;
}
