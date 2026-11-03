#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Interaction/InteractionComponent.h"
#include "InputCoreTypes.h"
#include "Combat/HealthComponent.h"
#include "Vehicles/Marine/NeonCityLamborghini63.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "Engine/OverlapResult.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace NeonCityWestLakeYachtBerth
{
/** The Lamborghini 63 at her Hubin berth, approached the way a player does: on foot along the promenade from every
 * side (F must board wherever one stands beside her, and one can also walk / jump down onto her decks), and by
 * helicopter, set down on the promenade lawns and paving right beside her berth. */
class FBerth:public IAutomationLatentCommand
{
public:
 explicit FBerth(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>400){Test->AddError(FString::Printf(TEXT("Yacht berth check timed out at stage %d"),Stage));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<6)return false;
  if(!Boat.IsValid())
  {
   Me=Cast<ACharacter>(PC->GetPawn());
   for(TActorIterator<ANeonCityLamborghini63> It(W);It;++It)if(!It->ActorHasTag(TEXT("WL.Leisure"))){Boat=*It;break;}      // the player's own 63, not the boat club's
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It){Heli=*It;break;}
   if(!Test->TestNotNull(TEXT("the Lamborghini 63"),Boat.Get()) || !Test->TestNotNull(TEXT("the player on foot"),Me.Get()))return true;
   Test->AddInfo(FString::Printf(TEXT("yacht at %s yaw %.1f v %.0f owned %d health %.0f/%.0f dead %d driver %s controller %s; player %s"),
    *Boat->GetActorLocation().ToCompactString(),Boat->GetActorRotation().Yaw,Boat->GetVelocity().Size(),Boat->Owned,Boat->GetHealthComponent()->GetCurrentHealth(),
    Boat->GetHealthComponent()->GetMaxHealth(),Boat->GetHealthComponent()->IsDead(),*GetNameSafe(Boat->GetMarineInteraction()->GetDriver()),*GetNameSafe(Boat->GetController()),
    *Me->GetActorLocation().ToCompactString()));
   Shore=ShoreSpots(W);
   Test->AddInfo(FString::Printf(TEXT("%d dry spots beside the berth"),Shore.Num()));
  }
  ACharacter* P=Me.Get();const double E=FPlatformTime::Seconds()-At;const FTransform X=Boat->GetActorTransform();
  auto* M=P->GetCharacterMovement();const float Half=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  auto OnBoat=[&](){return P->GetMovementBase() && P->GetMovementBase()->GetOwner()==Boat.Get();};
  switch(Stage)
  {
  case 0:
   // F from every dry spot within a few metres of her side
   if(Probe<Shore.Num())
   {
    if(Sub==0){Put(P,Shore[Probe].Get<0>());Sub=1;return false;}
    if(++Sub<4)return false;
    auto* I=P->FindComponentByClass<UInteractionComponent>();AActor* F=I?I->RefreshFocusedInteractable():nullptr;
    const auto Action=Boat->GetActionFor(P);const FVector L=X.InverseTransformPosition(P->GetActorLocation());
    const bool bOk=F==Boat.Get() && Action==ENeonCityYachtAction::Board;
    if(!bOk || Shore[Probe].Get<1>()<350)Test->AddInfo(FString::Printf(TEXT("  shore %d local %s gap %.0f: focus %s action %d mode %d"),Probe,*L.ToCompactString(),Shore[Probe].Get<1>(),*GetNameSafe(F),int32(Action),int32(M->MovementMode.GetValue())));
    if(Shore[Probe].Get<1>()<350){++Near;NearOk+=bOk;}
    ++Probe;Sub=0;return false;
   }
   Test->TestTrue(FString::Printf(TEXT("F boards from every spot within 3.5 m of her side (%d of %d)"),NearOk,Near),Near>0 && NearOk==Near);
   // the nearest spot to her side amidships: press F for real
   Best=0;for(int32 K=1;K<Shore.Num();++K)if(Score(Shore[K])<Score(Shore[Best]))Best=K;
   if(Shore.Num())Put(P,Shore[Best].Get<0>());
   return Next(1);
  case 1:
   if(E<1.5)return false;
   PC->SetControlRotation(FRotator(25,PC->GetControlRotation().Yaw+120,0));
   Key(PC,EKeys::F);return Next(2);
  case 2:
   if(E<1.5)return false;
   Test->TestTrue(FString::Printf(TEXT("F on the promenade puts one aboard (feet local %s)"),*X.InverseTransformPosition(P->GetActorLocation()-FVector(0,0,Half)).ToCompactString()),OnBoat());
   // walk aboard over the passerelle: from the quay a couple of metres short of its foot, up it to the platform
   if(!Test->TestTrue(TEXT("the passerelle is out to the quay"),Boat->IsGangwayOut()))return Next(4);
   {
    const FVector Dir=Boat->Gangway->GetForwardVector();const FVector Foot=Boat->Gangway->GetComponentLocation()+Dir*Boat->Gangway->GetComponentScale().X*100.f;
    FHitResult G;FCollisionQueryParams Q(SCENE_QUERY_STAT(BerthFoot),false,P);Q.AddIgnoredActor(Boat.Get());const FVector From=Foot+Dir.GetSafeNormal2D()*150;
    Test->AddInfo(FString::Printf(TEXT("passerelle %.0f cm long, pitch %.1f"),Boat->Gangway->GetComponentScale().X*100.f,Boat->Gangway->GetComponentRotation().Pitch));
    if(W->LineTraceSingleByChannel(G,From+FVector(0,0,300),From-FVector(0,0,300),ECC_Pawn,Q))Put(P,G.ImpactPoint);
   }
   return Next(3);
  case 3:
   {
    if(E<1)return false;
    const FVector Hinge=Boat->Gangway->GetComponentLocation();
    const FVector To=E<4.2?Hinge:X.TransformPosition(FVector(-900,0,80));             // up the plank, then up the tiers
    PC->SetControlRotation(FRotator(0,(To-P->GetActorLocation()).Rotation().Yaw,0));
    P->AddMovementInput((To-P->GetActorLocation()).GetSafeNormal2D(),1.f);
    if(E>1.3 && M->IsFalling())Fell+=W->GetDeltaSeconds();
    if(E<6)return false;
    const FVector L=X.InverseTransformPosition(P->GetActorLocation()-FVector(0,0,Half));
    Test->TestTrue(FString::Printf(TEXT("walked over the passerelle onto her (feet local %s, base %s, mode %d, %.2f s falling)"),*L.ToCompactString(),
     *GetNameSafe(P->GetMovementBase()),int32(M->MovementMode.GetValue()),Fell),OnBoat() && L.Z>40 && Fell<.8f);
    // into the lake amidships off her open-water side: F climbs out at the platform
    Put(P,X.TransformPosition(FVector(0,450,-60)));
    return Next(31);
   }
  case 31:
   if(E<2.5)return false;
   Test->TestEqual(FString::Printf(TEXT("swimming alongside amidships, F climbs aboard (mode %d)"),int32(M->MovementMode.GetValue())),int32(Boat->GetActionFor(P)),int32(ENeonCityYachtAction::ClimbAboard));
   Key(PC,EKeys::F);return Next(32);
  case 32:
   if(E<1.5)return false;
   Test->TestTrue(TEXT("out of the water onto the swim platform"),OnBoat());
   return Next(4);
  case 4:
   // the helicopter on the promenade beside her
   if(!Heli.IsValid())return true;
   if(Land==0){Pads=PadSpots(W);Test->AddInfo(FString::Printf(TEXT("%d helicopter spots beside the berth"),Pads.Num()));}
   if(Land>=Pads.Num())
   {
    Test->TestTrue(FString::Printf(TEXT("helicopter grounded beside the yacht on %d of %d spots"),Landed,Pads.Num()),Pads.Num()>0 && Landed==Pads.Num());
    return true;
   }
   {
    auto* Move=Heli->GetAircraftMovement();auto* Hull=Heli->GetHull();
    if(!bPlaced)
    {
     Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);Hull->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
     Heli->SetActorLocationAndRotation(Pads[Land]+FVector(0,0,350),FRotator(0,Boat->GetActorRotation().Yaw+Land*40,0),false,nullptr,ETeleportType::TeleportPhysics);
     Move->StartEngine();bPlaced=true;At=FPlatformTime::Seconds();Rest=0;return false;
    }
    Move->SetInput(FVector(0,0,-1),0);
    Rest=Hull->GetPhysicsLinearVelocity().Size()<15?Rest+W->GetDeltaSeconds():0;
    const bool bDown=Move->IsGrounded() && E>1 && (Land!=PostPad || Hull->GetComponentLocation().Z<PostTop);   // beside the post, not on it
    if(!bDown && E<(Land==PostPad?25:12) && (Rest<3 || Land==PostPad))return false;
    Landed+=bDown;
    const FVector Ctr=Hull->GetComponentLocation();FHitResult F;FCollisionQueryParams Q(SCENE_QUERY_STAT(BerthPad),false,Heli.Get());
    W->LineTraceSingleByChannel(F,Ctr,Ctr-FVector(0,0,2000),ECC_WorldStatic,Q);
    Test->AddInfo(FString::Printf(TEXT("  pad %d%s %s at local %s: ground %s/%s n.z %.2f gap %.0f up.z %.2f v %.0f alt %.0f state %d"),Land,Land==PostPad?TEXT(" (onto a lamp post)"):TEXT(""),bDown?TEXT("GROUNDED"):TEXT("NOT GROUNDED"),
     *X.InverseTransformPosition(Pads[Land]).ToCompactString(),*GetNameSafe(F.GetActor()),*GetNameSafe(F.GetComponent()),F.ImpactNormal.Z,F.bBlockingHit?float(Ctr.Z-F.ImpactPoint.Z):-1.f,
     Hull->GetUpVector().Z,Hull->GetPhysicsLinearVelocity().Size(),Move->GetAltitude(),int32(Move->GetFlightState())));
    if(!bDown)
    {
     TArray<UPrimitiveComponent*> Parts;Heli->GetComponents(Parts);
     for(auto* C:Parts)if(C->IsCollisionEnabled())Test->AddInfo(FString::Printf(TEXT("     heli part %s %s z %.0f..%.0f block world %d slip %s %.1f rest-ish v %.0f"),*C->GetName(),*C->GetClass()->GetName(),
      C->Bounds.GetBox().Min.Z,C->Bounds.GetBox().Max.Z,int32(C->GetCollisionResponseToChannel(ECC_WorldStatic)),*Move->GetSlipDir().ToCompactString(),Move->GetSlipTime(),Hull->GetPhysicsLinearVelocity().Size()));
     TArray<FOverlapResult> Under;const FVector Ext=Heli->GetProfile()->BodyExtent;
     W->OverlapMultiByObjectType(Under,Ctr-FVector(0,0,Ext.Z+20),Hull->GetComponentQuat(),FCollisionObjectQueryParams::AllObjects,FCollisionShape::MakeBox(FVector(Ext.X,Ext.Y,40)),Q);
     for(const auto& O:Under)
     {
      const auto* SM=Cast<UStaticMeshComponent>(O.GetComponent());FBox Box(ForceInit);
      if(const auto* IS=Cast<UInstancedStaticMeshComponent>(O.GetComponent());IS && O.ItemIndex>=0){FTransform T;IS->GetInstanceTransform(O.ItemIndex,T,true);Box=IS->GetStaticMesh()->GetBoundingBox().TransformBy(T);}
      Test->AddInfo(FString::Printf(TEXT("     under: %s/%s mesh %s item %d box %s..%s (belly z %.0f) angular %.2f"),*GetNameSafe(O.GetActor()),*GetNameSafe(O.GetComponent()),
       SM?*GetNameSafe(SM->GetStaticMesh()):TEXT("-"),O.ItemIndex,*Box.Min.ToCompactString(),*Box.Max.ToCompactString(),Ctr.Z-Heli->GetProfile()->BodyExtent.Z,Hull->GetPhysicsAngularVelocityInRadians().Size()));
     }
    }
    ++Land;bPlaced=false;return false;
   }
  default:return true;
  }
 }
private:
 typedef TTuple<FVector,float> FSpot;                  // ground point, gap to her side (cm)
 static float Score(const FSpot& S){return S.Get<1>();}
 bool Next(int32 N){Stage=N;At=FPlatformTime::Seconds();return false;}
 static void Key(APlayerController* PC,FKey K)
 {
  PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,IE_Pressed,FPlatformTime::Cycles64()));
  PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,IE_Released,FPlatformTime::Cycles64()));
 }
 void Put(ACharacter* P,const FVector& Ground)
 {
  P->GetCharacterMovement()->StopMovementImmediately();
  P->SetActorLocation(Ground+FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
  P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
 }
 bool Dry(UWorld* W,const FVector& G) const
 {
  float WaterZ;FVector Cur;auto* Marine=W->GetSubsystem<UMarineWorldSubsystem>();
  return !(Marine && Marine->SampleWater(G,WaterZ,Cur) && WaterZ>G.Z-20);
 }
 TArray<FSpot> ShoreSpots(UWorld* W) const
 {
  // a ring of points 1-6 m off her hull box, both sides, bow and stern; keep walkable dry ground
  TArray<FSpot> Out;const FTransform X=Boat->GetActorTransform();const FVector Ext=Boat->GetProfile()->HullExtent;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(BerthShore),false,Me.Get());Q.AddIgnoredActor(Boat.Get());
  for(float Gap:{100.f,200.f,300.f,450.f,600.f})for(float A=-1;A<=1.01f;A+=.25f)for(float S:{1.f,-1.f})
  {
   const FVector L(A*Ext.X,S*(Ext.Y+Gap),0);FHitResult H;const FVector P=X.TransformPosition(L);
   if(!W->LineTraceSingleByChannel(H,P+FVector(0,0,800),P-FVector(0,0,600),ECC_Pawn,Q) || H.ImpactNormal.Z<.7f || !Dry(W,H.ImpactPoint))continue;
   Out.Add(FSpot(H.ImpactPoint,Gap));
  }
  for(float Gap:{150.f,300.f})for(float S:{1.f,-1.f})
  {
   const FVector P=X.TransformPosition(FVector(S*(Ext.X+Gap),0,0));FHitResult H;
   if(W->LineTraceSingleByChannel(H,P+FVector(0,0,800),P-FVector(0,0,600),ECC_Pawn,Q) && H.ImpactNormal.Z>.7f && Dry(W,H.ImpactPoint))Out.Add(FSpot(H.ImpactPoint,Gap));
  }
  return Out;
 }
 TArray<FVector> PadSpots(UWorld* W) const
 {
  // dry, flat-ish ground 8-35 m from her, the clear spots a pilot would pick to set down beside her
  TArray<FVector> Out;const FVector C=Boat->GetActorLocation();
  FCollisionQueryParams Q(SCENE_QUERY_STAT(BerthPads),false,Heli.Get());Q.AddIgnoredActor(Boat.Get());
  for(float R:{650.f,900.f,1200.f,1600.f,2200.f,3000.f})for(int32 K=0;K<16 && Out.Num()<20;++K)
  {
   const FVector P=C+FVector(FMath::Cos(K*PI/8),FMath::Sin(K*PI/8),0)*R;FHitResult H;
   if(!W->LineTraceSingleByChannel(H,P+FVector(0,0,6000),P-FVector(0,0,3000),ECC_WorldStatic,Q) || H.ImpactNormal.Z<.9f || !Dry(W,H.ImpactPoint))continue;
   // rotor room: no tree or lamp crown within 4 m (benches, bollards and kerbs are where a pilot does set down)
   FHitResult Block;
   if(W->SweepSingleByChannel(Block,H.ImpactPoint+FVector(0,0,1500),H.ImpactPoint+FVector(0,0,900),FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(400),Q))continue;
   if(Out.ContainsByPredicate([&](const FVector& S){return FVector::Dist2D(S,H.ImpactPoint)<450;}))continue;
   // and a clear column for the airframe itself where it is dropped (not inside a lamp post)
   const FVector Ext=Heli->GetProfile()->BodyExtent;
   if(W->OverlapBlockingTestByChannel(H.ImpactPoint+FVector(0,0,350),FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(FMath::Max(Ext.X,Ext.Y)+50),Q))continue;
   Out.Add(H.ImpactPoint);
  }
  // last: let it down onto the head of the quay lamp nearest her - it must hop off and settle beside the post
  TArray<FOverlapResult> Around;
  W->OverlapMultiByObjectType(Around,C,FQuat::Identity,FCollisionObjectQueryParams::AllObjects,FCollisionShape::MakeSphere(2500),Q);
  float Closest=1e9;FVector Post=FVector::ZeroVector;
  for(const auto& O:Around)if(const auto* IS=Cast<UInstancedStaticMeshComponent>(O.GetComponent());IS && O.ItemIndex>=0 && GetNameSafe(IS->GetStaticMesh()).Contains(TEXT("Lamp")))
  {
   FTransform T;IS->GetInstanceTransform(O.ItemIndex,T,true);const FBox Box=IS->GetStaticMesh()->GetBoundingBox().TransformBy(T);
   if(FVector::Dist2D(Box.GetCenter(),C)<Closest){Closest=FVector::Dist2D(Box.GetCenter(),C);Post=FVector(Box.GetCenter().X,Box.GetCenter().Y,Box.Max.Z);}
  }
  if(!Post.IsZero()){PostPad=Out.Num();PostTop=Post.Z;Out.Add(Post+FVector(0,0,Heli->GetProfile()->BodyExtent.Z+150-350));}
  return Out;
 }
 mutable int32 PostPad=-1;mutable float PostTop=0;
 FAutomationTestBase* Test;double Started=0,At=0;int32 Stage=0,Probe=0,Sub=0,Near=0,NearOk=0,Best=0,Land=0,Landed=0;bool bPlaced=false;float Rest=0,Fell=0;
 TWeakObjectPtr<ANeonCityLamborghini63> Boat;TWeakObjectPtr<ANeonCityHelicopterPrototype> Heli;TWeakObjectPtr<ACharacter> Me;
 TArray<FSpot> Shore;TArray<FVector> Pads;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeYachtBerthPIE,"NeonCity.WestLake.YachtBerthPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeYachtBerthPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeYachtBerth::FBerth(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
