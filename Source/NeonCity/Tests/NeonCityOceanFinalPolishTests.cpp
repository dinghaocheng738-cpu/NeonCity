#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Adventure/MarineSpearDeath.h"
#include "Fishing/FishingSessionModel.h"
#include "Animation/AnimSequence.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Adventure/DivingComponent.h"
#include "Water/SwimmingComponent.h"
#include "Water/SwimmingAnimInstance.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarineDeathMotionRules,"NeonCity.OceanExperience.SpearDeathRules",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMarineDeathMotionRules::RunTest(const FString&)
{
 for(float Length:{18.f,48.f,155.f})
 {
  FVector Final[2];int32 Case=0;
  for(float Dt:{1.f/30,1.f/120})
  {
   FMarineSpearDeath D;FVector P(0,0,-500);FQuat Q=FQuat::Identity;
   D.Begin(0,P,Q,FVector(50,0,0),P+FVector(0,3,0),FVector(0,1,0),Length,7);
   TestTrue(TEXT("A hit has a visible, non-collectible reaction"),D.State==EMarineSpearLife::HitReaction && !D.Collectible());
   TSet<uint8> Seen;float MaxStep=0;double Now=0;
   for(int32 I=0;I<FMath::RoundToInt(8/Dt);++I)
   {
    const FVector Previous=P;Now+=Dt;D.Step(Now,Dt,FVector(4,2,0),-1000,-130,P,Q);Seen.Add(uint8(D.State));
    MaxStep=FMath::Max(MaxStep,float(FVector::Dist(P,Previous)));
    const FTransform A=D.Attachment(P,Q);
    TestTrue(TEXT("Embedded spear keeps its local penetration during body roll"),A.GetLocation().Equals(P+Q.RotateVector(D.HitOffset),.001));
    if(Now<2.5)TestFalse(TEXT("Holding retrieve cannot bypass death motion"),D.Collectible());
   }
   TestEqual(TEXT("All five post-hit stages are observable"),Seen.Num(),5);
   TestTrue(TEXT("Death finishes with a visible, still collectible body"),D.Collectible() && !D.Expired(100,false));
   TestTrue(TEXT("Water drag prevents stone-like falling"),P.Z>-600 && P.Z<-500 && FMath::Abs(D.Velocity.Z)<20);
   TestTrue(TEXT("Current causes lateral drift"),P.Y>5);
   TestTrue(TEXT("Tail settles instead of continuing its swim cycle"),D.TailStrength==0);
   TestTrue(TEXT("Finite integration has no teleport"),MaxStep<6);
   TestFalse(TEXT("Attached fish is not cleaned up because player moved away"),D.Expired(6000,true));
   D.Age=15;TestTrue(TEXT("Uncollected distant corpse is safely retired"),D.Expired(6000,false));
   D.Age=91;TestTrue(TEXT("Orphaned reservations cannot retain corpses forever"),D.Expired(100,true));
   Final[Case++]=P;
   const float PreviousAge=D.Age;
   D.Attach(P,Q,P+FVector(0,0,2),FVector(0,-1,0),Length);
   TestTrue(TEXT("Reattaching a dropped corpse keeps its death state and uses the new penetration"),D.Collectible() && D.Age==PreviousAge && D.Attachment(P,Q).GetLocation().Equals(P+FVector(0,0,2),.001));
  }
  TestTrue(TEXT("30 and 120 Hz preserve the same slow drift"),FVector::Distance(Final[0],Final[1])<3);
 }
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastMotionClock,"NeonCity.Fishing.CastAnimationClock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCastMotionClock::RunTest(const FString&)
{
 auto* Back=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/Fishing/R2/Motion/A_FishingR2_Windup"));
 auto* Forward=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/Fishing/R2/Motion/A_FishingR2_Cast"));
 if(!TestNotNull(TEXT("Authored back swing"),Back) || !TestNotNull(TEXT("Authored forward cast"),Forward))return false;
 auto Curve=[](const UAnimSequence* Clip,FName Name,float Time){return Clip->EvaluateCurveData(Name,FAnimExtractContext(Time));};
 TestTrue(TEXT("Back swing includes shoulder loading and weight shift before the timing window"),Curve(Back,TEXT("Body"),1.1f)>.9f && Curve(Back,TEXT("SeatX"),1.1f)<17);
 FNeonCityFishingSession S;S.Reset(42,1);S.Press();
 for(int32 I=0;I<25;++I)S.Step(.05f);
 TestTrue(TEXT("Loaded pose owns the ready marker"),Curve(Back,TEXT("CastReady"),S.Elapsed)>.99f);
 S.Release(Curve(Back,TEXT("CastReady"),S.Elapsed)>.5f);
 TestTrue(TEXT("The real animation marker grants the perfect result"),S.PerfectCast);
 TestFalse(TEXT("The rod has not released line at the beginning of its forward stroke"),Curve(Forward,TEXT("LineReleased"),.15f)>.5f);
 const float Release=Curve(Back,TEXT("ReleaseSeconds"),.9f);
 AddInfo(FString::Printf(TEXT("Release marker=%.9f flag=%.9f pitch=%.9f nextflag=%.9f"),Release,Curve(Forward,TEXT("LineReleased"),Release),Curve(Forward,TEXT("RodPitch"),Release),Curve(Forward,TEXT("LineReleased"),Release+.001f)));
 TestTrue(TEXT("Line release belongs to a forward-facing rod"),Curve(Forward,TEXT("LineReleased"),Release)>.99f && Curve(Forward,TEXT("RodPitch"),Release)>20 && Curve(Forward,TEXT("RodPitch"),Release)<60);
 TestTrue(TEXT("Follow-through precedes return to ready"),Curve(Forward,TEXT("Body"),.5f)<-.6f && FMath::Abs(Curve(Forward,TEXT("Body"),1.3f))<.01f);
 for(const auto Name:{TEXT("RodPitch"),TEXT("RodYaw"),TEXT("SeatX"),TEXT("SeatY"),TEXT("SeatZ"),TEXT("Brace")})
  TestTrue(FString(Name)+TEXT(" remains continuous at the backswing/cast boundary"),FMath::IsNearlyEqual(Curve(Back,Name,float(Back->GetPlayLength())*FNeonCityFishingSession::PerfectLow),Curve(Forward,Name,0),.05f));
 S.Reset(42,1);S.Press();for(int32 I=0;I<25;++I)S.Step(.05f);S.Release(false);
 TestFalse(TEXT("Power alone cannot grant perfect timing without the animation marker"),S.PerfectCast);
 return !HasAnyErrors();
}
namespace
{
class FSurfaceTreadReview : public IAutomationLatentCommand
{
public:
 explicit FSurfaceTreadReview(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>100){Test->AddError(TEXT("Surface transition review timed out"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  auto* Dive=P->FindComponentByClass<UDivingComponent>();auto* Swim=P->FindComponentByClass<USwimmingComponent>();
  auto* Anim=Cast<USwimmingAnimInstance>(P->GetMesh()->GetAnimInstance());
  const auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
  const float Now=W->GetTimeSeconds(),Age=Now-At;
  Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   const FVector Site=NeonCityCoastalLayout::FromLegacyIsland(FVector(57500,-179000,0));auto* Region=ANeonCityCoastalRegion::Find(W);
   if(!Region || !Region->EnsureDestinationReady(Site))return false;
   FWaterSurfaceSample Water;if(!Test->TestTrue(TEXT("Surface review uses existing ocean"),W->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Site,Water)))return true;
   if(!Test->TestTrue(TEXT("Actual R05 equipment is worn"),Dive && Dive->Equip()))return true;
   P->SetActorLocation(Water.SurfacePoint-FVector(0,0,330),false,nullptr,ETeleportType::TeleportPhysics);
   P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Swimming);P->SetActorRotation(FRotator::ZeroRotator);
   P->GetCameraBoom()->TargetArmLength=300;P->GetCameraBoom()->SocketOffset=FVector(0,10,15);PC->SetControlRotation(FRotator(-8,135,0));
   Key(EKeys::C,IE_Pressed);Stage=1;At=Now;return false;
  }
  if(Age>=NextFrame)
  {
   NextFrame=Age+.3f;
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Surface-R3-%d-%03d.png"),Stage,Frame++));
   if(Anim)Test->AddInfo(FString::Printf(TEXT("SURFACE_FRAME stage=%d age=%.3f pose=%d head=%s"),Stage,Age,Anim->OceanPose,*P->GetMesh()->GetSocketLocation(TEXT("head")).ToString()));
  if(Stage==3 && Anim)
   {
    Test->AddInfo(FString::Printf(TEXT("SURFACE_WEIGHTS equipment=%.3f"),Anim->EquipmentWaterAlpha));
    for(const FName Bone:{FName(TEXT("upperarm_l")),FName(TEXT("lowerarm_l")),FName(TEXT("hand_l")),FName(TEXT("upperarm_r")),FName(TEXT("lowerarm_r")),FName(TEXT("hand_r"))})
     Test->AddInfo(FString::Printf(TEXT("SURFACE_BONE %s %s"),*Bone.ToString(),*P->GetMesh()->GetSocketTransform(Bone,RTS_Component).GetLocation().ToString()));
   }
  }
  const auto Next=[&](int32 S){Stage=S;At=Now;NextFrame=0;};
  if(Stage==1 && Age>2){Key(EKeys::C,IE_Released);Key(EKeys::SpaceBar,IE_Pressed);Next(2);}
  else if(Stage==2 && Age>2 && Swim->IsFollowingSurface())
  {Key(EKeys::SpaceBar,IE_Released);Next(3);}
  else if(Stage==3 && Age>6)
  {
   Test->TestTrue(TEXT("No-input surface floats use the dedicated treading state"),Anim && Anim->OceanPose==0);
   for(const FString Side:{FString(TEXT("l")),FString(TEXT("r"))})
   {
    const auto Bone=[&](FString Prefix){return P->GetMesh()->GetSocketTransform(FName(Prefix+Side),RTS_Component).GetLocation();};
    Test->TestTrue(TEXT("Sculling elbows relax below the shoulders"),Bone(TEXT("upperarm_")).Z-Bone(TEXT("lowerarm_")).Z>16);
    Test->TestTrue(TEXT("Sculling wrists remain outside the torso"),FMath::Abs(Bone(TEXT("hand_")).X)>24);
   }
   FWaterSurfaceSample Water;W->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(P->GetActorLocation(),Water);
   Test->TestTrue(TEXT("Surface head stays near or above the local waterline"),P->GetMesh()->GetSocketLocation(TEXT("head")).Z>Water.SurfacePoint.Z-15);
   Key(EKeys::W,IE_Pressed);Next(4);
  }
  else if(Stage==4 && Age>4)
  {Test->TestTrue(TEXT("Intentional swimming transitions out of treading"),Anim && Anim->OceanPose==1);Key(EKeys::W,IE_Released);PC->SetControlRotation(FRotator(-8,55,0));Next(5);}
  else if(Stage==5 && Age>6)
  {Test->TestTrue(TEXT("Stopping returns to treading despite wave drift"),Anim && Anim->OceanPose==0);Key(EKeys::C,IE_Pressed);Next(6);}
  else if(Stage==6 && Age>3)
  {
   Key(EKeys::C,IE_Released);Test->TestTrue(TEXT("Explicit dive exits the surface state"),Swim->HasDiveIntent() && Anim && Anim->OceanPose!=0 && Anim->OceanPose!=1);
   Test->AddInfo(TEXT("Actual PIE transitions and equipment frames recorded; naturalness remains a visual/player judgement."));return true;
  }
  return false;
 }
 FAutomationTestBase* Test;double Started=0;float At=0,NextFrame=0;int32 Stage=0,Frame=0;
 FNeonCityVisualEvidence Evidence{TEXT("SurfaceTread")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceTreadPIE,"NeonCity.OceanExperience.SurfaceTreadPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSurfaceTreadPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FSurfaceTreadReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
