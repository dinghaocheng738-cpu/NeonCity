#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Adventure/IslandEstateAmenity.h"
#include "Adventure/IslandLeisureFacility.h"
#include "Adventure/IslandScenePanel.h"
#include "Adventure/IslandLandLight.h"
#include "Components/LightComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandEstate916
{
void Key(APlayerController*,FKey,bool);
bool Focus(APlayerController*,ANeonCityPlayerCharacter*,AActor*);

// Disclosed independent initial approaches, followed by normal F, animation,
// finite sessions and safe W exits. These do not replace continuous room access.
class FAmenities:public IAutomationLatentCommand
{
public:explicit FAmenities(FAutomationTestBase* InTest,int32 StartIndex=0,int32 EndIndex=9,bool ContactEvidence=false,bool ReverseEntry=false):Test(InTest),Index(StartIndex),LastExclusive(EndIndex),BedContactEvidence(ContactEvidence),EarlyReverse(ReverseEntry){}
 bool Update()override
 {
  W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<6)return false;
  if(Ending)
  {
   if(Seat && Seat->IsOccupied())
   {
    const double CleanupAge=FPlatformTime::Seconds()-CleanupStarted;
    if(CleanupAge>10){Test->AddError(TEXT("Amenity safe cleanup timed out; isolated process must end without relocating the occupied player"));if(Clock){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}return true;}
    if(CleanupAge>NextCleanupRetry){Seat->Leave();NextCleanupRetry=CleanupAge+1;}
    return false;
   }
   if(P)P->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);
   if(Clock){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
   return true;
  }
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>180)return End(TEXT("Estate amenity tour timeout"));
  const float Age=W->GetTimeSeconds()-At;
  if(!P)
  {
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return End(TEXT("Normal on-foot player missing"));Original=P->GetActorTransform();
   for(TActorIterator<AIslandHomeSeat> I(W);I;++I)
    if(I->PropertyID==TEXT("Property.Mainland.RivieraEstate916") && I->bSleepBed)Targets.Add(*I);
   Test->TestEqual(TEXT("Six fitted suite beds exist"),Targets.Num(),6);
   for(TActorIterator<AIslandLeisureFacility> I(W);I;++I)
    if(I->PropertyID==TEXT("Property.Mainland.RivieraEstate916") && I->Kind==EIslandLeisureKind::Sauna)Targets.Add(*I);
   for(TActorIterator<AIslandEstateAmenity> I(W);I;++I)
    if(I->AmenityID==TEXT("Mainland916.Estate.Sink"))Targets.Add(*I);
   for(TActorIterator<AIslandScenePanel> I(W);I;++I)
    if(I->ZoneID==TEXT("Mainland916.Estate.Lighting")){Panel=*I;OriginalScene=Panel->GetScene();Targets.Add(*I);}
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I){Clock=*I;OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);break;}
   if(Targets.Num()!=9 || !Clock)return End(TEXT("Native beds, sauna, sink or lighting controls missing"));
  }
  if(Stage==0)
  {
   Target=Targets[Index];Seat=Cast<AIslandHomeSeat>(Target);Amenity=Cast<AIslandEstateAmenity>(Target);Leisure=Cast<AIslandLeisureFacility>(Target);
   if(Target==Panel){Clock->SetTimeOfDay(22);Panel->SetScene(OriginalScene);}
   if(Seat && Seat->IsOccupied())return End(TEXT("Initial fixture requires a vacant furnishing"));
   if(!Approach())return End(FString::Printf(TEXT("No supported clear F approach for %s"),*GetNameSafe(Target)));
   if(Amenity)CompletedBefore=Amenity->GetCompletedUses();
   if(Seat && Seat->bSleepBed){SleptBefore=Seat->GetSleptHours();if(!Seat->BedEnter || !Seat->BedSleep || !Seat->BedExit)return End(TEXT("Bed lacks actual licensed transition animations"));}
   if(Leisure)TemperatureBefore=Leisure->GetTemperature();
   return Next(1);
  }
  if(Stage==1 && Age>1.5)
  {
   if(!Focus(PC,P,Target))return End(FString::Printf(TEXT("Normal amenity prompt lost after settling %s local%s ground%d eligible%d"),*GetNameSafe(Target),*Target->GetActorTransform().InverseTransformPosition(P->GetActorLocation()).ToString(),P->GetCharacterMovement()->IsMovingOnGround(),Seat?Seat->CanInteract_Implementation(P):false));
   if(Target==Panel){for(const auto& Lamp:Panel->Lights)if(IsValid(Lamp))if(auto* Light=Lamp->GetLightComponent())LightIntensities.Add(Light,Light->Intensity);if(LightIntensities.IsEmpty())return End(TEXT("Lighting panel has no live bound lamps"));}
   Key(PC,EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.3)
  {
   Key(PC,EKeys::F,false);
   if(Target==Panel)
   {
    if(Panel->GetScene()==OriginalScene)return End(TEXT("Normal F did not change the room lighting scene"));
    int32 Changed=0;for(const auto& Before:LightIntensities)if(IsValid(Before.Key) && !FMath::IsNearlyEqual(Before.Key->Intensity,Before.Value,1.f))++Changed;
    if(Changed==0)return End(TEXT("Lighting scene changed but no actual bound light intensity changed"));
#if WITH_EDITOR
    if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))FNeonCityVisualEvidence::CaptureRenderedJPEG(W,TEXT("Estate916-LightingChanged.jpg"));
#endif
    Panel->SetScene(OriginalScene);for(const auto& Before:LightIntensities)if(!IsValid(Before.Key) || !FMath::IsNearlyEqual(Before.Key->Intensity,Before.Value,1.f))return End(TEXT("Lighting scene failed to restore live lamp intensity"));
    Test->AddInfo(FString::Printf(TEXT("Normal F changed%d actual lamps and restored their intensities"),Changed));return Advance();
   }
   if(Amenity){if(!Amenity->IsUsing())return End(FString::Printf(TEXT("Normal F failed to start sink: eligible%d reach%d local%s facing%s"),Amenity->CanInteract_Implementation(P),Amenity->CanReachFixture(P),*Amenity->GetActorTransform().InverseTransformPosition(P->GetActorLocation()).ToString(),*Amenity->GetActorTransform().InverseTransformVectorNoScale(P->GetActorForwardVector()).ToString()));}
   else if(!Seat->IsOccupied())return End(TEXT("Normal F failed to occupy the fitted bed/sauna"));
   return Next(3);
  }
  if(Stage==3)
  {
   if(Amenity)
   {
    SawWater|=Amenity->GetWaterFraction()>.5f && Amenity->WaterStream->IsVisible();SawAudio|=Amenity->IsFeedbackPlaying();
    if(Amenity->GetUseSeconds()>3 && Amenity->GetWaterFraction()>.5f)
    {
     const float R=FVector::Dist(Amenity->TaskBody->GetBoneLocationByName(TEXT("hand_r"),EBoneSpaces::WorldSpace),Amenity->GetActorTransform().TransformPosition(Amenity->RightHandLocal));
     const float L=FVector::Dist(Amenity->TaskBody->GetBoneLocationByName(TEXT("hand_l"),EBoneSpaces::WorldSpace),Amenity->GetActorTransform().TransformPosition(Amenity->LeftHandLocal));
     SawContact|=R<12 && L<12;
#if WITH_EDITOR
     if(!Captured && FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills"))){FNeonCityVisualEvidence::CaptureRenderedJPEG(W,TEXT("Estate916-SinkWaterContact.jpg"));Captured=true;}
#endif
    }
    if(Age>18)return End(TEXT("Sink did not complete its finite wash/rinse cycle"));
    if(Amenity->IsUsing())return false;
    if(Amenity->GetCompletedUses()!=CompletedBefore+1 || !SawWater || !SawAudio || !SawContact || !P->GetCharacterMovement()->IsMovingOnGround() || PC->IsMoveInputIgnored())return End(TEXT("Sink lacks evaluated hand contact/water/audio/completion or restored walking"));
    return Advance();
   }
   if(EarlyReverse && Seat->bSleepBed)
   {
    if(Age<2.6)return false;
    if(!Seat->IsOccupied() || Seat->IsSleeping() || Seat->BedRestOffset.IsNearlyZero())return End(TEXT("Late-entry reverse fixture did not reach the fitted entry phase"));
    Test->AddInfo(TEXT("Normal W cancels the fitted late-entry pose through its real reversed animation"));Key(PC,EKeys::W,true);return Next(4);
   }
   if(Age<4)return false;
   if(Seat->bSleepBed && (!Seat->IsSleeping() || Seat->GetSleptHours()<=SleptBefore))return End(TEXT("Bed did not reach its real animated sleeping/rest state"));
   if(Leisure && (!Leisure->IsFeedbackPlaying() || Leisure->GetTemperature()<=TemperatureBefore))return End(TEXT("Sauna lacks real heat/audio feedback"));
#if WITH_EDITOR
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))FNeonCityVisualEvidence::CaptureRenderedJPEG(W,FString::Printf(TEXT("Estate916-Amenity-%02d.jpg"),Index));
#endif
   if(Seat->bSleepBed && BedContactEvidence)
   {
    OldView=PC->GetViewTarget();BedCamera=W->SpawnActor<ACameraActor>();BedCamera->GetCameraComponent()->FieldOfView=55;
    const auto T=Seat->GetActorTransform();const FVector Eye=T.TransformPosition(FVector(30,320,160)),Aim=T.TransformPosition(FVector(0,0,63));
    BedCamera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());PC->SetViewTarget(BedCamera);return Next(6);
   }
   Key(PC,EKeys::W,true);return Next(4);
  }
  if(Stage==6 && Age>1.2)
  {
   auto* Body=Seat->Occupant->GetDriverMesh();if(!Body)return End(TEXT("Sleeping body missing in side evidence"));
   const auto T=Seat->GetActorTransform();const FVector Hip=T.InverseTransformPosition(Body->GetSocketLocation(TEXT("pelvis"))),Head=T.InverseTransformPosition(Body->GetSocketLocation(TEXT("head"))),Foot=T.InverseTransformPosition(Body->GetSocketLocation(TEXT("foot_l")));
   Test->AddInfo(FString::Printf(TEXT("Actual sleeping local cm hip%s head%s foot%s"),*Hip.ToString(),*Head.ToString(),*Foot.ToString()));
   if(Head.X>Hip.X-35 || Foot.X<Hip.X+40 || FMath::Abs(Hip.Y)>35 || Hip.Z<60 || Hip.Z>65 || Foot.Z<58 || Foot.Z>63 || Head.Z<75 || Head.Z>84)return End(TEXT("Sleeping body does not contact the measured new mattress/pillows"));
#if WITH_EDITOR
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))Test->TestTrue(TEXT("Actual settled sleeping side view retained"),FNeonCityVisualEvidence::CaptureRenderedJPEG(W,FString::Printf(TEXT("Estate916-BedContact-%02d.jpg"),Index)));
#endif
   RestoreView();Key(PC,EKeys::W,true);return Next(4);
  }
  if(Stage==4 && Age>.25){Key(PC,EKeys::W,false);return Next(5);}
  if(Stage==5)
  {
   if(Seat->IsOccupied()){if(Age>8)return End(TEXT("Animated amenity exit failed to find a safe supported stance"));return false;}
   if(!P->GetCharacterMovement()->IsMovingOnGround() || PC->IsMoveInputIgnored() || !P->GetActorEnableCollision())return End(TEXT("Amenity exit did not restore normal player movement/collision"));
   return Advance();
  }
  return false;
 }
private:
 bool Approach()
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(EstateAmenityApproach916),false,P);
  TArray<FVector> Offsets;
  if(Amenity)Offsets.Add(Amenity->StandLocal);
  else if(Seat && Seat->bSleepBed){Offsets.Add(FVector(0,130,0));Offsets.Add(FVector(0,140,0));}
  else for(float Distance:{140.f,180.f,220.f})for(FVector D:{FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0)})Offsets.Add(D*Distance);
  for(const FVector& Local:Offsets)
  {
   FVector AtWorld=Target->GetActorTransform().TransformPosition(Local);FHitResult Floor;
   if(!W->LineTraceSingleByChannel(Floor,AtWorld+FVector(0,0,200),AtWorld-FVector(0,0,200),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7)
   {if(Target==Panel)Test->AddInfo(FString::Printf(TEXT("Panel approach rejected floor: local%s world%s hit%s component%s normal%s"),*Local.ToString(),*AtWorld.ToString(),*GetNameSafe(Floor.GetActor()),*GetNameSafe(Floor.GetComponent()),*Floor.ImpactNormal.ToString()));continue;}
   const FVector Stand=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))
   {if(Target==Panel)Test->AddInfo(FString::Printf(TEXT("Panel approach rejected occupied capsule: local%s stand%s"),*Local.ToString(),*Stand.ToString()));continue;}
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   // The disclosed standing fixture includes facing the actual faucet. Camera
   // focus alone does not turn a movement-oriented character's body.
   if(Amenity)P->SetActorRotation(Amenity->GetActorTransform().TransformVectorNoScale(Amenity->FacingLocal).Rotation());
   // Bed eligibility deliberately requires supported walking. Let the real
   // floor settle before evaluating it, rather than filtering it mid-fixture.
   if(!(Seat && Seat->bSleepBed) && !Focus(PC,P,Target))
   {
    if(Target==Panel)
    {
     FVector Eye;FRotator View;P->GetActorEyesViewPoint(Eye,View);FHitResult Obstruction;
     W->LineTraceSingleByChannel(Obstruction,Eye,Target->GetActorLocation(),ECC_Visibility,Q);
     Test->AddInfo(FString::Printf(TEXT("Panel approach rejected focus: local%s stand%s eye%s panel%s eligible%d hit%s component%s impact%s normal%s"),*Local.ToString(),*Stand.ToString(),*Eye.ToString(),*Target->GetActorLocation().ToString(),Panel->CanInteract_Implementation(P),*GetNameSafe(Obstruction.GetActor()),*GetNameSafe(Obstruction.GetComponent()),*Obstruction.ImpactPoint.ToString(),*Obstruction.ImpactNormal.ToString()));
    }
    continue;
   }
   P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);return true;
  }
  return false;
 }
 bool Next(int32 N){Stage=N;At=W->GetTimeSeconds();return false;}
 bool Advance(){Test->AddInfo(FString::Printf(TEXT("Normal F/feedback/exit passed item%d %s"),Index,*GetNameSafe(Target)));SawWater=SawAudio=false;Seat=nullptr;if(++Index>=FMath::Min(Targets.Num(),LastExclusive))return End(TEXT(""));return Next(0);}
 bool End(const FString& Error)
 {
  Key(PC,EKeys::F,false);Key(PC,EKeys::W,false);if(!Error.IsEmpty())Test->AddError(Error);
  RestoreView();
  if(Amenity && Amenity->IsUsing())Amenity->FinishUse(false);
  if(Seat && Seat->IsOccupied() && !Seat->Leave())Test->AddError(TEXT("Amenity cleanup could not leave safely"));
  if(Panel)Panel->SetScene(OriginalScene);CleanupStarted=FPlatformTime::Seconds();Ending=true;return false;
 }
 void RestoreView(){if(PC && OldView.IsValid())PC->SetViewTarget(OldView.Get());OldView.Reset();if(BedCamera){BedCamera->Destroy();BedCamera=nullptr;}}
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;
 TArray<AActor*> Targets;AActor* Target=nullptr;AIslandHomeSeat* Seat=nullptr;AIslandEstateAmenity* Amenity=nullptr;AIslandLeisureFacility* Leisure=nullptr;AIslandScenePanel* Panel=nullptr;ANeonCityDayNightCycle* Clock=nullptr;
 ACameraActor* BedCamera=nullptr;TWeakObjectPtr<AActor> OldView;
 FTransform Original;TMap<ULightComponent*,float> LightIntensities;int32 Stage=0,Index=0,LastExclusive=9,OriginalScene=0,CompletedBefore=0;float At=0,OldHour=12,SleptBefore=0,TemperatureBefore=0;double Started=0,CleanupStarted=0,NextCleanupRetry=1;bool Ending=false,OldCycle=false,SawWater=false,SawAudio=false,SawContact=false,Captured=false,BedContactEvidence=false,EarlyReverse=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateAmenities916,"NeonCity.Phase205c.Mainland916.Standalone.EstateAmenities",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateAmenities916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAmenities(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateSinkLighting916,"NeonCity.Phase205c.Mainland916.Standalone.EstateSinkLightingRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateSinkLighting916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAmenities(this,7));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateSink916,"NeonCity.Phase205c.Mainland916.Standalone.EstateSinkRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateSink916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAmenities(this,7,8));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateLightingOnly916,"NeonCity.Phase205c.Mainland916.Standalone.EstateLightingOnlyRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateLightingOnly916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAmenities(this,8,9));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateBedFinish916,"NeonCity.Phase205c.Mainland916.Standalone.EstateBedFinishRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateBedFinish916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAmenities(this,0,6,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateBedReverse916,"NeonCity.Phase205c.Mainland916.Standalone.EstateBedReverseRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateBedReverse916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAmenities(this,0,1,false,true));return true;}
#endif
