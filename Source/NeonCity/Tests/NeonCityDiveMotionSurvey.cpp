#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/DiveEquipmentComponent.h"
#include "Adventure/MarineSchool.h"
#include "Fishing/FishData.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Water/SwimmingComponent.h"
#include "Water/SwimmingAnimInstance.h"
#include "Water/DiveBreathingComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "SkeletalRenderPublic.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
namespace {
class FDiveMotionSurvey : public IAutomationLatentCommand
{
public:
 explicit FDiveMotionSurvey(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  if(!Start)Start=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Start>190){Test->AddError(TEXT("Dive motion survey timed out"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  auto* Dive=P->FindComponentByClass<UDivingComponent>();auto* Swim=P->FindComponentByClass<USwimmingComponent>();
  auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
  const float Now=W->GetTimeSeconds();
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-178000,0))))return false;
   if(!Test->TestTrue(TEXT("Ordinary suit equips"),Dive && Dive->Equip()))return true;
   FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(DiveMotionFloor),false,P);
   if(!W->LineTraceSingleByChannel(Hit,NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-178000,-300)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-178000,-20000)),ECC_WorldStatic,Q))return true;
   P->SetActorLocation(Hit.ImpactPoint+FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
   P->SetActorRotation(FRotator::ZeroRotator);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Swimming);
   P->GetCameraBoom()->TargetArmLength=340;P->GetCameraBoom()->SocketOffset=FVector(0,15,20);PC->SetControlRotation(FRotator(-7,155,0));
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(11);}
   Stage=1;At=NextFrame=Now;return false;
  }
  const float Age=Now-At;
  if(Now>=NextFrame && Age>2)
  {
   NextFrame=Now+.42f;
   FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("DiveMotion-%d-%03d.png"),Stage,Frame++));
   if(auto* Anim=Cast<USwimmingAnimInstance>(P->GetMesh()->GetAnimInstance()))
    Test->AddInfo(FString::Printf(TEXT("stage=%d age=%.2f pose=%d blend=%.2f head=%s regulator=%s"),Stage,Age,Anim->OceanPose,Anim->EquipmentWaterAlpha,*P->GetMesh()->GetSocketLocation(TEXT("head")).ToString(),*P->FindComponentByClass<UDiveBreathingComponent>()->GetRegulatorLocation().ToString()));
   Test->AddInfo(P->FindComponentByClass<UDiveBreathingComponent>()->DescribeExhaust());
   if(const auto* E=P->FindComponentByClass<UDiveEquipmentComponent>())Test->AddInfo(FString::Printf(TEXT("equipment stage=%d spear=%d fired=%d hits=%d retrieved=%d DPV=%d spool=%.2f speed=%.2f location=%s"),Stage,int32(E->GetSpearPhase()),E->GetFiredCount(),E->GetHitCount(),E->GetRetrievedCount(),E->IsDPVDeployed(),E->GetMotorSpool(),P->GetVelocity().Size(),*P->GetActorLocation().ToString()));
   if(Stage==5 || Stage==8)
   {
    TArray<USkeletalMeshComponent*> Skins;P->GetComponents(Skins);
    for(auto* S:Skins)if(S!=P->GetMesh())
    {
     Test->AddInfo(FString::Printf(TEXT("FP skin=%s vis=%d tick=%d anim=%s extent=%s hand-camera=%s relative=%s"),*S->GetName(),S->IsVisible(),S->IsComponentTickEnabled(),*GetNameSafe(S->GetAnimInstance()),*S->Bounds.BoxExtent.ToString(),*P->GetFollowCamera()->GetComponentTransform().InverseTransformPosition(S->GetSocketLocation(TEXT("hand_l"))).ToString(),*S->GetRelativeTransform().ToString()));
     if(Stage==5 && !SkinAudit)
     {
      SkinAudit=true;TArray<FFinalSkinVertex> Vertices;S->GetCPUSkinnedVertices(Vertices,0);FBox Bounds(ForceInit);
      for(const auto& V:Vertices)Bounds+=P->GetFollowCamera()->GetComponentTransform().InverseTransformPosition(S->GetComponentTransform().TransformPosition(FVector(V.Position)));
      Test->AddInfo(FString::Printf(TEXT("FP skinned vertex camera bounds=%s count=%d ownerOnly=%d ownerHidden=%d boundsCamera=%s"),*Bounds.ToString(),Vertices.Num(),S->bOnlyOwnerSee,S->bOwnerNoSee,*P->GetFollowCamera()->GetComponentTransform().InverseTransformPosition(S->Bounds.Origin).ToString()));
     }
    }
   }
  }
  if(Stage==1 && Age>8)
  {
   Test->TestTrue(TEXT("Supported floor pose is active"),Swim->IsSeafloorStanding());
   Key(EKeys::Q,IE_Pressed);Stage=11;At=Now;
  }
  else if(Stage==11)
  {
   Key(EKeys::Q,IE_Released);
   if(Age>4)
   {
    Test->TestTrue(TEXT("Deploying a stopped DPV preserves seafloor support"),Swim->IsSeafloorStanding());
    if(auto* A=Cast<USwimmingAnimInstance>(P->GetMesh()->GetAnimInstance()))Test->TestEqual(TEXT("Ground DPV uses its standing hold"),A->OceanPose,17);
    PC->SetControlRotation(FRotator(-7,85,0));Stage=12;At=Now;
   }
  }
  else if(Stage==12)
  {
   P->AddMovementInput(FVector(1,0,0),1);
   if(Age>4)
   {
    if(auto* A=Cast<USwimmingAnimInstance>(P->GetMesh()->GetAnimInstance()))Test->TestEqual(TEXT("Ground DPV uses its walking hold"),A->OceanPose,18);
    Key(EKeys::Q,IE_Pressed);PC->SetControlRotation(FRotator(-7,155,0));Stage=13;At=Now;
   }
  }
  else if(Stage==13 && Age>1){Key(EKeys::Q,IE_Released);Stage=2;At=Now;}
  else if(Stage==2)
  {
   P->AddMovementInput(FVector(1,0,0),1);
   if(Age>5){Key(EKeys::SpaceBar,IE_Pressed);Stage=3;At=Now;}
  }
  else if(Stage==3)
  {
   if(Age>.08f)Key(EKeys::SpaceBar,IE_Released);
   if(Age>7){Key(EKeys::SpaceBar,IE_Pressed);Stage=4;At=Now;}
  }
  else if(Stage==4)
  {
   if(Age>1.6f)Key(EKeys::SpaceBar,IE_Released);
   P->AddMovementInput(FVector(1,0,0),1);
   if(Age>7){Dive->SetFirstPerson(true);PC->SetControlRotation(FRotator(0,180,0));Stage=5;At=Now;}
  }
  else if(Stage==5 && Age>6)
  {
   Key(EKeys::G,IE_Pressed);Key(EKeys::RightMouseButton,IE_Pressed);Stage=6;At=Now;
  }
  else if(Stage==6)
  {
   Key(EKeys::G,IE_Released);
   if(Age>2 && !FiredInput){Key(EKeys::LeftMouseButton,IE_Pressed);FiredInput=true;}
   if(Age>2.15f)Key(EKeys::LeftMouseButton,IE_Released);
   if(Age>4){Key(EKeys::RightMouseButton,IE_Released);Key(EKeys::R,IE_Pressed);Stage=7;At=Now;}
  }
  else if(Stage==7 && Age>13)
  {
   Key(EKeys::R,IE_Released);auto* E=P->FindComponentByClass<UDiveEquipmentComponent>();
   Test->TestEqual(TEXT("One physical shaft released"),E->GetFiredCount(),1);
   Test->TestTrue(TEXT("Shaft has completed visible recovery and reload"),E->GetSpearPhase()==ESpearPhase::Ready);
   Key(EKeys::Q,IE_Pressed);Stage=8;At=Now;
  }
  else if(Stage==8)
  {
   Key(EKeys::Q,IE_Released);if(Age>2 && !MotorInput){Key(EKeys::LeftMouseButton,IE_Pressed);MotorInput=true;}
   PeakDPVSpeed=FMath::Max(PeakDPVSpeed,P->GetVelocity().Size());
   if(Age>10){Key(EKeys::LeftMouseButton,IE_Released);Stage=9;At=Now;}
  }
  else if(Stage==9 && Age>6)
  {
   Test->TestTrue(TEXT("DPV produces meaningful powered travel"),PeakDPVSpeed>180);
   Test->TestTrue(TEXT("Release allows drag to slow the vehicle"),P->GetVelocity().Size()<PeakDPVSpeed*.7f);
   Dive->SetFirstPerson(false);Key(EKeys::LeftMouseButton,IE_Pressed);Stage=10;At=Now;
  }
  else if(Stage==10 && Age>6)
  {
   Key(EKeys::LeftMouseButton,IE_Released);
   Test->AddInfo(TEXT("Native motion/equipment diagnostic frames only; player acceptance remains open."));return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;double Start=0;float At=0,NextFrame=0,PeakDPVSpeed=0;int32 Stage=0,Frame=0;bool FiredInput=false,MotorInput=false,SkinAudit=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityDiveMotionSurvey,"NeonCity.OceanExperience.DiveMotionSurvey",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityDiveMotionSurvey::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FDiveMotionSurvey(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
namespace {
class FSpearCaptureReview : public IAutomationLatentCommand
{
public:
 explicit FSpearCaptureReview(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>210){Test->AddError(TEXT("Physical spear capture timed out"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  auto* Dive=P->FindComponentByClass<UDivingComponent>();auto* Gear=P->FindComponentByClass<UDiveEquipmentComponent>();
  auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
  const auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
  const float Now=W->GetTimeSeconds(),Age=Now-At;
  SpearEvidence.Record(W,PC,Specimen*10+Stage);
  if(Stage==0)
  {
   const FVector XY=NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-178000,0));auto* Region=ANeonCityCoastalRegion::Find(W);
   if(!Region || !Region->EnsureDestinationReady(XY))return false;
   if(!Test->TestTrue(TEXT("Capture starts with ordinary equipped suit"),Dive && Dive->Equip()))return true;
   FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(SpearCaptureFloor),false,P);
   if(!Test->TestTrue(TEXT("Capture uses a real sea floor"),W->LineTraceSingleByChannel(Hit,XY+FVector(0,0,-300),XY-FVector(0,0,10000),ECC_WorldStatic,Q)))return true;
   P->SetActorLocation(Hit.ImpactPoint+FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
   P->SetActorRotation(FRotator::ZeroRotator);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Swimming);
   Dive->SetFirstPerson(true);PC->SetControlRotation(FRotator::ZeroRotator);
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(11);}
   Owned=Inventory->GetFish().Num();Stage=1;At=Now;return false;
  }
  if(Stage==1 && Age>4)
  {
   const TCHAR* Names[]={TEXT("Sardine"),TEXT("Seabream"),TEXT("YellowfinTuna")};
   SpeciesName=Names[Specimen];Owned=Inventory->GetFish().Num();HitsBefore=Gear->GetHitCount();RetrievedBefore=Gear->GetRetrievedCount();
   SawFlight=SawRetrieval=ReleasedEarlyRetrieve=false;SeenDeath.Reset();NextDeathFrame=0;
   const FVector Centre=P->GetFollowCamera()->GetComponentLocation()+FVector(480,0,0);
   auto* Data=LoadObject<UFishData>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Fishing/Data/DA_Fish_%s"),*SpeciesName));
   if(!Test->TestNotNull(TEXT("Capture uses the actual native species"),Data))return true;
   const FTransform Spawn(FRotator::ZeroRotator,Centre);
   Target=W->SpawnActorDeferred<AMarineSchool>(AMarineSchool::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Target->Species=Data;Target->CreatureMesh=Data->Mesh.LoadSynchronous();Target->HabitatID=TEXT("SpearReviewRealSeafloor");
   Target->Behaviour=EMarineBehaviour::Hover;Target->CruiseSpeed=Specimen==0?8:18;Target->HomeRadius=260;Target->FloorClearance=35;Target->SwimAmplitude=.025f;
   Target->FloorResolution=5;Target->FloorExtent=600;Target->FloorHeights.Reset();
   FCollisionQueryParams Q(SCENE_QUERY_STAT(SpearReviewGrid),false,P);
   for(int32 Y=0;Y<5;++Y)for(int32 X=0;X<5;++X)
   {
    const FVector Point=Centre+FVector((X-2)*300,(Y-2)*300,0);FHitResult Hit;
    if(!Test->TestTrue(TEXT("Capture habitat follows actual terrain"),W->LineTraceSingleByChannel(Hit,Point+FVector(0,0,100),Point-FVector(0,0,1500),ECC_WorldStatic,Q)))return true;
    Target->FloorHeights.Add(Hit.ImpactPoint.Z-Centre.Z);
   }
   Target->Starts={FTransform(FRotator(0,90,0),FVector::ZeroVector)};Target->FinishSpawning(Spawn);
   if(!P->FindComponentByClass<UDiveEquipmentComponent>()->IsSpeargunDeployed())Key(EKeys::G,IE_Pressed);
   Key(EKeys::RightMouseButton,IE_Pressed);Stage=2;At=Now;return false;
  }
  if(Stage==2)
  {
   Key(EKeys::G,IE_Released);
   PC->SetControlRotation((Target->CreaturePosition(0)-P->GetFollowCamera()->GetComponentLocation()).Rotation());
   if(Age>3)
   {
    Test->TestTrue(TEXT("Target is still simulated before the shot"),Target->LivingCount()==1 && !Target->HasReservedCreatures());
    FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Spear-%s-Aim.png"),*SpeciesName));
    Key(EKeys::LeftMouseButton,IE_Pressed);Stage=3;At=Now;
   }
  }
  else if(Stage==3)
  {
   if(Age>.08f)Key(EKeys::LeftMouseButton,IE_Released);
   if(Gear->GetSpearPhase()==ESpearPhase::Flying && !SawFlight)
   {
    SawFlight=true;Test->TestEqual(TEXT("Flight does not grant inventory"),Inventory->GetFish().Num(),Owned);
    FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Spear-%s-Flight.png"),*SpeciesName));
   }
   if(Gear->GetHitCount()>HitsBefore)
   {
    Test->TestTrue(TEXT("A finite traveling shaft preceded contact"),SawFlight);
    Test->TestEqual(TEXT("Struck fish leaves the living school"),Target->LivingCount(),0);
    FTransform Visible;Target->Bodies->GetInstanceTransform(0,Visible,false);
    Test->TestTrue(TEXT("Struck fish is still visible at its original body size"),Visible.GetScale3D().X>.5f);
    Test->TestTrue(TEXT("Actual hit reserved the world instance"),Target->HasReservedCreatures());
    Test->TestEqual(TEXT("Hit is not an instant reward"),Inventory->GetFish().Num(),Owned);
    FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Spear-%s-Hit.png"),*SpeciesName));
    Key(EKeys::RightMouseButton,IE_Released);Key(EKeys::R,IE_Pressed);Stage=4;At=Now;
   }
   else if(Age>5){Test->AddError(FString::Printf(TEXT("Aimed physical shot missed/stopped: %s"),*Gear->GetStatus()));return true;}
  }
  else if(Stage==4)
  {
   Test->TestEqual(TEXT("Waiting after the hit still grants nothing"),Inventory->GetFish().Num(),Owned);
   if(Age<.65f)Test->TestTrue(TEXT("Early retrieve input cannot cancel the death reaction"),Gear->GetSpearPhase()==ESpearPhase::Tethered);
   else if(!ReleasedEarlyRetrieve){Key(EKeys::R,IE_Released);ReleasedEarlyRetrieve=true;}
   const auto Life=Target->GetSpearLife(0);SeenDeath.Add(uint8(Life));
   if(Age>=NextDeathFrame)
   {
    NextDeathFrame=Age+.12f;
    if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Spear-%s-Death-%03d.png"),*SpeciesName,DeathFrame++));
    Test->AddInfo(FString::Printf(TEXT("SPEAR_DEATH_FRAME species=%s age=%.3f state=%d location=%s"),*SpeciesName,Age,int32(Life),*Target->CreaturePosition(0).ToString()));
   }
   if(Age>4.5f)
   {
    Test->TestTrue(TEXT("Small/medium/large fish all reach collectible only after visible drift"),Life==EMarineSpearLife::Collectible && SeenDeath.Num()>=4);
    Key(EKeys::R,IE_Pressed);Stage=5;At=Now;
   }
  }
  else if(Stage==5)
  {
   if(Age>.65f && !SawRetrieval)
   {
    Test->AddInfo(FString::Printf(TEXT("Retrieve input: R=%d firstPerson=%d phase=%d"),PC->IsInputKeyDown(EKeys::R),Dive->IsFirstPerson(),int32(Gear->GetSpearPhase())));
    SawRetrieval=true;FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Spear-%s-Retrieve.png"),*SpeciesName));
    Test->TestTrue(TEXT("Player must haul back the physical line"),Gear->GetSpearPhase()==ESpearPhase::Retrieving);
   }
   if(Gear->GetRetrievedCount()==RetrievedBefore+1 && Gear->GetSpearPhase()==ESpearPhase::Ready)
   {
    Key(EKeys::R,IE_Released);Test->TestEqual(TEXT("Complete recovery grants exactly one fish"),Inventory->GetFish().Num(),Owned+1);
    Test->TestEqual(TEXT("Recovered specimen leaves the visible school once"),Target->LivingCount(),0);
    Test->TestEqual(TEXT("Recovered species is the struck fish"),Inventory->GetFish().Last().SpeciesID,Target->Species->SpeciesID);
    FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Spear-%s-Collected.png"),*SpeciesName));
    Target->Destroy();Target=nullptr;++Specimen;
    if(Specimen<3){Stage=1;At=Now;return false;}
    Test->AddInfo(TEXT("Three real species: native projectile/death/drift/retrieval diagnostics; user playthrough remains required."));return true;
   }
   if(Age>15){Test->AddError(FString::Printf(TEXT("Retrieval did not finish: %s"),*Gear->GetStatus()));return true;}
  }
  return false;
 }
private:
 FAutomationTestBase* Test;AMarineSchool* Target=nullptr;double Started=0;float At=0,NextDeathFrame=0;int32 Stage=0,Owned=0,Specimen=0,HitsBefore=0,RetrievedBefore=0,DeathFrame=0;bool SawFlight=false,SawRetrieval=false,ReleasedEarlyRetrieve=false;
 FString SpeciesName;TSet<uint8> SeenDeath;
 FNeonCityVisualEvidence SpearEvidence{TEXT("SpearDeath")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCitySpearCaptureReview,"NeonCity.OceanExperience.SpearCapturePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCitySpearCaptureReview::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FSpearCaptureReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
