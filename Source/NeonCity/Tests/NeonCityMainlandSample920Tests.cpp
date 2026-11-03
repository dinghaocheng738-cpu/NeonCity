#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandLandResidence.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/PedestrianSpawnPoint.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Components/SplineComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/NeonCityPlayerController.h"
#include "Property/NeonCityGarageMarker.h"
#include "Property/PropertyComponent.h"
#include "Property/PropertySubsystem.h"
#include "Vehicles/VehicleData.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/OverlapResult.h"
#include "Materials/MaterialInterface.h"
#include "World/NeonCityMainlandSite.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandSample920
{
class FSupport : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 bool bCourtOnly=false;
 bool bSurveyed=false;float PopulationAt=0;double PopulationWallAt=0;
 TArray<TWeakObjectPtr<APedestrianSpawnPoint>> PromenadeMarkers;
 TMap<TWeakObjectPtr<ANeonCityNPCCharacter>,FVector> PopulationOrigins;
 TSet<TWeakObjectPtr<ANeonCityNPCCharacter>> MovedPopulation;
 bool ObservePopulation(UWorld* W)
 {
  int32 Visible=0,Animated=0;TSet<FName> VisibleIDs;
  for(const auto& Marker:PromenadeMarkers)if(Marker.IsValid())
  {
   auto* NPC=Marker->GetSpawnedPedestrian();
   if(!NPC || NPC->IsHidden() || NPC->IsPopulationDormant() || !NPC->GetMesh()->IsVisible())continue;
   ++Visible;VisibleIDs.Add(NPC->GetNPCId_Implementation());
   const bool bAnimated=NPC->GetMesh()->GetSkeletalMeshAsset()
    && NPC->GetMesh()->GetAnimationMode()==EAnimationMode::AnimationBlueprint
    && Cast<UCharacterPresentationAnimInstance>(NPC->GetMesh()->GetAnimInstance());
   if(!bAnimated)continue;
   ++Animated;const TWeakObjectPtr<ANeonCityNPCCharacter> Key(NPC);
   const FVector Origin=PopulationOrigins.FindOrAdd(Key,NPC->GetActorLocation());
   if(FVector::Dist2D(Origin,NPC->GetActorLocation())>=80)MovedPopulation.Add(Key);
  }
  if(W->GetTimeSeconds()-PopulationAt<12 && FPlatformTime::Seconds()-PopulationWallAt<30)return false;
  Test->TestTrue(TEXT("New promenade has at least two actual visible pedestrians with distinct identities"),Visible>=2 && VisibleIDs.Num()>=2);
  Test->TestTrue(TEXT("New promenade has at least two actual characters using the native animation graph"),Animated>=2);
  TSet<FName> MovedIDs;int32 SupportedMovers=0;
  for(const auto& Key:MovedPopulation)if(Key.IsValid())
  {
   auto* NPC=Key.Get();const FVector P=NPC->GetActorLocation();
   MovedIDs.Add(NPC->GetNPCId_Implementation());
   FCollisionQueryParams Q(SCENE_QUERY_STAT(C920ActivePedestrianSupport),false,NPC);FHitResult H;
   const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
   const float Half=NPC->GetSimpleCollisionHalfHeight(),Radius=NPC->GetSimpleCollisionRadius();
   const bool bFloor=W->LineTraceSingleByObjectType(H,P+FVector(0,0,80),P-FVector(0,0,Half+60),StaticObjects,Q)
    && H.ImpactNormal.Z>.94 && FMath::Abs(H.ImpactPoint.Z-55)<3
    && FMath::Abs(P.Z-H.ImpactPoint.Z-Half)<6;
   TArray<FOverlapResult> Overlaps;W->OverlapMultiByObjectType(Overlaps,P,FQuat::Identity,StaticObjects,FCollisionShape::MakeCapsule(Radius,Half-2),Q);
   const bool bClear=!Overlaps.ContainsByPredicate([](const FOverlapResult& H){return H.GetComponent() && H.GetComponent()->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block;});
   if(bFloor && bClear && NPC->GetCharacterMovement()->IsMovingOnGround())++SupportedMovers;
   Test->TestTrue(FString::Printf(TEXT("Actual moved pedestrian%s remains supported and structurally clear: pose%s floor=%s/%s height%.2f normal%s clear%d grounded%d"),
    *NPC->GetNPCId_Implementation().ToString(),*P.ToString(),*GetPathNameSafe(H.GetActor()),*GetPathNameSafe(H.GetComponent()),H.ImpactPoint.Z,*H.ImpactNormal.ToString(),bClear,NPC->GetCharacterMovement()->IsMovingOnGround()),
    bFloor && bClear && NPC->GetCharacterMovement()->IsMovingOnGround());
  }
  Test->TestTrue(TEXT("At least two different actual pedestrians move80cm on the new promenade"),MovedIDs.Num()>=2 && SupportedMovers>=2);
  Test->AddInfo(FString::Printf(TEXT("Read-only C920 population observation: markers%d visible%d nativeAnimated%d distinctMovers%d supportedMovers%d observation%.2fs; no NPC/player/controller changes"),
   PromenadeMarkers.Num(),Visible,Animated,MovedIDs.Num(),SupportedMovers,W->GetTimeSeconds()-PopulationAt));
  return true;
 }
 void ValidateParking(UWorld* W)
 {
  const FName EstateID(TEXT("Property.Mainland.RivieraEstate916"));
  const auto SlotPose=[](int32 I){return FVector(12460+340*I,4150,40);};
  auto* PC=W->GetFirstPlayerController();auto* Player=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  if(!Test->TestNotNull(TEXT("Parking geometry uses the actual normal player capsule"),Player))return;
  UPropertyComponent* Estate=nullptr;int32 Estates=0;
  for(TActorIterator<AIslandLandResidence> I(W);I;++I)
   if(I->GetLevel()==W->PersistentLevel && I->Property && I->Property->GetPropertyID()==EstateID){Estate=I->Property;++Estates;}
  Test->TestEqual(TEXT("One exact live estate property supplies the court bays"),Estates,1);
  if(!Estate)return;
  Test->TestEqual(TEXT("Native estate property keeps all eight real garage slots"),Estate->GarageSlotsLocal.Num(),8);
  FNeonCityPropertyRecord Record;
  auto* Properties=W->GetGameInstance()->GetSubsystem<UPropertySubsystem>();
  const bool bRecord=Properties && Properties->GetProperty(EstateID,Record);
  Test->TestTrue(TEXT("Exact estate property remains registered in the existing property domain"),bRecord);
  if(bRecord)Test->TestEqual(TEXT("Live estate record has eight slots"),Record.GarageSlots.Num(),8);
  FCollisionQueryParams Ground(SCENE_QUERY_STAT(C920CourtNativeFloor),false,Player);
  const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
  auto Floor=[&](const FString& Label,const FVector& At)
  {
   FHitResult H;const bool bFloor=W->LineTraceSingleByObjectType(H,FVector(At.X,At.Y,190),FVector(At.X,At.Y,-160),StaticObjects,Ground)
    && H.ImpactNormal.Z>.94 && FMath::Abs(H.ImpactPoint.Z-40)<3;
   Test->TestTrue(Label+FString::Printf(TEXT(" static support: actor=%s component=%s at=%s normal=%s"),*GetPathNameSafe(H.GetActor()),*GetPathNameSafe(H.GetComponent()),*H.ImpactPoint.ToString(),*H.ImpactNormal.ToString()),bFloor);
   return bFloor;
  };
  TArray<UStaticMeshComponent*> Posts,Terminals;Posts.SetNumZeroed(8);Terminals.SetNumZeroed(8);
  for(int32 I=0;I<8;++I)
  {
   const FVector Expected=SlotPose(I);
   if(Estate->GarageSlotsLocal.IsValidIndex(I))
    Test->TestTrue(FString::Printf(TEXT("Exact native estate slot%d world pose: %s"),I,*Estate->GetGarageLocation(I).ToString()),Estate->GetGarageLocation(I).Equals(Expected,1));
   if(bRecord && Record.GarageSlots.IsValidIndex(I))
    Test->TestTrue(FString::Printf(TEXT("Registered estate slot%d agrees with native authoring"),I),Record.GarageSlots[I].SlotIndex==I && Record.GarageSlots[I].Location.Equals(Expected,1));
   Floor(FString::Printf(TEXT("Estate slot%d"),I),Expected);
   ANeonCityGarageMarker* Marker=nullptr;int32 MarkerCount=0;
   for(TActorIterator<ANeonCityGarageMarker> M(W);M;++M)
    if(M->GetLevel()==W->PersistentLevel && M->PropertyID==EstateID && M->SlotIndex==I){Marker=*M;++MarkerCount;}
   Test->TestEqual(FString::Printf(TEXT("Exact estate slot%d has one physical marker"),I),MarkerCount,1);
   if(!Marker)continue;
   Test->TestTrue(FString::Printf(TEXT("Estate marker%d is beside its real bay"),I),Marker->GetActorLocation().Equals(Expected+FVector(120,310,0),1));
   TInlineComponentArray<UStaticMeshComponent*> Components(Marker);
   for(auto* C:Components)if(C->GetFName()==TEXT("Post"))Posts[I]=C;
   const FVector TerminalPose=Marker->GetActorLocation()+FVector(0,7.5,65);int32 TerminalCount=0;
   for(TActorIterator<AStaticMeshActor> A(W);A;++A)
   {
    auto* C=A->GetStaticMeshComponent();UStaticMesh* Mesh=C->GetStaticMesh();
    if(A->GetLevel()==W->PersistentLevel && Mesh && Mesh->GetFName()==TEXT("SM_C920_LightingPanel") && A->GetActorLocation().Equals(TerminalPose,1))
    {Terminals[I]=C;++TerminalCount;}
   }
   Test->TestEqual(FString::Printf(TEXT("Estate marker%d has one correctly paired native terminal"),I),TerminalCount,1);
  }

  // Existing adoption evidence identifies the seven retained cars; presentation
  // replacement rows intentionally omit the two already using hero profiles.
  FString Text;TSharedPtr<FJsonObject> Adoption;const TArray<TSharedPtr<FJsonValue>>* Fleet=nullptr;
  const bool bFleet=FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/MainlandC920/Validation/adoption920.json")))
   && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Adoption) && Adoption
   && Adoption->TryGetArrayField(TEXT("fleet"),Fleet) && Fleet && Fleet->Num()==7;
  if(!Test->TestTrue(TEXT("Immutable actual adoption fleet contains exactly seven retained identities"),bFleet))return;
  TArray<AIslandSupercar*> Cars;Cars.SetNumZeroed(7);TArray<FBox> Bodies,PawnBodies;Bodies.Init(FBox(ForceInit),7);PawnBodies.Init(FBox(ForceInit),7);TArray<TArray<FBox>> QueryBodies;QueryBodies.SetNum(7);TSet<FGuid> IDs;
  int32 CourtCars=0;
  for(TActorIterator<AIslandSupercar> C(W);C;++C)
   if(C->GetLevel()==W->PersistentLevel && C->GetActorLocation().X>=12100 && C->GetActorLocation().X<=15100 && C->GetActorLocation().Y>=3800 && C->GetActorLocation().Y<=6700)++CourtCars;
  Test->TestEqual(TEXT("Exactly seven real hero cars occupy the court; no fabricated eighth car"),CourtCars,7);
  for(int32 I=0;I<7;++I)
  {
   const auto Row=(*Fleet)[I].IsValid() && (*Fleet)[I]->Type==EJson::Object?(*Fleet)[I]->AsObject():nullptr;FString IDText,Profile;FGuid ID;
   const bool bIdentity=Row && Row->TryGetStringField(TEXT("id"),IDText) && FGuid::Parse(IDText,ID) && ID.IsValid()
    && Row->TryGetStringField(TEXT("profile"),Profile) && !IDs.Contains(ID);
   if(!Test->TestTrue(FString::Printf(TEXT("Retained fleet row%d has a unique exact identity/profile"),I),bIdentity))continue;
   IDs.Add(ID);int32 Found=0;
   for(TActorIterator<AIslandSupercar> C(W);C;++C)if(C->GetLevel()==W->PersistentLevel && C->VehicleID==ID){Cars[I]=*C;++Found;}
   Test->TestEqual(FString::Printf(TEXT("Retained car%d GUID%s has one original live hero presentation"),I,*IDText),Found,1);
   auto* Car=Cars[I];if(!Car)continue;
   Test->TestTrue(FString::Printf(TEXT("Retained car%d uses its authored bay and north-facing pose"),I),FVector::Dist2D(Car->GetActorLocation(),SlotPose(I))<75 &&
    Car->GetActorLocation().Z>40 && Car->GetActorLocation().Z<220 && FMath::Abs(FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,90))<5);
   Test->TestTrue(FString::Printf(TEXT("Retained car%d profile remains%s"),I,*Profile),Car->GetVehicleProfile() && Car->GetVehicleProfile()->GetFName()==FName(*Profile));
   FBodyInstance* Body=Car->GetMesh()->GetBodyInstance();
   const FBox Bounds=Body && Body->IsValidBodyInstance()?Body->GetBodyBounds():FBox(ForceInit);
   if(!Test->TestTrue(FString::Printf(TEXT("Retained car%d has actual live native chassis physics bounds"),I),Body && Body->IsValidBodyInstance() && Bounds.IsValid && !Bounds.Min.ContainsNaN() && !Bounds.Max.ContainsNaN()))continue;
   Bodies[I]=Bounds;
   Test->AddInfo(FString::Printf(TEXT("C920 court car%d actor=%s GUID%s pose={%s} native_physics_bounds_min=%s max=%s"),I,*Car->GetPathName(),*IDText,*Car->GetActorTransform().ToString(),*Bounds.Min.ToString(),*Bounds.Max.ToString()));
   auto* Mesh=Car->GetMesh();
   Test->AddInfo(FString::Printf(TEXT("C920 car%d native query mesh=%s rig=%s physics_asset=%s skeletal_render_bounds_min=%s max=%s per_poly=%d component_pawn=%d component_collision=%d bodies=%d"),
    I,*Mesh->GetPathName(),*GetPathNameSafe(Mesh->GetSkeletalMeshAsset()),*GetPathNameSafe(Mesh->GetPhysicsAsset()),*Mesh->Bounds.GetBox().Min.ToString(),*Mesh->Bounds.GetBox().Max.ToString(),
    Mesh->bEnablePerPolyCollision,static_cast<int32>(Mesh->GetCollisionResponseToChannel(ECC_Pawn)),static_cast<int32>(Mesh->GetCollisionEnabled()),Mesh->Bodies.Num()));
   for(int32 B=0;B<Mesh->Bodies.Num();++B)
   {
    auto* Native=Mesh->Bodies[B];if(!Native || !Native->IsValidBodyInstance())continue;
    const auto* Setup=Native->GetBodySetup();const FBox BB=Native->GetBodyBounds();
    // Component-level native capsule sweeps hit retained wheel query shapes,
    // even when a wheel BodyInstance's own Pawn response reports Ignore.
    // The real AccessFixes1 hit names Phys_Wheel_FR; never discard that shape.
    const bool bPawnQuery=BB.IsValid && CollisionEnabledHasQuery(Native->GetCollisionEnabled()) && Mesh->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block;
    if(bPawnQuery){PawnBodies[I]+=BB;QueryBodies[I].Add(BB);}
    Test->AddInfo(FString::Printf(TEXT("C920 car%d native_body%d bone=%s setup=%s query_pawn=%d enabled=%d pawn_response=%d world_min=%s max=%s pose={%s} boxes%d spheres%d capsules%d convex%d"),
     I,B,Setup?*Setup->BoneName.ToString():TEXT("none"),*GetPathNameSafe(Setup),bPawnQuery,static_cast<int32>(Native->GetCollisionEnabled()),static_cast<int32>(Native->GetResponseToChannel(ECC_Pawn)),
     *BB.Min.ToString(),*BB.Max.ToString(),*Native->GetUnrealWorldTransform().ToString(),Setup?Setup->AggGeom.BoxElems.Num():0,Setup?Setup->AggGeom.SphereElems.Num():0,Setup?Setup->AggGeom.SphylElems.Num():0,Setup?Setup->AggGeom.ConvexElems.Num():0));
    if(Setup)for(int32 S=0;S<Setup->AggGeom.BoxElems.Num();++S)
    {
     const auto& Box=Setup->AggGeom.BoxElems[S];
     Test->AddInfo(FString::Printf(TEXT("C920 car%d native_body%d box%d local_center=%s dimensions=%s rotation=%s shape_collision=%d"),
      I,B,S,*Box.Center.ToString(),*FVector(Box.X,Box.Y,Box.Z).ToString(),*Box.Rotation.ToString(),static_cast<int32>(Box.GetCollisionEnabled())));
    }
    if(Setup)for(int32 S=0;S<Setup->AggGeom.SphereElems.Num();++S)
    {
     const auto& Sphere=Setup->AggGeom.SphereElems[S];
     Test->AddInfo(FString::Printf(TEXT("C920 car%d native_body%d sphere%d local_center=%s radius=%.3f shape_collision=%d"),
      I,B,S,*Sphere.Center.ToString(),Sphere.Radius,static_cast<int32>(Sphere.GetCollisionEnabled())));
    }
   }
   Test->TestTrue(FString::Printf(TEXT("Car%d has finite actual Pawn-blocking native body bounds"),I),PawnBodies[I].IsValid && !PawnBodies[I].Min.ContainsNaN() && !PawnBodies[I].Max.ContainsNaN());
   for(const FVector P:{Bounds.GetCenter(),FVector(Bounds.Min.X,Bounds.Min.Y,40),FVector(Bounds.Min.X,Bounds.Max.Y,40),FVector(Bounds.Max.X,Bounds.Min.Y,40),FVector(Bounds.Max.X,Bounds.Max.Y,40)})
    Floor(FString::Printf(TEXT("Actual car%d chassis footprint"),I),P);
   FBox Chassis=Bounds;Chassis.Min.Z=FMath::Max(Chassis.Min.Z,43.); // Exclude its supporting floor, not the floor actor's walls.
   FCollisionQueryParams Q(SCENE_QUERY_STAT(C920CourtChassisClearance),false,Car);Q.AddIgnoredActor(Player);
   TArray<FOverlapResult> Hits;
   if(Chassis.Min.Z<Chassis.Max.Z)W->OverlapMultiByChannel(Hits,Chassis.GetCenter(),FQuat::Identity,ECC_Vehicle,FCollisionShape::MakeBox(Chassis.GetExtent()),Q);
   int32 Blockers=0;
   for(const auto& H:Hits)if(H.bBlockingHit){++Blockers;Test->AddInfo(FString::Printf(TEXT("C920 car%d chassis obstruction actor=%s component=%s"),I,*GetPathNameSafe(H.GetActor()),*GetPathNameSafe(H.GetComponent())));}
   Test->TestTrue(FString::Printf(TEXT("Actual car%d chassis has supported nonzero clearance above floor"),I),Chassis.Min.Z<Chassis.Max.Z && Blockers==0);
  }
  for(int32 Slot=0;Slot<8;++Slot)for(int32 I=0;I<7;++I)if(Bodies[I].IsValid)
  {
   if(Posts[Slot])Test->TestFalse(FString::Printf(TEXT("Post%d native bounds do not intersect car%d chassis"),Slot,I),Posts[Slot]->Bounds.GetBox().Intersect(Bodies[I]));
   if(Terminals[Slot])Test->TestFalse(FString::Printf(TEXT("Terminal%d native bounds do not intersect car%d chassis"),Slot,I),Terminals[Slot]->Bounds.GetBox().Intersect(Bodies[I]));
  }
  const float Radius=Player->GetSimpleCollisionRadius(),Half=Player->GetSimpleCollisionHalfHeight();
  const auto Capsule=FCollisionShape::MakeCapsule(Radius,Half);
  FCollisionQueryParams WalkQuery(SCENE_QUERY_STAT(C920CourtDoorCirculation),false,Player); // Every car and post remains an obstruction.
  for(int32 I=0;I<7;++I)
  {
   const int32 Left=I<6?I:I-1,Right=Left+1;
   if(!Cars[I] || !Cars[Left] || !Cars[Right] || !PawnBodies[Left].IsValid || !PawnBodies[Right].IsValid)continue;
   const FVector From=(Cars[Left]->GetActorLocation()+Cars[Right]->GetActorLocation())*.5+FVector(0,130,0);FVector Stand;
   const bool bApproach=Phase205cVehicleApproach(W,Player,Cars[I],From,Stand);
   if(!Test->TestTrue(FString::Printf(TEXT("Car%d has a normal supported F approach between neighboring cars"),I),bApproach))
   {
    FVector Probe=Cars[I]->GetActorLocation()+(From-Cars[I]->GetActorLocation()).GetSafeNormal2D()*215;Probe.Z=40+Half+3;
    TArray<FOverlapResult> Hits;W->OverlapMultiByChannel(Hits,Probe,FQuat::Identity,ECC_Pawn,Capsule,WalkQuery);
    for(const auto& H:Hits)if(H.bBlockingHit)Test->AddInfo(FString::Printf(TEXT("C920 car%d first normal approach candidate%s blocked by%s/%s"),I,*Probe.ToString(),*GetPathNameSafe(H.GetActor()),*GetPathNameSafe(H.GetComponent())));
    continue;
   }
   // The actual approach lies alongside the front wheels. Rear-wheel bounds
   // at another Y cannot replace that local cross-section of the real gap.
   double LeftEdge=-TNumericLimits<double>::Max(),RightEdge=TNumericLimits<double>::Max();bool LeftSection=false,RightSection=false;
   for(const FBox& B:QueryBodies[Left])if(B.Min.Y-Radius-3<=Stand.Y && B.Max.Y+Radius+3>=Stand.Y && B.Min.Z<=Stand.Z+Half && B.Max.Z>=Stand.Z-Half)
   {LeftEdge=FMath::Max(LeftEdge,B.Max.X);LeftSection=true;}
   for(const FBox& B:QueryBodies[Right])if(B.Min.Y-Radius-3<=Stand.Y && B.Max.Y+Radius+3>=Stand.Y && B.Min.Z<=Stand.Z+Half && B.Max.Z>=Stand.Z-Half)
   {RightEdge=FMath::Min(RightEdge,B.Min.X);RightSection=true;}
   if(!Test->TestTrue(FString::Printf(TEXT("Car%d native approach cross-section contains both real neighboring bodies"),I),LeftSection && RightSection))continue;
   const double Low=LeftEdge+Radius+3,High=RightEdge-Radius-3;
   Test->TestTrue(FString::Printf(TEXT("Car%d approach fits its real neighbor gap: stand%s allowedX[%.1f,%.1f]"),I,*Stand.ToString(),Low,High),Low<=High && Stand.X>=Low && Stand.X<=High);
   if(Low>High)continue;
   double Front=FMath::Max(PawnBodies[Left].Max.Y,PawnBodies[Right].Max.Y),CorridorLow=Bodies[Left].Max.X+Radius+3;
   const double CorridorHigh=Bodies[Right].Min.X-Radius-3;
   if(Posts[Left])Front=FMath::Max(Front,Posts[Left]->Bounds.GetBox().Max.Y);
   if(Terminals[Left])Front=FMath::Max(Front,Terminals[Left]->Bounds.GetBox().Max.Y);
   for(auto* Fixture:{Posts[Left],Terminals[Left]})if(Fixture)CorridorLow=FMath::Max(CorridorLow,Fixture->Bounds.GetBox().Max.X+Radius+3);
   Test->AddInfo(FString::Printf(TEXT("C920 car%d native wheel-section gap %.3fcm; capsule radius%.3f half%.3f; door interval[%.3f,%.3f] forward fixture interval[%.3f,%.3f] pairedPost=%s pairedTerminal=%s"),
    I,RightEdge-LeftEdge,Radius,Half,Low,High,CorridorLow,CorridorHigh,*GetPathNameSafe(Posts[Left]),*GetPathNameSafe(Terminals[Left])));
   if(!Test->TestTrue(FString::Printf(TEXT("Car%d paired fixtures leave a real nonzero forward aisle corridor"),I),CorridorLow<CorridorHigh))continue;
   const double DoorX=(Low+High)*.5,CorridorX=(CorridorLow+CorridorHigh)*.5;
   // Finish passing every native shape that occupies the later post-detour
   // X before moving sideways. Applying the post's X at the door pushed the
   // capsule into the neighboring front wheel, despite a clear normal stand.
   double TurnY=Stand.Y;
   for(int32 Neighbor:{Left,Right})for(const FBox& B:QueryBodies[Neighbor])
    if(B.Min.X-Radius-3<=CorridorX && B.Max.X+Radius+3>=CorridorX && B.Min.Z<=Stand.Z+Half && B.Max.Z>=Stand.Z-Half)
     TurnY=FMath::Max(TurnY,B.Max.Y+Radius+3);
   double FixtureStart=TNumericLimits<double>::Max();
   for(auto* Fixture:{Posts[Left],Terminals[Left]})if(Fixture)FixtureStart=FMath::Min(FixtureStart,Fixture->Bounds.GetBox().Min.Y);
   if(!Test->TestTrue(FString::Printf(TEXT("Car%d can pass its real wheels before the physical post zone: turnY%.3f postMinY%.3f"),I,TurnY,FixtureStart),TurnY+Radius+3<FixtureStart))continue;
   const FVector Door(DoorX,Stand.Y,Stand.Z),WheelClear(DoorX,TurnY,Stand.Z),Lane(CorridorX,TurnY,Stand.Z),Aisle(CorridorX,Front+Radius+20,Stand.Z);
   const TArray<FVector> Circulation={Stand,Door,WheelClear,Lane,Aisle};TArray<FHitResult> SweepHits;SweepHits.SetNum(Circulation.Num()-1);
   bool bBlocked=false;FHitResult Hit;
   for(int32 P=0;P<Circulation.Num();++P)
   {
    const bool Overlap=W->OverlapBlockingTestByChannel(Circulation[P],FQuat::Identity,ECC_Pawn,Capsule,WalkQuery);
    const bool Sweep=P>0 && W->SweepSingleByChannel(SweepHits[P-1],Circulation[P-1],Circulation[P],FQuat::Identity,ECC_Pawn,Capsule,WalkQuery);
    if(Sweep && !Hit.bBlockingHit)Hit=SweepHits[P-1];bBlocked|=Overlap || Sweep;
    Test->AddInfo(FString::Printf(TEXT("C920 car%d actual circulation point%d=%s overlap%d incoming_sweep%d"),I,P,*Circulation[P].ToString(),Overlap,Sweep));
    Floor(FString::Printf(TEXT("Car%d circulation point%d"),I,P),Circulation[P]);
   }
   Test->TestTrue(FString::Printf(TEXT("Car%d real door/wheel-clear/post-detour/front-aisle full-capsule circulation blocker=%s/%s"),I,*GetPathNameSafe(Hit.GetActor()),*GetPathNameSafe(Hit.GetComponent())),!bBlocked);
   if(bBlocked)
   {
    for(const FHitResult& SweepHit:SweepHits)if(SweepHit.bBlockingHit)
    {
     const FHitResult* H=&SweepHit;
     Test->AddInfo(FString::Printf(TEXT("C920 car%d actual full-capsule sweep from%s to%s time%.6f distance%.3f initial_overlap%d penetration%.3f location%s impact%s normal%s impact_normal%s actor=%s component=%s bone=%s item%d face%d"),
      I,*H->TraceStart.ToString(),*H->TraceEnd.ToString(),H->Time,H->Distance,H->bStartPenetrating,H->PenetrationDepth,*H->Location.ToString(),*H->ImpactPoint.ToString(),*H->Normal.ToString(),*H->ImpactNormal.ToString(),
      *GetPathNameSafe(H->GetActor()),*GetPathNameSafe(H->GetComponent()),*H->BoneName.ToString(),H->Item,H->FaceIndex));
    }
    for(const FVector Point:Circulation)
    {
     TArray<FOverlapResult> Hits;W->OverlapMultiByChannel(Hits,Point,FQuat::Identity,ECC_Pawn,Capsule,WalkQuery);
     for(const auto& H:Hits)if(H.bBlockingHit)Test->AddInfo(FString::Printf(TEXT("C920 car%d circulation endpoint%s blocked by%s/%s component_bounds_min%s max%s"),I,*Point.ToString(),*GetPathNameSafe(H.GetActor()),*GetPathNameSafe(H.GetComponent()),
      H.GetComponent()?*H.GetComponent()->Bounds.GetBox().Min.ToString():TEXT("none"),H.GetComponent()?*H.GetComponent()->Bounds.GetBox().Max.ToString():TEXT("none")));
    }
    for(int32 Neighbor:{Left,Right})for(auto* B:Cars[Neighbor]->GetMesh()->Bodies)if(B && B->IsValidBodyInstance())
    {
     FVector Surface=FVector::ZeroVector;const float Distance=B->GetDistanceToBody(Lane,Surface);
     Test->AddInfo(FString::Printf(TEXT("C920 car%d corridor Lane to car%d body%s closest native surface%s point_distance%.3fcm"),I,Neighbor,*B->GetBodyDebugName(),*Surface.ToString(),Distance));
    }
   }
   for(auto* Fixture:{Posts[Left],Terminals[Left]})if(Fixture)
   {
    const FBox Expanded=Fixture->Bounds.GetBox().ExpandBy(FVector(Radius,Radius,Half));
    bool Intersects=false;for(int32 P=1;P<Circulation.Num();++P)
     Intersects|=FMath::LineBoxIntersection(Expanded,Circulation[P-1],Circulation[P],Circulation[P]-Circulation[P-1]);
    Test->TestFalse(FString::Printf(TEXT("Car%d circulation avoids paired fixture native bounds%s"),I,*Fixture->GetPathName()),
     Intersects);
   }
  }
 }
public:
 explicit FSupport(FAutomationTestBase* T,bool InCourtOnly=false):Test(T),bCourtOnly(InCourtOnly){}
 bool Update() override
 {
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<8)return false;
  if(bSurveyed)return ObservePopulation(W);
  if(!Test->TestNotNull(TEXT("Actual C mainland authored placement"),ANeonCityMainlandSite::Find(W)))return true;
  if(bCourtOnly){ValidateParking(W);return true;}
  FCollisionQueryParams Query(SCENE_QUERY_STAT(MainlandC920StreetSupport),false);
  for(TActorIterator<APawn> I(W);I;++I)Query.AddIgnoredActor(*I);
  int32 Roads=0,Probes=0,Clearance=0,Failures=0;
  for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)
  {
   if(!I->RoadID.ToString().StartsWith(TEXT("C920.")))continue;
   ++Roads;auto* S=I->GetRoadSpline();
   for(float D=0;D<=S->GetSplineLength();D+=250)
   {
    const FVector P=S->GetLocationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);
    const FVector N=S->GetRightVectorAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);
    for(float Offset:{-245.f,0.f,245.f})
    {
     const FVector A=P+N*Offset;FHitResult H;++Probes;
     const bool Hit=W->LineTraceSingleByChannel(H,A+FVector(0,0,200),A-FVector(0,0,250),ECC_WorldStatic,Query);
     if(!Hit || H.ImpactNormal.Z<.94f || FMath::Abs(H.ImpactPoint.Z-40)>3)
     {if(++Failures<10)Test->AddError(FString::Printf(TEXT("Street support %s %.1fm lane%.0f: %s z%.2f"),*I->RoadID.ToString(),D/100,Offset,*GetNameSafe(H.GetActor()),H.ImpactPoint.Z));}
    }
    if(D+250<=S->GetSplineLength())for(float Offset:{-170.f,170.f})
    {
     const FVector A=P+N*Offset+FVector(0,0,80);
     const FVector B=S->GetLocationAtDistanceAlongSpline(D+250,ESplineCoordinateSpace::World)+N*Offset+FVector(0,0,80);
     FHitResult H;++Clearance;
     if(W->SweepSingleByChannel(H,A,B,(B-A).Rotation().Quaternion(),ECC_WorldStatic,FCollisionShape::MakeBox(FVector(220,100,55)),Query))
     {if(++Failures<10)Test->AddError(FString::Printf(TEXT("Real chassis path blocked by %s on %s"),*GetNameSafe(H.GetActor()),*I->RoadID.ToString()));}
    }
   }
  }
  Test->TestEqual(TEXT("Exactly four new mainland road guides"),Roads,4);
  Test->TestEqual(TEXT("No road support or structural clearance failure"),Failures,0);
  Test->AddInfo(FString::Printf(TEXT("C920 actual collision probes: %d support, %d continuous chassis sweeps"),Probes,Clearance));
  int32 Markers=0;TSet<FString> MarkerSlots;
  const FName StarterID(TEXT("NeonCity.Property.StarterApartment")),EstateID(TEXT("Property.Mainland.RivieraEstate916"));
  for(TActorIterator<ANeonCityGarageMarker> I(W);I;++I)
  {
   if(I->PropertyID!=StarterID && I->PropertyID!=EstateID)continue;
   ++Markers;
   const FString Slot=FString::Printf(TEXT("%s:%d"),*I->PropertyID.ToString(),I->SlotIndex);
   Test->TestFalse(FString::Printf(TEXT("Unique authored garage marker %s"),*Slot),MarkerSlots.Contains(Slot));MarkerSlots.Add(Slot);
   Test->TestTrue(FString::Printf(TEXT("Exact authored garage slot %s"),*Slot),I->PropertyID==StarterID?I->SlotIndex==0:I->SlotIndex>=0 && I->SlotIndex<8);
   UStaticMeshComponent* Post=nullptr;
   TInlineComponentArray<UStaticMeshComponent*> Components(*I);
   for(auto* C:Components)if(C->GetFName()==TEXT("Post")){Post=C;break;}
   if(!Test->TestNotNull(FString::Printf(TEXT("Original interaction Post component %s"),*Slot),Post))continue;
   UStaticMesh* Mesh=Post->GetStaticMesh();FString Overrides,Resolved;
   for(auto& Material:Post->OverrideMaterials){if(!Overrides.IsEmpty())Overrides+=TEXT(" | ");Overrides+=GetPathNameSafe(Material.Get());}
   for(int32 M=0;M<Post->GetNumMaterials();++M){if(!Resolved.IsEmpty())Resolved+=TEXT(" | ");Resolved+=GetPathNameSafe(Post->GetMaterial(M));}
   const FVector NativeDimensions=Mesh?Mesh->GetBounds().BoxExtent*2:FVector::ZeroVector;
   Test->AddInfo(FString::Printf(TEXT("C920 garage marker slot=%s actor=%s component=%s mesh=%s native_dimensions_cm=%s world_dimensions_cm=%s relative={%s} world={%s} overrides=[%s] resolved=[%s] collision_profile=%s enabled=%d visibility_response=%d vehicle_response=%d visible=%d actor_hidden=%d registered=%d"),
    *Slot,*I->GetPathName(),*Post->GetPathName(),*GetPathNameSafe(Mesh),*NativeDimensions.ToString(),*(Post->Bounds.BoxExtent*2).ToString(),
    *Post->GetRelativeTransform().ToString(),*Post->GetComponentTransform().ToString(),*Overrides,*Resolved,*Post->GetCollisionProfileName().ToString(),
    static_cast<int32>(Post->GetCollisionEnabled()),static_cast<int32>(Post->GetCollisionResponseToChannel(ECC_Visibility)),
    static_cast<int32>(Post->GetCollisionResponseToChannel(ECC_Vehicle)),Post->IsVisible(),I->IsHidden(),Post->IsRegistered()));
   Test->TestTrue(FString::Printf(TEXT("Native bronze bollard replaces prototype %s"),*Slot),Mesh && Mesh->GetFName()==TEXT("SM_C920_MooringBollard"));
   Test->TestEqual(FString::Printf(TEXT("No teal per-instance material override %s"),*Slot),Post->GetNumOverrideMaterials(),0);
   Test->TestTrue(FString::Printf(TEXT("Native bollard uses its fitted local pose %s"),*Slot),Post->GetRelativeTransform().Equals(FTransform::Identity,.01));
   Test->TestTrue(FString::Printf(TEXT("Garage post is rendered %s"),*Slot),Post->IsRegistered() && Post->IsVisible() && !I->IsHidden());
   Test->TestTrue(FString::Printf(TEXT("Garage post retains query collision %s"),*Slot),Post->IsQueryCollisionEnabled());
   Test->TestEqual(FString::Printf(TEXT("Garage post blocks the real interaction trace %s"),*Slot),Post->GetCollisionResponseToChannel(ECC_Visibility),ECR_Block);
   FHitResult Hit;const FVector Center=Post->Bounds.Origin;
   const bool bHit=W->LineTraceSingleByChannel(Hit,Center+Post->GetForwardVector()*100,Center,ECC_Visibility,Query);
   Test->TestTrue(FString::Printf(TEXT("Actual post receives Visibility hit %s; hit=%s component=%s"),*Slot,*GetPathNameSafe(Hit.GetActor()),*GetPathNameSafe(Hit.GetComponent())),bHit && Hit.GetComponent()==Post);
  }
  Test->TestEqual(TEXT("Exactly nine actual mainland garage interaction posts"),Markers,9);
  Test->TestEqual(TEXT("All nine exact authored property slots have a unique marker"),MarkerSlots.Num(),9);
  ValidateParking(W);
  for(TActorIterator<AActor> I(W);I;++I)
  {
   TInlineComponentArray<UStaticMeshComponent*> Components(*I);
   for(auto* C:Components)
   {
    UStaticMesh* Mesh=C->GetStaticMesh();if(!Mesh)continue;
    const FString Name=Mesh->GetName();
    if(Name!=TEXT("SM_C920_Road") && Name!=TEXT("SM_C920_GardenPoolWater"))continue;
    FString Native,Effective;
    for(const auto& M:Mesh->GetStaticMaterials()){if(!Native.IsEmpty())Native+=TEXT(" | ");Native+=GetPathNameSafe(M.MaterialInterface.Get());}
    for(int32 M=0;M<C->GetNumMaterials();++M){if(!Effective.IsEmpty())Effective+=TEXT(" | ");Effective+=GetPathNameSafe(C->GetMaterial(M));}
    Test->AddInfo(FString::Printf(TEXT("C920 rendered surface actor=%s component=%s mesh=%s static_materials=[%s] effective_materials=[%s] nanite_enabled=%d visible=%d actor_hidden=%d world={%s}"),
     *I->GetPathName(),*C->GetPathName(),*Mesh->GetPathName(),*Native,*Effective,Mesh->IsNaniteEnabled(),C->IsVisible(),I->IsHidden(),*C->GetComponentTransform().ToString()));
   }
  }
  // Only the relocated native promenade markers participate. Island/offshore
  // population and dormant actors cannot satisfy this active mainland check.
  const auto* Capsule=GetDefault<ANeonCityNPCCharacter>()->GetCapsuleComponent();
  const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetUnscaledCapsuleRadius(),Capsule->GetUnscaledCapsuleHalfHeight()-2);
  const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
  int32 RouteProbes=0,RouteBlockers=0;
  for(TActorIterator<APedestrianSpawnPoint> I(W);I;++I)
  {
   const FVector P=I->GetActorLocation();
   if(I->GetLevel()!=W->PersistentLevel || !I->bDistanceManaged || P.X< -16700 || P.X>16700 || P.Y< -3200 || P.Y> -1600)continue;
   PromenadeMarkers.Add(*I);
   Test->TestTrue(FString::Printf(TEXT("Relocated promenade marker%s keeps a native route"),*I->PedestrianId.ToString()),I->LocalRoute.Num()>=2);
   for(int32 J=1;J<I->LocalRoute.Num();++J)
   {
    const FVector A=I->GetActorTransform().TransformPosition(I->LocalRoute[J-1]),B=I->GetActorTransform().TransformPosition(I->LocalRoute[J]);
    FHitResult Block;
    if(W->SweepSingleByObjectType(Block,A,B,FQuat::Identity,StaticObjects,Shape,Query))
    {++RouteBlockers;Test->AddError(FString::Printf(TEXT("Native promenade route%s blocked by%s/%s at%s"),*I->PedestrianId.ToString(),*GetPathNameSafe(Block.GetActor()),*GetPathNameSafe(Block.GetComponent()),*Block.ImpactPoint.ToString()));}
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/250));
    for(int32 K=0;K<=Steps;++K)
    {
     const FVector At=FMath::Lerp(A,B,double(K)/Steps);FHitResult H;++RouteProbes;
     const bool bFloor=W->LineTraceSingleByObjectType(H,At+FVector(0,0,100),At-FVector(0,0,200),StaticObjects,Query)
      && H.ImpactNormal.Z>.94 && FMath::Abs(H.ImpactPoint.Z-55)<3;
     if(!bFloor){++RouteBlockers;Test->AddError(FString::Printf(TEXT("Native promenade route%s lacks finished support at%s actor%s z%.2f"),*I->PedestrianId.ToString(),*At.ToString(),*GetPathNameSafe(H.GetActor()),H.ImpactPoint.Z));}
    }
   }
  }
  Test->TestTrue(TEXT("Actual relocated promenade markers exist"),PromenadeMarkers.Num()>=2);
  Test->TestEqual(TEXT("Relocated native promenade routes have static support and capsule clearance"),RouteBlockers,0);
  Test->AddInfo(FString::Printf(TEXT("C920 native promenade route survey: %d actual markers, %d support probes"),PromenadeMarkers.Num(),RouteProbes));
  bSurveyed=true;PopulationAt=W->GetTimeSeconds();PopulationWallAt=FPlatformTime::Seconds();
  return ObservePopulation(W);
 }
};

class FWalk : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int32 Step=0;double Start=0,At=0;
 FNeonCityLandFrameMetrics Metrics;
 bool bHitchDiagnostic=false,bDiagnosticReached=false;
 FVector DiagnosticOrigin=FVector::ZeroVector,DiagnosticLast=FVector::ZeroVector;
 double DiagnosticTraceStart=0,DiagnosticPreviousSample=0,DiagnosticTravelCm=0,DiagnosticPeakIntervalMs=0;
 int32 DiagnosticIntervalsOver50ms=0,DiagnosticSamples=0;
 bool End(ACharacter* Character,const FString& Error)
 {
  if(Character)Character->GetCharacterMovement()->StopMovementImmediately();
  if(!Error.IsEmpty())Test->AddError(Error);
  const bool Written=Metrics.Flush();
  Test->TestTrue(TEXT("Continuous walk frame telemetry retained"),Written);
  if(bHitchDiagnostic)
  {
   const double Elapsed=FPlatformTime::Seconds()-Start;
   TRACE_BOOKMARK(TEXT("C920WalkStartHitchRepro End elapsed=%.6f reached=%d over50=%d peak_interval_ms=%.4f"),
    Elapsed,bDiagnosticReached?1:0,DiagnosticIntervalsOver50ms,DiagnosticPeakIntervalMs);
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   Label=FPaths::MakeValidFileName(Label.IsEmpty()?TEXT("WalkStartHitchRepro"):Label,TEXT('_'));Label.ReplaceInline(TEXT(".."),TEXT("_"));
   const FString Directory=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC920/WalkStartHitch")/Label;
   IFileManager::Get().MakeDirectory(*Directory,true);
   auto Receipt=MakeShared<FJsonObject>();
   Receipt->SetBoolField(TEXT("diagnostic_collection_passed"),Error.IsEmpty() && Written && bDiagnosticReached && Elapsed>=8.);
   Receipt->SetBoolField(TEXT("full_continuous_walk_pass"),false);
   Receipt->SetStringField(TEXT("gameplay_acceptance"),TEXT("NOT CLAIMED: bounded trace diagnosis only; cached full ContinuousWalk is separate"));
   Receipt->SetStringField(TEXT("failure"),Error);Receipt->SetStringField(TEXT("run_label"),Label);
   Receipt->SetNumberField(TEXT("elapsed_seconds"),Elapsed);Receipt->SetNumberField(TEXT("sample_updates"),DiagnosticSamples);
   Receipt->SetStringField(TEXT("actual_player_start_cm"),DiagnosticOrigin.ToString());
   Receipt->SetStringField(TEXT("actual_player_finish_cm"),Character?Character->GetActorLocation().ToString():TEXT("absent"));
   Receipt->SetNumberField(TEXT("actual_ordered_player_travel_m"),DiagnosticTravelCm/100.);
   Receipt->SetNumberField(TEXT("observed_update_intervals_over_50ms"),DiagnosticIntervalsOver50ms);
   Receipt->SetNumberField(TEXT("peak_observed_update_interval_ms"),DiagnosticPeakIntervalMs);
   Receipt->SetBoolField(TEXT("clean_performance_sample"),false);
   Receipt->SetBoolField(TEXT("image_readback"),false);
   Receipt->SetStringField(TEXT("scope"),TEXT("8-second trace window from unchanged normal PlayerStart; normal +X movement to a 20m target, then supported stillness. No teleport/fixture, collision/movement-mode changes or full route repeat. Interval bookmarks are observations, not root-cause attribution."));
   FString Json;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Json));
   const FString File=Directory/TEXT("receipt.json");
   if(IFileManager::Get().FileExists(*File) || !FFileHelper::SaveStringToFile(Json,*File))
    Test->AddError(TEXT("Could not retain a new bounded walking diagnostic receipt without overwriting old evidence"));
   Test->AddInfo(TEXT("WalkStartHitchRepro collection only; no full-route gameplay or clean performance PASS is claimed."));
  }
  return true;
 }
public:
 explicit FWalk(FAutomationTestBase* T,bool InHitchDiagnostic=false):Test(T),
  Metrics(InHitchDiagnostic?TEXT("MainlandC920-WalkStartHitchRepro"):TEXT("MainlandC920-ContinuousWalk"),true),bHitchDiagnostic(InHitchDiagnostic){}
 bool Update()override
 {
  TRACE_CPUPROFILER_EVENT_SCOPE(C920WalkUpdate);
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<8)return false;
  if(bHitchDiagnostic)
  {
   const double SampleAt=FPlatformTime::Seconds();
   if(DiagnosticTraceStart==0)
   {DiagnosticTraceStart=SampleAt;TRACE_BOOKMARK(TEXT("C920WalkStartHitchRepro Begin game=%.6f"),W->GetTimeSeconds());}
   if(DiagnosticPreviousSample>0)
   {
    const double IntervalMs=(SampleAt-DiagnosticPreviousSample)*1000.;
    DiagnosticPeakIntervalMs=FMath::Max(DiagnosticPeakIntervalMs,IntervalMs);
    if(IntervalMs>50.)
    {
     ++DiagnosticIntervalsOver50ms;
     TRACE_BOOKMARK(TEXT("C920WalkStartHitchRepro IntervalOver50ms wall=%.6f game=%.6f interval_ms=%.4f"),
      SampleAt-DiagnosticTraceStart,W->GetTimeSeconds(),IntervalMs);
    }
   }
   DiagnosticPreviousSample=SampleAt;++DiagnosticSamples;
  }
  Metrics.Sample(W,bHitchDiagnostic?(bDiagnosticReached?TEXT("walk-start-diagnostic-supported-hold"):TEXT("walk-start-diagnostic-normal-moving")):
   FString::Printf(TEXT("walk-leg%02d%s"),Step,
   FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills"))?TEXT("-stills-enabled"):TEXT("")));
  auto* PC=W->GetFirstPlayerController();auto* C=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  if(!C)return End(nullptr,TEXT("Normal possessed player character required"));
  const double Now=FPlatformTime::Seconds();
  if(Start==0)
  {
   Start=At=Now;
   if(FVector::Dist2D(C->GetActorLocation(),FVector(-7000,-2400,0))>200)return End(C,TEXT("Authored PlayerStart must begin this continuous route; no teleport fallback"));
   if(bHitchDiagnostic)DiagnosticOrigin=DiagnosticLast=C->GetActorLocation();
  }
  if(bHitchDiagnostic)
  {
   TRACE_CPUPROFILER_EVENT_SCOPE(C920WalkStartDiagnosticControl);
   const FVector Position=C->GetActorLocation();
   if(Position.ContainsNaN() || Position.Z<100 || Position.Z>300 || !C->GetActorEnableCollision() ||
    !C->GetCharacterMovement()->IsMovingOnGround() || C->GetAttachParentActor())
    return End(C,TEXT("Bounded walking diagnostic lost normal grounded player/collision state"));
   DiagnosticTravelCm+=FVector::Dist2D(Position,DiagnosticLast);DiagnosticLast=Position;
   const FVector Goal=DiagnosticOrigin+FVector(2000.,0.,0.);
   if(!bDiagnosticReached && FVector::Dist2D(Position,Goal)<25.)
   {
    bDiagnosticReached=true;C->GetCharacterMovement()->StopMovementImmediately();
    TRACE_BOOKMARK(TEXT("C920WalkStartHitchRepro Reached20m elapsed=%.6f travel_m=%.4f"),Now-Start,DiagnosticTravelCm/100.);
   }
   if(Now-Start>=8.)return End(C,bDiagnosticReached?TEXT(""):TEXT("Bounded eight-second trace window did not reach the normal 20m walking target"));
   if(!bDiagnosticReached)C->AddMovementInput((Goal-Position).GetSafeNormal2D(),1);
   return false;
  }
  const FVector Goals[]={FVector(-3300,-2400,0),FVector(-3300,-1465,0),FVector(-3100,-1465,0),
   FVector(-3100,1700,0),FVector(-1475,1700,0),FVector(-1475,-2400,0),
   FVector(1500,-2400,0),FVector(1500,1700,0),FVector(5200,1700,0),FVector(5200,-2400,0),FVector(7000,-2400,0)};
  const FVector P=C->GetActorLocation();
  if(P.Z<100 || P.Z>300)return End(C,FString::Printf(TEXT("Walk route lost finished floor at step%d: %s"),Step,*P.ToString()));
  if(FVector::Dist2D(P,Goals[Step])<70)
  {
   C->GetCharacterMovement()->StopMovementImmediately();
#if WITH_EDITOR
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))
    Test->TestTrue(TEXT("Actual normal-player route frame"),FNeonCityVisualEvidence::CaptureRenderedJPEG(W,FString::Printf(TEXT("C920-Walk-%02d.jpg"),Step)));
#endif
   if(++Step==UE_ARRAY_COUNT(Goals)){Test->AddInfo(FString::Printf(TEXT("Continuous production-character route completed in %.1fs; no movement-mode or input architecture changes"),Now-Start));return End(C,TEXT(""));}
   At=Now;
  }
  if(Now-At>30)return End(C,FString::Printf(TEXT("Normal movement blocked at route step%d location%s"),Step,*P.ToString()));
  C->AddMovementInput((Goals[Step]-P).GetSafeNormal2D(),1);return false;
 }
};

static bool ReadStillViewIndices(FAutomationTestBase* Test,TArray<int32>& Indices,bool& bExplicitSubset)
{
 const TCHAR* Cursor=FCommandLine::Get();FString Argument,Selector;bExplicitSubset=false;
 while(FParse::Token(Cursor,Argument,false))
 {
  if(Argument!=TEXT("-C920ViewIndices") && !Argument.StartsWith(TEXT("-C920ViewIndices="),ESearchCase::CaseSensitive))continue;
  if(bExplicitSubset){Test->AddError(TEXT("C920ViewIndices may be supplied only once"));return false;}
  bExplicitSubset=true;
  if(!Argument.StartsWith(TEXT("-C920ViewIndices="),ESearchCase::CaseSensitive))
  {Test->AddError(TEXT("C920ViewIndices requires a nonempty comma-separated value"));return false;}
  Selector=Argument.Mid(17);
 }
 if(!bExplicitSubset){for(int32 I=0;I<19;++I)Indices.Add(I);return true;}
 TArray<FString> Parts;Selector.ParseIntoArray(Parts,TEXT(","),false);
 if(Selector.IsEmpty() || Parts.IsEmpty() || Parts.Num()>19)
 {Test->AddError(TEXT("C920ViewIndices requires 1..19 unique indices from 0 through 18"));return false;}
 TSet<int32> Seen;
 for(const FString& Part:Parts)
 {
  bool bDigits=!Part.IsEmpty() && Part.Len()<=2;
  for(const TCHAR C:Part)bDigits&=C>='0' && C<='9';
  const int32 Index=bDigits?FCString::Atoi(*Part):-1;
  if(!bDigits || Index<0 || Index>=19 || Seen.Contains(Index))
  {Test->AddError(FString::Printf(TEXT("Invalid or duplicate C920ViewIndices entry '%s'; only unique integer indices 0..18 are accepted"),*Part));return false;}
  Seen.Add(Index);Indices.Add(Index);
 }
 return true;
}

class FViews : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;bool bMetrics;ACameraActor* Camera=nullptr;ANeonCityDayNightCycle* Clock=nullptr;
 TWeakObjectPtr<AActor> OldView;float OldHour=12;bool OldCycle=true;int32 View=0;double At=0;
 FNeonCityLandFrameMetrics Metrics{TEXT("MainlandC920-Sample")};
 static constexpr int32 MetricsCount=11,StillCount=19;
 TArray<int32> StillIndices,MetricIndices;bool bExplicitSubset=false;int32 FailedStills=0;
 bool bReferenceMetrics=false,bRestored=false,bOldPhotographyHidden=false,bReferenceSampleBookmark=false;FTransform ReferenceMetricsPose;float ReferenceMetricsFOV=65;
 TWeakObjectPtr<APlayerController> MetricsController;
 TArray<TSharedPtr<FJsonValue>> CapturedViews;
 void RestoreView()
 {
  if(bRestored)return;bRestored=true;
  if(IsValid(Camera))
  {
   if(MetricsController.IsValid() && OldView.IsValid())MetricsController->SetViewTarget(OldView.Get());
   if(bReferenceMetrics)if(auto* PC=Cast<ANeonCityPlayerController>(MetricsController.Get()))PC->SetPhotographyHUDHidden(bOldPhotographyHidden);
   Camera->Destroy();Camera=nullptr;
   if(IsValid(Clock)){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
  }
 }
 int32 ActualViewIndex()const{return bMetrics?MetricIndices[View]:StillIndices[View];}
 void WriteViewReceipt()
 {
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
  Label=FPaths::MakeValidFileName(Label.IsEmpty()?TEXT("SampleViews"):Label,TEXT('_'));Label.ReplaceInline(TEXT(".."),TEXT("_"));
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC920/Views")/Label;
  IFileManager::Get().MakeDirectory(*Directory,true);
  auto Receipt=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Requested;
  for(int32 Index:StillIndices)Requested.Add(MakeShared<FJsonValueNumber>(Index));
  Receipt->SetStringField(TEXT("run_label"),Label);Receipt->SetBoolField(TEXT("explicit_subset"),bExplicitSubset);
  Receipt->SetBoolField(TEXT("passed"),FailedStills==0 && CapturedViews.Num()==StillIndices.Num());
  Receipt->SetNumberField(TEXT("failed_requested_stills"),FailedStills);
  Receipt->SetArrayField(TEXT("requested_view_indices"),Requested);Receipt->SetArrayField(TEXT("captured_views"),CapturedViews);
  Receipt->SetStringField(TEXT("scope"),TEXT("Fixed-camera rendered comparisons only. Original view indices and filenames are retained; neither normal-player traversal nor user subjective visual approval is claimed."));
  FString Json;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Json));
  const FString File=Directory/TEXT("receipt.json");
  if(IFileManager::Get().FileExists(*File) || !FFileHelper::SaveStringToFile(Json,*File))
   Test->AddError(TEXT("Could not retain a new comparison-view receipt without overwriting earlier evidence"));
 }
 void Place()
 {
  if(bReferenceMetrics)
  {
   Camera->GetCameraComponent()->FieldOfView=ReferenceMetricsFOV;
   Camera->GetCameraComponent()->bConstrainAspectRatio=false;
   Camera->SetActorTransform(ReferenceMetricsPose);Clock->SetTimeOfDay(16.5f);At=FPlatformTime::Seconds();bReferenceSampleBookmark=false;
   TRACE_BOOKMARK(TEXT("C923ReferenceMetrics351 WarmupBegin wall=%.6f game=%.6f"),At,Camera->GetWorld()->GetTimeSeconds());return;
  }
  const FVector Eyes[]={FVector(-5000,-3000,225),FVector(800,-3100,225),FVector(-3300,-1800,210),
   FVector(4056,1500,220),FVector(-10500,1000,225),FVector(1800,1600,215),FVector(-9000,-8000,6500),
   FVector(-5000,-3000,225),FVector(-5000,-3000,225),FVector(1800,1600,215),
   FVector(-3748,-1120,215),FVector(-3760,-1480,170),FVector(4950,-2500,350),
   FVector(-4900,-2700,2200),FVector(12100,3800,195),FVector(-270,-1250,640),
   FVector(3390,-1000,190),FVector(2970,-1110,215),FVector(-27000,5500,9000)};
  const FVector Aims[]={FVector(1000,-700,800),FVector(-2500,-1600,280),FVector(-3100,-1400,320),
   FVector(4056,-300,180),FVector(-10000,3500,160),FVector(0,1100,80),FVector(0,0,1500),
   FVector(1000,-700,800),FVector(1000,-700,800),FVector(0,1100,80),
   FVector(-3748,-985,160),FVector(-3760,-1304,423),FVector(4150,-1600,500),
   FVector(-3100,-650,1700),FVector(12740,4150,125),FVector(-680,-1180,565),
   FVector(3158,-840,177),FVector(2290,-640,155),FVector(-18000,6500,-130)};
  // All11 metrics mappings and original still identities stay unchanged.
  // View11 observes the actual Riva lettering from inside its arcade; the
  // previous exterior eye was occluded by the real arch/lamp geometry.
  // Still15 also captures the existing Courtyard interior station.
  // Courtyard origin is (0,-800,55)cm;
  // first-floor front_lounge bounds are X[-1000,-190],Y[-1550,-920],
  // Z[480,775]. Eye(-270,-1250,640) is 160cm above its finished floor,
  // 80cm inside the nearest partition and away from the authored seat groups.
  // Gallery origin is (3300,-700,55)cm. Door camera16 is inside the central
  // hall X[3170,3430],40cm from its east wall; it faces the west-wall portal.
  // Suite camera17 is local(-3.3,-4.1,1.6)m, inside the finished suite
  // X[-13.05,-1.55],Y[-5.4,3.4], above the adjacent armchair back.
  // These fixed comparisons are visual evidence, not normal-player traversal.
  // View18 is outside the new coast (west of its X=-203m limit) and observes
  // the actual west/north water formerly removed by the retired land rectangle.
  const int32 ViewIndex=ActualViewIndex();
  const int32 Station=bMetrics && ViewIndex==MetricsCount-1?15:ViewIndex;
  Camera->GetCameraComponent()->FieldOfView=Station==16?75:65;
  Camera->SetActorLocationAndRotation(Eyes[Station],(Aims[Station]-Eyes[Station]).Rotation());
  Clock->SetTimeOfDay(ViewIndex==7?17.8f:(ViewIndex==8 || ViewIndex==9)?22:12);At=FPlatformTime::Seconds();
 }
public:
 FViews(FAutomationTestBase* T,bool InMetrics,const TArray<int32>& InStillIndices=TArray<int32>(),bool InExplicitSubset=false,const TArray<int32>& InMetricIndices=TArray<int32>()):
  Test(T),bMetrics(InMetrics),StillIndices(InStillIndices),MetricIndices(InMetricIndices),bExplicitSubset(InExplicitSubset)
 {bReferenceMetrics=bMetrics && FParse::Param(FCommandLine::Get(),TEXT("C923ReferenceMetrics"));if(!bMetrics && StillIndices.IsEmpty())for(int32 I=0;I<StillCount;++I)StillIndices.Add(I);if(bMetrics && MetricIndices.IsEmpty())for(int32 I=0;I<MetricsCount;++I)MetricIndices.Add(I);}
 ~FViews()override{RestoreView();}
 bool Update()override
 {
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<8)return false;
  auto* PC=W->GetFirstPlayerController();if(!PC)return false;
  if(!Camera)
  {
   if(bReferenceMetrics)
   {
    if(!Cast<ANeonCityPlayerController>(PC)){Test->AddError(TEXT("Reference timing requires the normal photography HUD controller"));return true;}
    if(MetricIndices.Num()!=1 || MetricIndices[0]!=0){Test->AddError(TEXT("Reference timing requires only its explicit index0"));return true;}
    int32 Count=0;
    for(TActorIterator<ACameraActor> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel && I->ActorHasTag(TEXT("NeonCity.MainlandC923.ReferenceTarget")))
    {ReferenceMetricsPose=I->GetActorTransform();ReferenceMetricsFOV=I->GetCameraComponent()->FieldOfView;++Count;}
    if(Count!=1 || !ReferenceMetricsPose.GetLocation().Equals(FVector(-5300,-2662,275),.01) || !ReferenceMetricsPose.Rotator().Equals(FRotator(-1.5,4.004173,0),.0001) || !FMath::IsNearlyEqual(ReferenceMetricsFOV,65.f))
    {Test->AddError(TEXT("Reference timing requires the actual calibrated photograph camera"));return true;}
   }
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I){Clock=*I;break;}
   if(!Clock){Test->AddError(TEXT("Normal day/night clock missing"));return true;}
   OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);
   MetricsController=PC;OldView=PC->GetViewTarget();Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;
   if(bReferenceMetrics){auto* NC=CastChecked<ANeonCityPlayerController>(PC);bOldPhotographyHidden=NC->IsPhotographyHUDHidden();NC->SetPhotographyHUDHidden(true);}
   PC->SetViewTarget(Camera);Place();return false;
  }
  const double Age=FPlatformTime::Seconds()-At;
  if(bReferenceMetrics && Age>=15 && !bReferenceSampleBookmark)
  {
   bReferenceSampleBookmark=true;
   TRACE_BOOKMARK(TEXT("C923ReferenceMetrics351 FormalBegin wall=%.6f game=%.6f age=%.6f"),FPlatformTime::Seconds(),W->GetTimeSeconds(),Age);
  }
  if(bMetrics)Metrics.Sample(W,bReferenceMetrics?FString::Printf(TEXT("C923Reference-%s"),Age<15?TEXT("warmup"):TEXT("sample")):ActualViewIndex()==MetricsCount-1
   ?FString::Printf(TEXT("CourtyardInterior-View10-%s"),Age<15?TEXT("warmup"):TEXT("sample"))
   :FString::Printf(TEXT("View%02d-%s"),ActualViewIndex(),Age<15?TEXT("warmup"):TEXT("sample")));
  if(Age<(bMetrics?40:15))return false;
  if(!bMetrics)
  {
   const int32 ViewIndex=ActualViewIndex();const FString Name=FString::Printf(TEXT("C920-View-%02d.jpg"),ViewIndex);bool bCaptured=false;
#if WITH_EDITOR
   bCaptured=FNeonCityVisualEvidence::CaptureRenderedJPEG(W,Name);
#endif
   Test->TestTrue(FString::Printf(TEXT("Actual rendered C920 comparison index%d file%s"),ViewIndex,*Name),bCaptured);
   if(!bCaptured)++FailedStills;
   auto Entry=MakeShared<FJsonObject>();Entry->SetNumberField(TEXT("view_index"),ViewIndex);
   Entry->SetStringField(TEXT("output_file_name"),Name);Entry->SetBoolField(TEXT("capture_success"),bCaptured);
   Entry->SetStringField(TEXT("camera_eye_cm"),Camera->GetActorLocation().ToString());
   Entry->SetStringField(TEXT("camera_rotation"),Camera->GetActorRotation().ToString());
   Entry->SetNumberField(TEXT("hour"),Clock->GetTimeOfDay());CapturedViews.Add(MakeShared<FJsonValueObject>(Entry));
  }
  if(++View<(bMetrics?MetricIndices.Num():StillIndices.Num())){Place();return false;}
  if(bMetrics)
  {
   if(bReferenceMetrics)TRACE_BOOKMARK(TEXT("C923ReferenceMetrics351 FormalEnd wall=%.6f game=%.6f age=%.6f"),FPlatformTime::Seconds(),W->GetTimeSeconds(),Age);
   Test->TestTrue(TEXT("Readback-free performance samples retained"),Metrics.Flush());
  }
  RestoreView();if(!bMetrics)WriteViewReceipt();return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandSupport920,"NeonCity.Phase205c.MainlandC920.Standalone.StreetSupport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandSupport920::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(MainlandSample920::FSupport(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandCourtParkingClearance920,"NeonCity.Phase205c.MainlandC920.Standalone.CourtParkingClearance",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandCourtParkingClearance920::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(MainlandSample920::FSupport(this,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandWalk920,"NeonCity.Phase205c.MainlandC920.Standalone.ContinuousWalk",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandWalk920::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(MainlandSample920::FWalk(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandWalkStartHitchRepro920,"NeonCity.Phase205c.MainlandC920.Standalone.WalkStartHitchRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandWalkStartHitchRepro920::RunTest(const FString&)
{
 FString Channels;const bool bTraceArgument=FParse::Value(FCommandLine::Get(),TEXT("trace="),Channels,false);
 TArray<FString> EnabledChannels;Channels.ParseIntoArray(EnabledChannels,TEXT(","),true);
 const bool bCPU=EnabledChannels.Contains(TEXT("cpu")),bFrame=EnabledChannels.Contains(TEXT("frame")),bBookmark=EnabledChannels.Contains(TEXT("bookmark"));
 const bool bPersistenceDisabled=FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"));
 const bool bStills=FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")),bMetrics=FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics"));
 if(!bTraceArgument || !bCPU || !bFrame || !bBookmark || !bPersistenceDisabled || bStills || bMetrics)
 {AddError(FString::Printf(TEXT("WalkStartHitchRepro requires isolated cpu/frame/bookmark trace with automatic persistence disabled and no image/clean-metrics collection; trace_argument=%d channels='%s' cpu=%d frame=%d bookmark=%d persistence_disabled=%d stills=%d metrics=%d"),
   bTraceArgument,*Channels,bCPU,bFrame,bBookmark,bPersistenceDisabled,bStills,bMetrics));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(MainlandSample920::FWalk(this,true));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandViews920,"NeonCity.Phase205c.MainlandC920.Standalone.SampleViews",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandViews920::RunTest(const FString&)
{
 TArray<int32> Indices;bool bExplicitSubset=false;
 if(!MainlandSample920::ReadStillViewIndices(this,Indices,bExplicitSubset))return false;
 ADD_LATENT_AUTOMATION_COMMAND(MainlandSample920::FViews(this,false,Indices,bExplicitSubset));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandMetrics920,"NeonCity.Phase205c.MainlandC920.Standalone.SampleMetrics",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandMetrics920::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics")))return false;
 FString Requested;TArray<int32> Indices;
 if(FParse::Value(FCommandLine::Get(),TEXT("C920MetricsIndices="),Requested,false))
 {
  TArray<FString> Parts;Requested.ParseIntoArray(Parts,TEXT(","),false);TSet<int32> Seen;
  if(Parts.IsEmpty()){AddError(TEXT("Empty affected performance selection"));return false;}
  for(const FString& Part:Parts)
  {
   int32 Index=-1;if(!Part.IsNumeric() || !LexTryParseString(Index,*Part) || Index<0 || Index>10 || Seen.Contains(Index))
   {AddError(TEXT("Affected performance views must be unique existing indices0..10"));return false;}
   Seen.Add(Index);Indices.Add(Index);
  }
 }
 ADD_LATENT_AUTOMATION_COMMAND(MainlandSample920::FViews(this,true,TArray<int32>(),false,Indices));return true;
}
#endif
