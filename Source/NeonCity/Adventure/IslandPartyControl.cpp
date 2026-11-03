#include "Adventure/IslandPartyControl.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "Property/PropertySubsystem.h"
#include "Presentation/CharacterAppearanceComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

namespace PartyDiagnostics
{
FString Cell(FString Value){Value.ReplaceInline(TEXT("\""),TEXT("\"\""));return TEXT("\"")+Value+TEXT("\"");}
FString XYZ(const FVector& P){return FString::Printf(TEXT("%.3f,%.3f,%.3f"),P.X,P.Y,P.Z);}
FString Identity(const FHitResult& Hit)
{
 const auto* C=Hit.GetComponent();const auto* M=Cast<UStaticMeshComponent>(C);
 return Cell(GetNameSafe(Hit.GetActor()))+TEXT(",")+Cell(GetNameSafe(C))+TEXT(",")+Cell(M?GetPathNameSafe(M->GetStaticMesh()):TEXT(""));
}
void Row(FString& CSV,const FString& Prefix,const TCHAR* Query,const FVector& From,const FVector& To,bool Blocked,const FHitResult& Hit)
{
 CSV+=Prefix+TEXT(",")+Query+TEXT(",")+XYZ(From)+TEXT(",")+XYZ(To)+FString::Printf(TEXT(",%d,%d,"),Blocked,Hit.bStartPenetrating)+Identity(Hit)+TEXT(",")+XYZ(Hit.ImpactPoint)+FString::Printf(TEXT(",%.4f,%.4f\n"),Hit.ImpactNormal.Z,Hit.PenetrationDepth);
}
void Overlaps(UWorld* World,FString& CSV,const FString& Prefix,const TCHAR* Query,const FVector& At,const FCollisionShape& Shape,const FCollisionQueryParams& Q)
{
 TArray<FOverlapResult> Hits;World->OverlapMultiByChannel(Hits,At,FQuat::Identity,ECC_Pawn,Shape,Q);bool Any=false;
 for(const auto& O:Hits)if(O.bBlockingHit){Any=true;FHitResult H;H.bBlockingHit=true;H.bStartPenetrating=true;H.Component=O.Component;H.HitObjectHandle=O.OverlapObjectHandle;Row(CSV,Prefix,Query,At,At,true,H);}
 if(!Any)Row(CSV,Prefix,Query,At,At,false,FHitResult());
}
}

AIslandPartyControl::AIslandPartyControl()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PartyHost")));
 Panel=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InvitationStand"));Panel->SetupAttachment(RootComponent);Panel->SetCollisionProfileName(TEXT("BlockAll"));Panel->SetRelativeScale3D(FVector(.35));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Stand(TEXT("/Game/NeonCity/Art/IslandLandPass1/Meshes/SM_LandWayfindingPanel.SM_LandWayfindingPanel"));if(Stand.Succeeded())Panel->SetStaticMesh(Stand.Object);
 Interaction=CreateDefaultSubobject<UBoxComponent>(TEXT("InvitationPanel"));Interaction->SetupAttachment(RootComponent);
 Interaction->SetBoxExtent(FVector(22,30,22));Interaction->SetRelativeLocation(FVector(0,0,110));
 Interaction->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Interaction->SetCollisionResponseToAllChannels(ECR_Ignore);Interaction->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Music=CreateDefaultSubobject<UAudioComponent>(TEXT("DeckMusic"));Music->SetupAttachment(RootComponent);Music->bAutoActivate=false;Music->bAllowSpatialization=true;
 Music->bOverrideAttenuation=true;Music->AttenuationOverrides.bAttenuate=true;Music->AttenuationOverrides.bSpatialize=true;Music->AttenuationOverrides.AttenuationShape=EAttenuationShape::Sphere;
 Music->AttenuationOverrides.AttenuationShapeExtents=FVector(350);Music->AttenuationOverrides.FalloffDistance=1800;Music->SetVolumeMultiplier(.24f);
 PrimaryActorTick.bCanEverTick=true;
}
void AIslandPartyControl::BeginPlay(){Super::BeginPlay();if(auto* Sound=PartyMusic.LoadSynchronous())Music->SetSound(Sound);}
bool AIslandPartyControl::CanInteract_Implementation(AActor* Visitor)const
{
 const auto* Pawn=Cast<APawn>(Visitor);const auto* C=Cast<ACharacter>(Visitor);
 return HasAuthority() && C && Pawn->IsPlayerControlled() && C->GetCharacterMovement()->IsMovingOnGround() && FVector::Dist(GetActorLocation(),Visitor->GetActorLocation())<260
  && GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID) && DeckRouteLocal.Num()>=4;
}
FText AIslandPartyControl::GetInteractionName_Implementation()const
{return FText::FromString(bEnding?TEXT("Guests are leaving · wait for the deck to clear"):bActive?TEXT("End poolside gathering · guests walk out"):TEXT("Invite four adult guests · poolside gathering"));}
void AIslandPartyControl::Notice(const FString& Text)const
{if(auto* PC=GetWorld()->GetFirstPlayerController())if(auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>())Nav->ShowNotice(FText::FromString(Text),4);}
void AIslandPartyControl::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor) || bEnding)return;
 if(bActive){BeginEnding();return;}
 ClearGuests();bActive=true;NextArrival=0;Arrivals=2.1f;Music->Play();Notice(TEXT("Your gathering has started. Adult guests arrive on foot along the deck; use F here to end it."));
}
bool AIslandPartyControl::SupportedPoint(FVector Local,FVector& Location,const AActor* Ignore)const
{
 const FVector Ground=GetActorTransform().TransformPosition(Local);FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(PartyDeckSupport),false,this);if(Ignore)Q.AddIgnoredActor(Ignore);
 // Survey the real capsule's foot width. A centre-only ray falls through the
 // narrow timber-board joints and incorrectly places the full capsule below
 // neighbouring boards. The physical sphere finds their supporting surface.
 constexpr float Radius=34;
 if(!GetWorld()->SweepSingleByChannel(Hit,Ground+FVector(0,0,80+Radius),Ground-FVector(0,0,90-Radius),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(Radius),Q) || Hit.ImpactNormal.Z<.7)return false;
 const FVector Floor=Hit.Location-FVector(0,0,Radius);if(FMath::Abs(Floor.Z-Ground.Z)>35)return false;
 Location=Floor+FVector(0,0,93);
 return !GetWorld()->OverlapBlockingTestByChannel(Location,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,90),Q);
}
bool AIslandPartyControl::SpawnVisitor(int32 Index,const FNeonCityPartyGuestSaveData* Saved)
{
 if(!DeckRouteLocal.Num())return false;FVector Location;const int32 Start=Saved?FMath::Clamp(Saved->Station,0,DeckRouteLocal.Num()-1):0;
 FVector Source=Saved?GetActorTransform().InverseTransformPosition(Saved->Transform.GetLocation()-FVector(0,0,93)):DeckRouteLocal[0];
 if(!SupportedPoint(Source,Location))return false;
 FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
 auto* Guest=GetWorld()->SpawnActor<ANeonCityNPCCharacter>(Location,GetActorRotation(),P);if(!Guest)return false;
 const FName ID(*FString::Printf(TEXT("Aurelia.AdultGuest.%d"),Index+1));Guest->InitializePedestrian(ID,nullptr,{},871+Index);
 if(auto* Controller=Guest->GetController())Controller->SetActorTickEnabled(false);
 Guest->GetCombatAI()->Disengage();Guest->GetCombatAI()->SetComponentTickEnabled(false);
 Guest->GetCharacterMovement()->AddTickPrerequisiteActor(this);Guest->GetCharacterMovement()->MaxWalkSpeed=125+Index*8;
 if(auto* Appearance=Guest->FindComponentByClass<UCharacterAppearanceComponent>())Appearance->ApplyPalette(Index%5);
 FVisitor Data;Data.Pawn=Guest;Data.ID=ID;Data.Station=Saved?Start:1;Data.Wait=Saved?FMath::Clamp(Saved->WaitSeconds,0.f,24.f):0;Guests.Add(Data);return true;
}
void AIslandPartyControl::BeginEnding()
{
 bActive=false;bEnding=true;Music->FadeOut(1.5f,0);for(auto& V:Guests){V.Wait=0;V.ProgressSeconds=0;}Notice(TEXT("Gathering ended. Your guests are walking to the deck entrance."));
}
void AIslandPartyControl::Tick(float Delta)
{
 Super::Tick(Delta);
 if((bActive || bEnding) && !GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID)){ClearGuests();bActive=bEnding=false;Music->Stop();return;}
 if(bActive && NextArrival<4)
 {
  Arrivals+=Delta;if(Arrivals>=2.1f){if(SpawnVisitor(NextArrival)){++NextArrival;Arrivals=0;}else Arrivals=1.4f;}
 }
 for(int32 I=Guests.Num()-1;I>=0;--I)
 {
  auto& V=Guests[I];auto* Guest=V.Pawn.Get();if(!Guest){Guests.RemoveAt(I);continue;}
  if(V.Wait>0){V.Wait-=Delta;Guest->GetCharacterMovement()->StopMovementImmediately();continue;}
  const FVector Goal=GetActorTransform().TransformPosition(DeckRouteLocal[FMath::Clamp(V.Station,0,DeckRouteLocal.Num()-1)])+FVector(0,0,93),Direction=Goal-Guest->GetActorLocation();
  if(Direction.Size2D()<45)
  {
   Guest->GetCharacterMovement()->StopMovementImmediately();V.ProgressSeconds=0;
   if(bEnding){if(V.Station==0){Guest->Destroy();Guests.RemoveAt(I);}else --V.Station;continue;}
   // Different pauses prevent synchronous crowd motion. Guests converse/rest
   // at the existing safe deck loop, without claiming unauthored dance/swim.
   V.Wait=6.f+(I*3.f);V.Station=(V.Station+1)%DeckRouteLocal.Num();continue;
  }
  FVector Move=Direction.GetSafeNormal2D();FCollisionQueryParams Q(SCENE_QUERY_STAT(PartyGuestAvoidance),false,Guest);FHitResult Ahead;
  const FVector From=Guest->GetActorLocation();
  V.AvoidSeconds=FMath::Max(0.f,V.AvoidSeconds-Delta);
  const bool Obstacle=GetWorld()->SweepSingleByChannel(Ahead,From,From+Move*100,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,85),Q);
  const FVector Side(-Move.Y,Move.X,0);const float Preferred=V.AvoidSide==0?1.f:V.AvoidSide;TArray<TPair<FVector,float>> Candidates;
  // Keep a chosen passing side briefly to avoid alternating sides every frame
  // when another physical character is just at the look-ahead boundary.
  if(Obstacle || V.AvoidSeconds>0){Candidates.Add({(Move*.55+Side*Preferred).GetSafeNormal2D(),Preferred});Candidates.Add({(Move*.55-Side*Preferred).GetSafeNormal2D(),-Preferred});}
  Candidates.Add({Move,0});if(!Obstacle && V.AvoidSeconds<=0){Candidates.Add({(Move*.55+Side).GetSafeNormal2D(),1});Candidates.Add({(Move*.55-Side).GetSafeNormal2D(),-1});}
  bool Safe=false;
  for(const auto& Candidate:Candidates)
  {
   FVector Support;const FVector FootLocal=GetActorTransform().InverseTransformPosition(From+Candidate.Key*55-FVector(0,0,93));if(!SupportedPoint(FootLocal,Support,Guest))continue;
   // Sweep exactly the chosen diagonal, including the full physical capsule.
   // A 3cm floor clearance matches SupportedPoint; no actor pose is adjusted.
   FHitResult Step;if(GetWorld()->SweepSingleByChannel(Step,From+FVector(0,0,3),Support,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,90),Q))continue;
   Move=Candidate.Key;Safe=true;if(Candidate.Value!=0){V.AvoidSide=Candidate.Value;if(Obstacle)V.AvoidSeconds=.65f;}break;
  }
  if(!Safe){Guest->GetCharacterMovement()->StopMovementImmediately();continue;}
  Guest->AddMovementInput(Move,1);
 }
 if(bEnding && Guests.IsEmpty()){bEnding=false;Notice(TEXT("All guests have left. The deck is clear."));}
}
int32 AIslandPartyControl::GetGuestCount()const
{int32 Count=0;for(const auto& V:Guests)if(V.Pawn.IsValid())++Count;return Count;}
bool AIslandPartyControl::WriteReadOnlyDiagnostics(const FString& Directory,bool IncludeRoute)const
{
 using namespace PartyDiagnostics;if(!GetWorld() || DeckRouteLocal.IsEmpty())return false;IFileManager::Get().MakeDirectory(*Directory,true);
 const FString Header=TEXT("subject,station,wait_seconds,position_x,position_y,position_z,goal_x,goal_y,goal_z,distance_2d,velocity_x,velocity_y,velocity_z,query,from_x,from_y,from_z,to_x,to_y,to_z,blocked,start_penetrating,actor,component,mesh,impact_x,impact_y,impact_z,normal_z,penetration_depth\n");
 FString Live=Header;
 for(const auto& V:Guests)if(const auto* G=V.Pawn.Get())
 {
  const FVector From=G->GetActorLocation(),Goal=GetActorTransform().TransformPosition(DeckRouteLocal[FMath::Clamp(V.Station,0,DeckRouteLocal.Num()-1)])+FVector(0,0,93),Move=(Goal-From).GetSafeNormal2D(),Side(-Move.Y,Move.X,0);
  const FString Prefix=Cell(V.ID.ToString())+FString::Printf(TEXT(",%d,%.4f,"),V.Station,V.Wait)+XYZ(From)+TEXT(",")+XYZ(Goal)+FString::Printf(TEXT(",%.3f,"),FVector::Dist2D(From,Goal))+XYZ(G->GetVelocity());
  FCollisionQueryParams Q(SCENE_QUERY_STAT(PartyReadOnlyGuest),false,G);FHitResult H;
  for(const auto& Probe:TArray<TPair<FString,FVector>>{{TEXT("ahead100"),Move*100},{TEXT("side_left80"),Side*80},{TEXT("side_right80"),-Side*80}})
  {const bool Blocked=GetWorld()->SweepSingleByChannel(H,From,From+Probe.Value,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,85),Q);Row(Live,Prefix,*Probe.Key,From,From+Probe.Value,Blocked,H);}
  for(const auto& Probe:TArray<TPair<FString,FVector>>{{TEXT("forward"),Move},{TEXT("left"),(Move*.55+Side).GetSafeNormal()},{TEXT("right"),(Move*.55-Side).GetSafeNormal()}})
  {
   const FVector Foot=From+Probe.Value*55-FVector(0,0,93);const bool Ray=GetWorld()->LineTraceSingleByChannel(H,Foot+FVector(0,0,80),Foot-FVector(0,0,90),ECC_Pawn,Q);Row(Live,Prefix,*(Probe.Key+TEXT("_ground_ray")),Foot+FVector(0,0,80),Foot-FVector(0,0,90),Ray,H);
   const bool Hit=GetWorld()->SweepSingleByChannel(H,Foot+FVector(0,0,114),Foot-FVector(0,0,56),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(34),Q);Row(Live,Prefix,*(Probe.Key+TEXT("_ground_sphere34")),Foot+FVector(0,0,114),Foot-FVector(0,0,56),Hit,H);
   const bool Supported=Hit && H.ImpactNormal.Z>=.7 && FMath::Abs(H.Location.Z-34-Foot.Z)<=35;const FVector Capsule=Supported?H.Location+FVector(0,0,59):Foot+FVector(0,0,93);
   Overlaps(GetWorld(),Live,Prefix,*(Probe.Key+TEXT("_full_capsule")),Capsule,FCollisionShape::MakeCapsule(34,90),Q);
   const bool Blocked=GetWorld()->SweepSingleByChannel(H,From+FVector(0,0,3),Capsule,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,90),Q);Row(Live,Prefix,*(Probe.Key+TEXT("_actual55_capsule")),From+FVector(0,0,3),Capsule,Blocked,H);
  }
 }
 bool Saved=FFileHelper::SaveStringToFile(Live,*(Directory/TEXT("guest-blockers.csv")));if(!IncludeRoute)return Saved;
 FString Route=Header;FCollisionQueryParams Q(SCENE_QUERY_STAT(PartyReadOnlyRoute),false);
 for(int32 Segment=0;Segment<DeckRouteLocal.Num();++Segment)
 {
  const FVector A=GetActorTransform().TransformPosition(DeckRouteLocal[Segment]),B=GetActorTransform().TransformPosition(DeckRouteLocal[(Segment+1)%DeckRouteLocal.Num()]);const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/25));
  for(int32 Step=0;Step<=Steps;++Step)
  {
   const FVector Ground=FMath::Lerp(A,B,float(Step)/Steps);const FString Prefix=Cell(FString::Printf(TEXT("route_%d_%d"),Segment,Step))+FString::Printf(TEXT(",%d,0,"),Segment)+XYZ(Ground+FVector(0,0,93))+TEXT(",")+XYZ(B+FVector(0,0,93))+FString::Printf(TEXT(",%.3f,0,0,0"),FVector::Dist2D(Ground,B));FHitResult H;
   const bool Hit=GetWorld()->LineTraceSingleByChannel(H,Ground+FVector(0,0,80),Ground-FVector(0,0,90),ECC_Pawn,Q);Row(Route,Prefix,TEXT("ground"),Ground+FVector(0,0,80),Ground-FVector(0,0,90),Hit,H);
   const bool Sphere=GetWorld()->SweepSingleByChannel(H,Ground+FVector(0,0,114),Ground-FVector(0,0,56),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(34),Q);Row(Route,Prefix,TEXT("ground_sphere34"),Ground+FVector(0,0,114),Ground-FVector(0,0,56),Sphere,H);
   const bool Support=Sphere && H.ImpactNormal.Z>=.7 && FMath::Abs(H.Location.Z-34-Ground.Z)<=35;const FVector Centre=Support?H.Location+FVector(0,0,59):Ground+FVector(0,0,93);Overlaps(GetWorld(),Route,Prefix,TEXT("full_capsule"),Centre,FCollisionShape::MakeCapsule(34,90),Q);
   if(Step<Steps){const FVector NextGround=FMath::Lerp(A,B,float(Step+1)/Steps);FVector Next=NextGround+FVector(0,0,93);SupportedPoint(GetActorTransform().InverseTransformPosition(NextGround),Next);const bool Blocked=GetWorld()->SweepSingleByChannel(H,Centre,Next,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,90),Q);Row(Route,Prefix,TEXT("next25_capsule"),Centre,Next,Blocked,H);}
  }
 }
 Saved=FFileHelper::SaveStringToFile(Route,*(Directory/TEXT("route-ground-capsule.csv"))) && Saved;
 const FString Scope=TEXT("Read-only native layout/blocker evidence. Actor poses, guest state, collision and input remain unchanged. Raw ground ray retained for comparison; production support uses sphere radius34, normal_z>=0.7, and (sphere centreZ-34) height delta<=35cm. Complete capsule radius34/halfheight90cm, centre floor+93cm; actual candidate sweep starts3cm above unmodified guest pose and ends at the supported55cm footstep. Route next25 end uses its own surveyed floor height. This diagnostic is not a gameplay PASS.\n");
 return FFileHelper::SaveStringToFile(Scope,*(Directory/TEXT("diagnostic-scope.txt"))) && Saved;
}
void AIslandPartyControl::ClearGuests(){for(auto& V:Guests)if(V.Pawn.IsValid())V.Pawn->Destroy();Guests.Empty();}
FNeonCityResidencePartySaveData AIslandPartyControl::BuildPersistenceSnapshot(bool Defaults)const
{
 FNeonCityResidencePartySaveData S;S.PropertyID=PropertyID;if(Defaults)return S;S.Active=bActive;S.Ending=bEnding;S.NextArrival=NextArrival;
 for(const auto& V:Guests)if(V.Pawn.IsValid()){FNeonCityPartyGuestSaveData D;D.GuestID=V.ID;D.Transform=V.Pawn->GetActorTransform();D.Station=V.Station;D.WaitSeconds=FMath::Max(0.f,V.Wait);S.Guests.Add(D);}return S;
}
void AIslandPartyControl::ApplyPersistenceSnapshot(const FNeonCityResidencePartySaveData& S)
{
 ClearGuests();Music->Stop();bActive=bEnding=false;NextArrival=0;Arrivals=0;
 if(S.PropertyID!=PropertyID || !GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID))return;
 bActive=S.Active;bEnding=S.Ending;
 if(bActive || bEnding)for(const auto& D:S.Guests){const FString Tail=D.GuestID.ToString().Right(1);const int32 Index=FCString::Atoi(*Tail)-1;if(Index>=0 && Index<4)SpawnVisitor(Index,&D);}
 NextArrival=FMath::Clamp<int32>(S.NextArrival,0,4);if(bActive)Music->Play();
}
void AIslandPartyControl::EndPlay(EEndPlayReason::Type Reason){ClearGuests();Super::EndPlay(Reason);}
