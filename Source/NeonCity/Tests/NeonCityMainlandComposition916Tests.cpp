#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Adventure/IslandEstateAmenity.h"
#include "Adventure/IslandHomeWardrobe.h"
#include "Adventure/IslandOwnedAppearanceState.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandEstate916
{
void Key(APlayerController*,FKey,bool);
bool Focus(APlayerController*,ANeonCityPlayerCharacter*,AActor*);

// Independent, disclosed floor fixtures isolate the newly fitted furniture.
// Every action uses normal player F/page inputs and the existing finite use.
class FCompositionUses:public IAutomationLatentCommand
{
public:
 explicit FCompositionUses(FAutomationTestBase* InTest,int32 StartIndex=0,int32 EndIndex=9):Test(InTest),Index(StartIndex),LastExclusive(EndIndex){}
 bool Update()override
 {
  W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<6)return false;
  if(Ending)
  {
   if(Amenity && Amenity->IsOccupied())
   {
    const double Age=FPlatformTime::Seconds()-CleanupStarted;
    if(Age>10){Test->AddError(TEXT("Composition safe cleanup timed out; occupied player was not relocated"));RestoreState();return true;}
    if(Age>NextCleanupRetry){Amenity->FinishUse(false);NextCleanupRetry=Age+1;}
    return false;
   }
   if(P){P->GetCharacterMovement()->StopMovementImmediately();P->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);}
   RestoreState();return true;
  }
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>160)return End(TEXT("New composition use timeout"));
  if(!P)
  {
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return End(TEXT("Normal on-foot player missing"));Original=P->GetActorTransform();
   Appearance=UIslandOwnedAppearanceState::FindOrCreate(P);if(!Appearance)return End(TEXT("Owned appearance state missing"));
   OriginalPalette=Appearance->GetSelectedPalette();OriginalMask=Appearance->GetOwnedPaletteMask();
   for(int32 N=0;N<6;++N)for(TActorIterator<AIslandHomeWardrobe> I(W);I;++I)
    if(I->ActorHasTag(FName(*FString::Printf(TEXT("Mainland916.Composition.Wardrobe.%d"),N))))Targets.Add(*I);
   Test->TestEqual(TEXT("Six fitted wardrobe fronts exist"),Targets.Num(),6);
   for(const auto ID:{TEXT("Mainland916.Composition.Read"),TEXT("Mainland916.Composition.Drink"),TEXT("Mainland916.Composition.Bath")})
   {
    int32 Count=0;for(TActorIterator<AIslandEstateAmenity> I(W);I;++I)if(I->AmenityID==ID){Targets.Add(*I);++Count;}
    Test->TestEqual(FString::Printf(TEXT("Unique %s owner"),ID),Count,1);
   }
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I){Clock=*I;OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(12);break;}
   if(Targets.Num()!=9 || !Clock)return End(TEXT("Composition owners or clock missing"));
  }
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   Target=Targets[Index];Amenity=Cast<AIslandEstateAmenity>(Target);
   if(Amenity && (!Amenity->HasPresentation() || Amenity->IsUsing() || Amenity->IsOccupied()))return End(TEXT("New amenity lacks presentation or is already in use"));
   if(!Approach())return End(FString::Printf(TEXT("No supported unobstructed normal prompt for %s"),*GetNameSafe(Target)));
   PaletteBefore=Appearance->GetSelectedPalette();CompletedBefore=Amenity?Amenity->GetCompletedUses():0;
   SawAudio=SawState=Captured=false;return Next(1);
  }
  if(Stage==1 && Age>1.5)
  {
   if(!Focus(PC,P,Target))return End(FString::Printf(TEXT("Normal prompt lost after settling %s"),*GetNameSafe(Target)));
   ViewOffsetBefore=P->GetCameraBoom()->TargetOffset;ViewRotationBefore=PC->GetControlRotation();
   Key(PC,EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.25)
  {
   Key(PC,EKeys::F,false);
   if(!Amenity)
   {
    if(Appearance->GetSelectedPalette()==PaletteBefore || Appearance->GetOwnedPaletteMask()!=OriginalMask)return End(TEXT("Wardrobe F failed to change the owned outfit safely"));
    Capture(TEXT("Wardrobe"));return Advance();
   }
   if(!Amenity->IsUsing())return End(FString::Printf(TEXT("Normal F failed to start %s"),*GetNameSafe(Target)));
   return Next(3);
  }
  if(Stage==3)
  {
   SawAudio|=Amenity->IsFeedbackPlaying();
   if(Amenity->Kind==EIslandEstateAmenityKind::Read)
   {
    if(!PagePressed && Age>2){Key(PC,EKeys::Right,true);PagePressed=true;}
    if(PagePressed && Age>2.3)Key(PC,EKeys::Right,false);
    SawState|=Amenity->GetPageIndex()==1 && Amenity->HeldObject->IsVisible() && Amenity->TaskBody->IsVisible() && !Amenity->Details->Text.IsEmpty();
   }
   else if(Amenity->Kind==EIslandEstateAmenityKind::Drink)
    SawState|=Amenity->GetDrinkFraction()<.7f && Amenity->HeldObject->IsVisible() && Amenity->WaterSurface->IsVisible() && Amenity->TaskBody->IsVisible();
   else if(Amenity->Kind==EIslandEstateAmenityKind::Bath)
    SawState|=Amenity->GetWaterFraction()>.8f && Amenity->WaterSurface->IsVisible() && Amenity->WaterStream->IsVisible() && Amenity->IsOccupied();
   if(SawState && !Captured)
   {
    if(Amenity->bUseSeatedPose)
    {
     auto* Body=Amenity->Kind==EIslandEstateAmenityKind::Read?static_cast<USkinnedMeshComponent*>(Amenity->TaskBody.Get()):static_cast<USkinnedMeshComponent*>(Amenity->Occupant->GetDriverMesh());
     if(!Body)return End(TEXT("Seated amenity body missing"));
     FVector Eye;FRotator Look;PC->GetPlayerViewPoint(Eye,Look);const float Clearance=FVector::Dist(Eye,Body->GetSocketLocation(TEXT("head")));
     Test->AddInfo(FString::Printf(TEXT("Normal amenity camera/head clearance%.1fcm"),Clearance));
     if(Clearance<90)return End(TEXT("Normal amenity camera is inside or too close to the represented player"));
     if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Amenity)return End(TEXT("Fitted seated view lost the normal F finish target"));
    }
    if(Amenity->Kind==EIslandEstateAmenityKind::Read || Amenity->Kind==EIslandEstateAmenityKind::Drink)
    {
     FVector Right=Amenity->RightHandLocal;
     if(Amenity->Kind==EIslandEstateAmenityKind::Drink)Right+=Amenity->HeldObject->GetRelativeLocation()-Amenity->ObjectRestLocal;
     const float R=FVector::Dist(Amenity->TaskBody->GetBoneLocationByName(TEXT("hand_r"),EBoneSpaces::WorldSpace),Amenity->GetActorTransform().TransformPosition(Right));
     const float L=FVector::Dist(Amenity->TaskBody->GetBoneLocationByName(TEXT("hand_l"),EBoneSpaces::WorldSpace),Amenity->GetActorTransform().TransformPosition(Amenity->LeftHandLocal));
     Test->AddInfo(FString::Printf(TEXT("Evaluated composition hand contacts right%.2f left%.2f cm"),R,L));
     if(R>12 || L>12)return End(TEXT("New fitted furnishing exceeds evaluated hand contact tolerance"));
     if(Amenity->Kind==EIslandEstateAmenityKind::Drink)
     {
      const FVector Lip=Amenity->HeldObject->GetComponentTransform().TransformPosition(FVector(0,0,8.6)),Head=Amenity->TaskBody->GetBoneLocationByName(TEXT("head"),EBoneSpaces::WorldSpace);
      Test->AddInfo(FString::Printf(TEXT("Raised cup lip%s versus actual head%s"),*Lip.ToString(),*Head.ToString()));
      if(Lip.Z<Head.Z-8 || Lip.Z>Head.Z+22 || FVector::Dist2D(Lip,Head)>25)return End(TEXT("Standing cup is not raised to the actual face"));
     }
    }
    Capture(TEXT("Active"));
   }
   if(Age>Amenity->SessionSeconds+10)return End(TEXT("New amenity failed to finish and release the player"));
   if(Amenity->IsUsing())return false;
   if(Amenity->GetCompletedUses()!=CompletedBefore+1 || !SawAudio || !SawState)return End(TEXT("Amenity lacks real visual state/audio/finite completion"));
   if(Amenity->IsOccupied() || !P->GetCharacterMovement()->IsMovingOnGround() || PC->IsMoveInputIgnored() || !P->GetActorEnableCollision())return End(TEXT("Amenity did not restore normal walking and collision"));
   if(Amenity->SeatedViewLift>0 && (!P->GetCameraBoom()->TargetOffset.Equals(ViewOffsetBefore,.01f) || !PC->GetControlRotation().Equals(ViewRotationBefore,.1f)))return End(TEXT("Amenity failed to restore the previous orbit camera"));
   return Advance();
  }
  return false;
 }
private:
 bool Approach()
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(CompositionApproach916),false,P);TArray<FVector> Offsets;
  if(Amenity && !Amenity->bUseSeatedPose)Offsets.Add(Amenity->StandLocal);
  else if(!Amenity)Offsets.Add(FVector(130,0,-110));
  else for(float D:{140.f,180.f,220.f})for(FVector Axis:{FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0)})Offsets.Add(Axis*D);
  for(const auto& Local:Offsets)
  {
   const FVector Feet=Target->GetActorTransform().TransformPosition(Local);FHitResult Floor;
   if(!W->LineTraceSingleByChannel(Floor,Feet+FVector(0,0,190),Feet-FVector(0,0,150),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7f || FMath::Abs(Floor.ImpactPoint.Z-Feet.Z)>20)continue;
   const FVector Stand=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))continue;
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   if(Amenity && !Amenity->bUseSeatedPose)P->SetActorRotation(Amenity->GetActorTransform().TransformVectorNoScale(Amenity->FacingLocal).Rotation());
   if(!Focus(PC,P,Target))continue;
   P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);return true;
  }
  return false;
 }
 void Capture(const TCHAR* Suffix)
 {
#if WITH_EDITOR
  if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))Test->TestTrue(TEXT("Actual composition use screenshot retained"),FNeonCityVisualEvidence::CaptureRenderedJPEG(W,FString::Printf(TEXT("Estate916-Composition-%02d-%s.jpg"),Index,Suffix)));
#endif
  Captured=true;
 }
 bool Next(int32 N){Stage=N;At=W->GetTimeSeconds();return false;}
 bool Advance(){Test->AddInfo(FString::Printf(TEXT("Normal F/state/feedback/exit passed %s"),*GetNameSafe(Target)));PagePressed=false;if(++Index>=FMath::Min(Targets.Num(),LastExclusive))return End(TEXT(""));return Next(0);}
 void RestoreState(){if(Appearance)Appearance->RestoreState(OriginalPalette,OriginalMask);if(Clock){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}}
 bool End(const FString& Error)
 {
  Key(PC,EKeys::F,false);Key(PC,EKeys::Right,false);if(!Error.IsEmpty())Test->AddError(Error);
  if(Amenity && Amenity->IsUsing())Amenity->FinishUse(false);
  CleanupStarted=FPlatformTime::Seconds();Ending=true;return false;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;
 UIslandOwnedAppearanceState* Appearance=nullptr;ANeonCityDayNightCycle* Clock=nullptr;
 TArray<AActor*> Targets;AActor* Target=nullptr;AIslandEstateAmenity* Amenity=nullptr;FTransform Original;
 FVector ViewOffsetBefore;FRotator ViewRotationBefore;
 int32 Stage=0,Index=0,LastExclusive=9,OriginalPalette=0,PaletteBefore=0,CompletedBefore=0;uint8 OriginalMask=0;
 float At=0,OldHour=12;double Started=0,CleanupStarted=0,NextCleanupRetry=1;
 bool Ending=false,OldCycle=false,SawAudio=false,SawState=false,Captured=false,PagePressed=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateCompositionUses916,"NeonCity.Phase205c.Mainland916.Standalone.EstateCompositionUses",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateCompositionUses916::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;
 ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FCompositionUses(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateCompositionTail916,"NeonCity.Phase205c.Mainland916.Standalone.EstateCompositionTailRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateCompositionTail916::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;
 ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FCompositionUses(this,2));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateDrinkBath916,"NeonCity.Phase205c.Mainland916.Standalone.EstateDrinkBathRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateDrinkBath916::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;
 ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FCompositionUses(this,7));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateReadCamera916,"NeonCity.Phase205c.Mainland916.Standalone.EstateReadCameraRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateReadCamera916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FCompositionUses(this,6,7));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateDrinkContact916,"NeonCity.Phase205c.Mainland916.Standalone.EstateDrinkContactRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateDrinkContact916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FCompositionUses(this,7,8));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateBathCamera916,"NeonCity.Phase205c.Mainland916.Standalone.EstateBathCameraRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateBathCamera916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FCompositionUses(this,8,9));return true;}
#endif
