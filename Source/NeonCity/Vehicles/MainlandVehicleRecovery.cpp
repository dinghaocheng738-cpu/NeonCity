#include "Vehicles/MainlandVehicleRecovery.h"

#include "World/NeonCityMainlandSite.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Property/PropertySubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

namespace NeonCityVehiclePersistence
{
bool HasStaticVehicleObstruction(UWorld& World,const FTransform& Transform,FVector HalfExtents,FVector LocalOffset)
{
 FCollisionQueryParams Query(SCENE_QUERY_STAT(MainlandRetiredVehicleClearance),false);
 for(TActorIterator<APawn> It(&World);It;++It)Query.AddIgnoredActor(*It);
 TArray<FOverlapResult> Overlaps;
 World.OverlapMultiByObjectType(Overlaps,Transform.TransformPosition(LocalOffset),Transform.GetRotation(),
  FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeBox(HalfExtents),Query);
 for(const auto& Overlap:Overlaps)
  if(const auto* Component=Overlap.GetComponent();Component && !Component->IsSimulatingPhysics()
   && Component->GetCollisionResponseToChannel(ECC_Vehicle)==ECR_Block)return true;
 return false;
}

bool MigrateAuthoredMainlandVehicleProfile(const ANeonCityMainlandSite* Site,FNeonCityVehicleSaveData& Data)
{
 FName Replacement;
 if(!Site || !Site->GetFleetProfileReplacement(Data.VehicleID,Data.DataAssetID,Replacement))return false;
 Data.DataAssetID=Replacement;
 Data.VehicleType=uint8(ENeonCityVehicleType::SportsCar);
 return true;
}

namespace
{
 bool IsRetiredMainlandPosition(const ANeonCityMainlandSite& Site,const FVector& Point)
 {
  // Authored bounds may narrow the audited retired city, never capture an
  // island or offshore save through an accidentally oversized marker bound.
  const FBox RetiredEnvelope(FVector(-160000,-3000,-20000),FVector(160000,160000,40000));
  return !Point.ContainsNaN() && RetiredEnvelope.IsInsideOrOn(Point)
   && !Site.RetiredMainlandMin.ContainsNaN() && !Site.RetiredMainlandMax.ContainsNaN()
   && Site.RetiredMainlandMin.X<Site.RetiredMainlandMax.X && Site.RetiredMainlandMin.Y<Site.RetiredMainlandMax.Y
   && Site.RetiredMainlandMin.Z<Site.RetiredMainlandMax.Z
   && FBox(Site.RetiredMainlandMin,Site.RetiredMainlandMax).IsInsideOrOn(Point);
 }

 bool HasStaticParkingSupport(UWorld& World,const FTransform& Transform,bool RequireFullFootprint=false,double* HighestSupport=nullptr)
 {
  FCollisionQueryParams Query(SCENE_QUERY_STAT(MainlandRetiredParkingSupport),false);
  for(TActorIterator<APawn> It(&World);It;++It)Query.AddIgnoredActor(*It);
  const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
  // Conservative: even one supported point keeps the player's existing pose.
  // Static object queries exclude ocean/water volumes and moving physics props.
  bool AnySupported=false,AllSupported=true;
  if(HighestSupport)*HighestSupport=-TNumericLimits<double>::Max();
  for(const FVector Offset:{FVector::ZeroVector,FVector(180,80,0),FVector(180,-80,0),FVector(-180,80,0),FVector(-180,-80,0)})
  {
   const FVector Point=Transform.TransformPosition(Offset);FHitResult Hit;
   const bool Supported=World.LineTraceSingleByObjectType(Hit,Point+FVector(0,0,50),Point-FVector(0,0,500),StaticObjects,Query)
    && Hit.ImpactNormal.Z>.5 && Hit.GetComponent() && !Hit.GetComponent()->IsSimulatingPhysics();
   AnySupported|=Supported;AllSupported&=Supported;
   if(Supported && HighestSupport)*HighestSupport=FMath::Max(*HighestSupport,Hit.ImpactPoint.Z);
  }
  return RequireFullFootprint?AllSupported:AnySupported;
 }

 bool HasClearStaticChassis(UWorld& World,const FTransform& Transform)
 {
  // A solid road-car body envelope avoids counting door interaction boxes as
  // occupied space. Clip its lower face above measured support instead of
  // ignoring floor actors: one architectural mesh may contain floor AND walls.
  double FloorZ;
  HasStaticParkingSupport(World,Transform,false,&FloorZ);
  if(FloorZ>Transform.GetLocation().Z+10)return false; // A floor through the body is not safe support.
  const double Bottom=FMath::Max(Transform.GetLocation().Z-20,FloorZ+5);
  const double Top=Transform.GetLocation().Z+70;
  if(Bottom>=Top)return false;
  FTransform Upright=Transform;
  Upright.SetRotation(FRotator(0,Transform.Rotator().Yaw,0).Quaternion());
  return !HasStaticVehicleObstruction(World,Upright,FVector(220,90,(Top-Bottom)*.5),
   FVector(0,0,(Top+Bottom)*.5-Transform.GetLocation().Z));
 }

 template<typename Value,typename Predicate>
 const Value* UniqueMatch(TConstArrayView<Value> Values,Predicate Matches)
 {
  const Value* Found=nullptr;
  for(const auto& Item:Values)if(Matches(Item))
  {
   if(Found)return nullptr;
   Found=&Item;
  }
  return Found;
 }
}

bool MigrateDefaultMainlandParking(UWorld* World,FNeonCityVehicleSaveData& Data,
 const FNeonCityVehicleSaveData* AuthoredDefault,FString* Diagnostic,TConstArrayView<FNeonCityVehicleSaveData> SavedParkingChoices)
{
 if(Diagnostic)Diagnostic->Reset();
 const auto* Site=ANeonCityMainlandSite::Find(World);
 if(!Site || !Site->bRecoverUnsupportedMainlandVehicles || !Site->ActorHasTag(TEXT("NeonCity.MainlandC920"))
  || Data.LegacyFleetStored || !NeonCityPersistence::ValidTransform(Data.Transform))return false;
 struct FReleasedBay{FGuid ID;const TCHAR* CurrentProfile;};
 // C920 AdoptMainland920.py / Author8 parking-layout.json: released 3.4m
 // centres. The destination is the actual registered native C921 pose, never
 // a reconstructed root height or a save/schema-wide relocation policy.
 const FReleasedBay Bays[]={
  {FGuid(0x0889167F,0x430338D9,0x839BAEB8,0xF5500BB3),TEXT("DA_HeroKestrel")},
  {FGuid(0x6A26E40E,0x49A5ABC2,0xB2DD5E98,0xB399755E),TEXT("DA_HeroKestrel")},
  {FGuid(0x66CBA46E,0x4EFEE00C,0xBF50C299,0xAA9B4A0B),TEXT("DA_HeroNacre")},
  {FGuid(0xFE101992,0x4247593F,0x4BFD20A3,0x0519AE6C),TEXT("DA_HeroNacre")},
  {FGuid(0xBDBA4723,0x99505C57,0xAD706630,0x958711BA),TEXT("DA_HeroKestrel")},
  {FGuid(0x8D521A13,0x4E5857A2,0xBA5DEF01,0x745C71C8),TEXT("DA_HeroNacre")},
  {FGuid(0x65A15939,0x583852E0,0x88263B9A,0x8A474E12),TEXT("DA_HeroKestrel")}};
 int32 Index=INDEX_NONE;for(int32 I=0;I<UE_ARRAY_COUNT(Bays);++I)if(Data.VehicleID==Bays[I].ID){Index=I;break;}
 if(Index==INDEX_NONE || Data.DataAssetID!=Bays[Index].CurrentProfile || Data.VehicleType!=uint8(ENeonCityVehicleType::SportsCar))return false;
 const FVector OldDefault(12460.+340.*Index,4150.,90.);const FRotator Rotation=Data.Transform.Rotator();
 const FVector Point=Data.Transform.GetLocation();
 if(FVector::Dist2D(Point,OldDefault)>2. || Point.Z<40 || Point.Z>150
  || FMath::Abs(FMath::FindDeltaAngleDegrees(Rotation.Yaw,-90))>.5
  || FMath::Abs(Rotation.Pitch)>3 || FMath::Abs(Rotation.Roll)>3)return false;
 const auto Reject=[&](const TCHAR* Reason){if(Diagnostic)*Diagnostic=FString::Printf(TEXT("C920 default parking %s preserved: %s"),*Data.VehicleID.ToString(EGuidFormats::Digits),Reason);return false;};
 if(!FMath::IsFinite(Data.Condition) || Data.Condition<=0 || Data.Condition>1
  || !FMath::IsFinite(Data.RoofProgress) || Data.RoofProgress<0 || Data.RoofProgress>1)
  return Reject(TEXT("invalid/destroyed condition or roof state"));
 if(!AuthoredDefault || AuthoredDefault->VehicleID!=Data.VehicleID || AuthoredDefault->VehicleType!=Data.VehicleType
  || AuthoredDefault->DataAssetID!=Data.DataAssetID || AuthoredDefault->LegacyFleetStored
  || !AuthoredDefault->StoredPropertyID.IsNone() || AuthoredDefault->GarageSlot!=INDEX_NONE
  || !NeonCityPersistence::ValidTransform(AuthoredDefault->Transform))return Reject(TEXT("matching sealed live authored default unavailable"));
 const FTransform& Destination=AuthoredDefault->Transform;const FVector NewDefault(12340.+360.*Index,4150.,0);
 const FRotator NewRotation=Destination.Rotator();
 if(FVector::Dist2D(Destination.GetLocation(),NewDefault)>.1 || Destination.GetLocation().Z<40 || Destination.GetLocation().Z>150
  || FMath::Abs(FMath::FindDeltaAngleDegrees(NewRotation.Yaw,-90))>.01
  || FMath::Abs(NewRotation.Pitch)>.01 || FMath::Abs(NewRotation.Roll)>.01)
  return Reject(TEXT("current authored default is not the audited C921 bay"));
 // The final C920/C921 centre is identical. It needs no layout correction;
 // excluding it also prevents re-snapping a naturally settled current car.
 if(FVector::Dist2D(OldDefault,NewDefault)<=2.)return false;
 if(!Data.StoredPropertyID.IsNone() || Data.GarageSlot!=INDEX_NONE)
 {
  if(!Data.Owned || Data.StoredPropertyID!=TEXT("Property.Mainland.RivieraEstate916") || Data.GarageSlot!=Index)
   return Reject(TEXT("different/incomplete original property slot binding"));
  const auto* Properties=World->GetGameInstance()?World->GetGameInstance()->GetSubsystem<UPropertySubsystem>():nullptr;
  FNeonCityPropertyRecord Property;
  if(!Properties || !Properties->GetProperty(Data.StoredPropertyID,Property) || !Property.GarageSlots.IsValidIndex(Index)
   || Property.GarageSlots[Index].SlotIndex!=Index
   || FVector::Dist2D(Property.GarageSlots[Index].Location,Destination.GetLocation())>.1
   || FMath::Abs(Property.GarageSlots[Index].Location.Z-Destination.GetLocation().Z)>=150)
   return Reject(TEXT("same native property slot does not match the new authored bay"));
 }
 if(!IsRetiredMainlandPosition(*Site,Point) || !HasStaticParkingSupport(*World,Destination,true)
  || !HasClearStaticChassis(*World,Destination))return Reject(TEXT("destination lacks full real support or clear static chassis"));
 const auto Footprint=[](const FTransform& Pose)
 {
  FBox2D Bounds(ForceInit);for(const FVector Offset:{FVector(220,90,0),FVector(220,-90,0),FVector(-220,90,0),FVector(-220,-90,0)})
  {const FVector P=Pose.TransformPosition(Offset);Bounds+=FVector2D(P.X,P.Y);}return Bounds;
 };
 const FBox2D DestinationFootprint=Footprint(Destination);
 // The current live world still contains fresh map defaults. A saved player
 // choice elsewhere is applied later, so also reject a conservative footprint
 // intersection with that immutable input snapshot before changing this pose.
 for(const auto& Other:SavedParkingChoices)
  if(Other.VehicleID!=Data.VehicleID && Other.VehicleID.IsValid() && !Other.LegacyFleetStored
   && NeonCityPersistence::ValidTransform(Other.Transform) && FMath::IsFinite(Other.Condition) && Other.Condition>0 && Other.Condition<=1
   && RestoreVehicleClass(Other.VehicleType,Other.DataAssetID).Get()
   && FMath::Abs(Other.Transform.GetLocation().Z-Destination.GetLocation().Z)<140
   && DestinationFootprint.Intersect(Footprint(Other.Transform)))
   return Reject(TEXT("another saved parking choice occupies the new destination"));
 double FloorZ;HasStaticParkingSupport(*World,Destination,true,&FloorZ);
 const double Bottom=FMath::Max(Destination.GetLocation().Z-20,FloorZ+5),Top=Destination.GetLocation().Z+70;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(MainlandDefaultParkingDestination),false);
 if(auto* GI=World->GetGameInstance())if(auto* Registry=GI->GetSubsystem<UVehicleOwnershipSubsystem>())
  if(auto* Live=Registry->FindLiveVehicle(Data.VehicleID))Query.AddIgnoredActor(Live);
 if(World->OverlapBlockingTestByChannel(Destination.TransformPosition(FVector(0,0,(Top+Bottom)*.5-Destination.GetLocation().Z)),Destination.GetRotation(),
  ECC_Vehicle,FCollisionShape::MakeBox(FVector(220,90,(Top-Bottom)*.5)),Query))
  return Reject(TEXT("destination blocked by a real current collider"));
 Data.Transform=Destination;return true;
}

int32 MigrateAuthoredMainlandGarageParking(UWorld* World,
 TConstArrayView<FNeonCityPropertySaveData> OldProperties,
 TConstArrayView<FNeonCityPropertySaveData> NormalizedProperties,
 TArray<FNeonCityVehicleSaveData>& Vehicles)
{
 const auto* Site=ANeonCityMainlandSite::Find(World);
 if(!Site || !Site->bRecoverUnsupportedMainlandVehicles)return 0;
 int32 Changed=0;
 for(const FName PropertyID:{FName(TEXT("NeonCity.Property.StarterApartment")),FName(TEXT("Property.Mainland.RivieraEstate916"))})
 {
  const auto PropertyMatches=[PropertyID](const auto& Property){return Property.PropertyID==PropertyID;};
  const auto* OldProperty=UniqueMatch(OldProperties,PropertyMatches);
  const auto* NewProperty=UniqueMatch(NormalizedProperties,PropertyMatches);
  if(!OldProperty || !NewProperty || !OldProperty->Owned || !NewProperty->Owned)continue;
  for(const auto& NewSlot:NewProperty->GarageSlots)
  {
   if(NewSlot.SlotIndex<0 || !NewSlot.VehicleID.IsValid() || NewSlot.StoredLocation.ContainsNaN())continue;
   const auto SlotMatches=[&NewSlot](const auto& Slot){return Slot.SlotIndex==NewSlot.SlotIndex;};
   const auto* OldSlot=UniqueMatch(MakeArrayView(OldProperty->GarageSlots),SlotMatches);
   if(!OldSlot || !UniqueMatch(MakeArrayView(NewProperty->GarageSlots),SlotMatches)
    || OldSlot->VehicleID!=NewSlot.VehicleID || !IsRetiredMainlandPosition(*Site,OldSlot->StoredLocation)
    || FVector::Dist2D(OldSlot->StoredLocation,NewSlot.StoredLocation)<=250)continue;
   // An ambiguous/malformed record stays under normal domain validation.
   const auto VehicleMatches=[&NewSlot](const auto& Vehicle){return Vehicle.VehicleID==NewSlot.VehicleID;};
   const auto* Saved=UniqueMatch(MakeArrayView(static_cast<const TArray<FNeonCityVehicleSaveData>&>(Vehicles)),VehicleMatches);
   if(!Saved || !Saved->Owned || Saved->LegacyFleetStored || Saved->StoredPropertyID!=PropertyID
    || Saved->GarageSlot!=OldSlot->SlotIndex || !NeonCityPersistence::ValidTransform(Saved->Transform)
    || !FMath::IsFinite(Saved->Condition) || Saved->Condition<=0 || Saved->Condition>1
    || !FMath::IsFinite(Saved->RoofProgress) || Saved->RoofProgress<0 || Saved->RoofProgress>1
    || !RestoreVehicleClass(Saved->VehicleType,Saved->DataAssetID).Get()
    || FVector::Dist2D(Saved->Transform.GetLocation(),OldSlot->StoredLocation)>250
    || FMath::Abs(Saved->Transform.GetLocation().Z-OldSlot->StoredLocation.Z)>=150)continue;
   const FTransform Destination(FRotator(0,90,0),NewSlot.StoredLocation);
   // Unlike preservation of an existing player choice, relocation requires
   // support under every footprint point of the new authored bay.
   if(!NeonCityPersistence::ValidTransform(Destination) || !HasStaticParkingSupport(*World,Destination,true)
    || !HasClearStaticChassis(*World,Destination))continue;
   auto* Vehicle=Vehicles.FindByPredicate(VehicleMatches);
   Vehicle->Transform=Destination;
   ++Changed;
  }
 }
 return Changed;
}

EMainlandVehicleRecovery RecoverUnsupportedMainlandVehicle(UWorld* World,
 FNeonCityVehicleSaveData& Data,const FNeonCityVehicleSaveData* AuthoredDefault)
{
 const auto* Site=ANeonCityMainlandSite::Find(World);
 if(!Site || !Site->bRecoverUnsupportedMainlandVehicles || !Data.Owned || Data.LegacyFleetStored
  || !Data.StoredPropertyID.IsNone() || Data.GarageSlot!=INDEX_NONE || !Data.VehicleID.IsValid()
  || !NeonCityPersistence::ValidTransform(Data.Transform))return EMainlandVehicleRecovery::Unchanged;
 const FVector Point=Data.Transform.GetLocation();
 if(!IsRetiredMainlandPosition(*Site,Point)
  || (HasStaticParkingSupport(*World,Data.Transform) && HasClearStaticChassis(*World,Data.Transform)))return EMainlandVehicleRecovery::Unchanged;

 if(AuthoredDefault && AuthoredDefault->VehicleID==Data.VehicleID
  && AuthoredDefault->VehicleType==Data.VehicleType && AuthoredDefault->DataAssetID==Data.DataAssetID
  && !AuthoredDefault->LegacyFleetStored && AuthoredDefault->StoredPropertyID.IsNone()
  && AuthoredDefault->GarageSlot==INDEX_NONE && NeonCityPersistence::ValidTransform(AuthoredDefault->Transform)
  && HasStaticParkingSupport(*World,AuthoredDefault->Transform,true) && HasClearStaticChassis(*World,AuthoredDefault->Transform))
 {
  Data.Transform=AuthoredDefault->Transform;
  return EMainlandVehicleRecovery::AuthoredParking;
 }
 // Preserve the exact original identity/profile/pose/condition/roof in the
 // existing owned collection. Retrieval remains the existing garage action.
 Data.LegacyFleetStored=true;
 return EMainlandVehicleRecovery::OwnedStorage;
}
}
