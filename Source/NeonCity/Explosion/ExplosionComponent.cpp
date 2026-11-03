#include "Explosion/ExplosionComponent.h"
#include "Explosion/ExplosionSubsystem.h"
#include "Engine/World.h"
UNeonCityExplosionComponent::UNeonCityExplosionComponent() { PrimaryComponentTick.bCanEverTick=false; }
bool UNeonCityExplosionComponent::SetExplosionDataID(FName ID)
{
 if(ID.IsNone()) return false;
 ExplosionDataID=ID; ExplosionProfile=nullptr; return true;
}
FNeonCityExplosionResult UNeonCityExplosionComponent::DetonateAt(FVector Center,AController* Instigator,AActor* SourceActor,AActor* IgnoredActor,int32 ChainDepth,FGuid ParentExplosionId)
{
 FNeonCityExplosionResult Result;
 if(!GetOwner() || !GetOwner()->HasAuthority()) { Result.RefusedReason=TEXT("Client or missing owner."); return Result; }
 auto* System=GetWorld()?GetWorld()->GetSubsystem<UNeonCityExplosionSubsystem>():nullptr;
 const UNeonCityExplosionData* Data=GetExplosionData();
 if(!System || !Data) { Result.RefusedReason=TEXT("Explosion subsystem unavailable."); return Result; }
 UNeonCityExplosionSubsystem::FRequest Request;
 Request.Data=Data; Request.Center=Center; Request.Instigator=Instigator;
 Request.SourceActor=SourceActor?SourceActor:GetOwner();
 Request.ChainDepth=ChainDepth; Request.ParentExplosionId=ParentExplosionId;
 Request.Ignored.Add(GetOwner()); Request.Ignored.Add(IgnoredActor);
 Result=System->Detonate(Request);
 if(Result.bApplied) ++Detonations;
 return Result;
}
FNeonCityExplosionResult UNeonCityExplosionComponent::DetonateAtOwnLocation(AController* Instigator,int32 ChainDepth,FGuid ParentExplosionId)
{
 const FVector Center=GetOwner()?GetOwner()->GetActorLocation():FVector::ZeroVector;
 return DetonateAt(Center,Instigator,GetOwner(),nullptr,ChainDepth,ParentExplosionId);
}