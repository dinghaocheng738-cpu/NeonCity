#include "Adventure/IslandActivityJob.h"
#include "Opportunities/NeonCityOpportunitySubsystem.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "Combat/HealthComponent.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Animation/AnimSequence.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "ImageUtils.h"

AIslandActivityPoint::AIslandActivityPoint()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObjectiveVisual"));Visual->SetupAttachment(RootComponent);Visual->SetCollisionProfileName(TEXT("BlockAll"));
 Interaction=CreateDefaultSubobject<UBoxComponent>(TEXT("Interaction"));Interaction->SetupAttachment(RootComponent);Interaction->SetBoxExtent(FVector(45,45,75));Interaction->SetRelativeLocation(FVector(0,0,80));
 Interaction->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Interaction->SetCollisionResponseToAllChannels(ECR_Ignore);Interaction->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Sign=CreateDefaultSubobject<UTextRenderComponent>(TEXT("InstructionSign"));Sign->SetupAttachment(RootComponent);Sign->SetWorldSize(12);Sign->SetRelativeLocation(FVector(0,0,145));Sign->SetHorizontalAlignment(EHTA_Center);
}
bool AIslandActivityPoint::CanInteract_Implementation(AActor* Visitor) const
{return Job && Job->CanInteractAt(Index,Visitor);}
FText AIslandActivityPoint::GetInteractionName_Implementation() const
{return Job?Job->GetPointInstruction(Index):FText::GetEmpty();}
void AIslandActivityPoint::Interact_Implementation(AActor* Visitor)
{if(CanInteract_Implementation(Visitor))Job->InteractAt(Index,Visitor);}

AIslandActivityJob::AIslandActivityJob()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AuthoredActivity")));
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;
 PickupMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NeonCity/Art/IslandLandPass1/Meshes/SM_Land_wooden_crate_01.SM_Land_wooden_crate_01")));
}
FVector AIslandActivityJob::PointLocation(int32 Index) const
{return GetActorTransform().TransformPosition(Index==0?PickupLocal:Index==1?ReturnLocal:RoutePointsLocal.IsValidIndex(Index-2)?RoutePointsLocal[Index-2]:ReturnLocal);}
void AIslandActivityJob::BeginPlay()
{
 Super::BeginPlay();Progress.OpportunityID=ActivityID;
 if(ActivityID.IsNone() || Title.IsEmpty())return;
 GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->RegisterProvider(this);
 for(int32 I=0;I<(Kind==ENeonCityIslandActivity::HiddenLandmark?1:2);++I)
 {
  auto* Point=GetWorld()->SpawnActor<AIslandActivityPoint>(PointLocation(I),GetActorRotation());
  if(!Point)continue;Point->Job=this;Point->Index=I;Point->Sign->SetText(Title);Point->Sign->SetVisibility(false);Points.Add(Point);
  if(I==0 && (Kind==ENeonCityIslandActivity::Delivery || Kind==ENeonCityIslandActivity::DiveSalvage))Point->Visual->SetStaticMesh(PickupMesh.LoadSynchronous());
  else
  {
   Point->Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/IslandLandPass1/Meshes/SM_LandWayfindingPanel.SM_LandWayfindingPanel")));
   Point->Visual->SetRelativeScale3D(FVector(.35));
  }
 }
}
FName AIslandActivityJob::GetReceiptID() const
{return FName(*FString::Printf(TEXT("IslandActivity.%s.%s"),*ActivityID.ToString(),*Progress.ActivityRunID.ToString(EGuidFormats::Digits)));}
FNeonCityOpportunityData AIslandActivityJob::GetOpportunityData() const
{
 FNeonCityOpportunityData Data;Data.OpportunityID=ActivityID;Data.Source=ENeonCityOpportunitySource::Phone;Data.Contact=Contact;Data.Title=Title;
 Data.RewardPreview=Reward;Data.State=ENeonCityOpportunityState(Progress.State);Data.Repeatable=true;
 if(Kind==ENeonCityIslandActivity::Photography && Progress.ActivityStage==1)Data.PreviewImageFile=Progress.ActivityPhotoFile;
 const TCHAR* Rules[]={TEXT("Collect the actual parcel at the loading point, carry it by vehicle, then hand it to the receiving clerk."),
 TEXT("Photograph the marked subject in the centre of your normal camera view. Take and inspect a real photograph, then return to submit it."),
 TEXT("Park at the pickup. Your guest walks to the passenger door and takes the seat. Drive to the dropoff, stop, and let the guest walk away."),
 TEXT("Bring a real kept catch of the requested species and weight. One fish identity can be handed in only once."),
 TEXT("Equip your diving gear. Retrieve the marked submerged toolbox through the normal interaction, then return it to the harbour."),
 TEXT("Meet the visitor and walk the marked tour. Stop together at each view. Wait or return if your visitor loses contact."),
 TEXT("Reach the person beside the offshore buoy in a boat, take them aboard, and return to the safe dock. A lost person, wreck or timeout fails the run."),
 TEXT("Start on foot at the roof launch. Use your existing wingsuit, pass the ordered approach points and land normally in the clear village zone."),
 TEXT("Start on foot and hike the ordered trail stops. Finish on foot to record your time."),
 TEXT("Find and inspect this hidden island landmark. Its discovery is recorded in your existing activity log.")};
 FString Text=Rules[FMath::Clamp(int32(Kind),0,9)];
 if(Kind==ENeonCityIslandActivity::FishingOrder)
 {
  const auto* Inventory=GetGameInstance()?GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>():nullptr;
  const auto* Species=Inventory?Inventory->FindSpecies(RequiredSpecies):nullptr;
  const FString Name=Species?Species->Name.ToString():TEXT("Requested fish");
  Text+=FString::Printf(TEXT("\n%s · %.2f–%.2f kg"),*Name,RequiredWeightKg.X,RequiredWeightKg.Y);
 }
 if(Progress.State==1)Text+=TEXT("\n\n")+GetPointInstruction(Progress.ActivityStage==0?0:1).ToString();
 if(Progress.ActivityBestSeconds>0)Text+=FString::Printf(TEXT("\nBest %.1f s · %d completed runs"),Progress.ActivityBestSeconds,Progress.ActivityCompletions);
 Data.Description=FText::FromString(Text);
 Data.ActiveActionAvailable=PendingPhotoFile.IsEmpty() && Progress.State==1 && Progress.ActivityStage==0 && (Kind==ENeonCityIslandActivity::Photography || Kind==ENeonCityIslandActivity::SeaRescue || Kind==ENeonCityIslandActivity::Taxi);
 Data.ActiveActionLabel=FText::FromString(Kind==ENeonCityIslandActivity::Photography?(Progress.ActivityStage==0?TEXT("Take photograph"):TEXT("Review saved photograph / submit at clerk")):TEXT("Take person aboard when safely alongside"));
 return Data;
}
void AIslandActivityJob::Notify(const FText& Text,float Seconds) const
{if(auto* PC=GetWorld()->GetFirstPlayerController())if(auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>())Nav->ShowNotice(Text,Seconds);}
void AIslandActivityJob::SetProgressStage(uint8 Value)
{Progress.ActivityStage=Value;WaitSeconds=0;UpdateNavigation();GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->NotifyChanged(this);}
void AIslandActivityJob::UpdateNavigation()
{
 auto* PC=Customer.Get();if(!PC)PC=GetWorld()->GetFirstPlayerController();auto* Nav=PC?PC->FindComponentByClass<UNeonCityNavigationComponent>():nullptr;if(!Nav)return;
 if(Progress.State!=1){Nav->ClearDestination(ActivityID);return;}
 FNeonCityNavigationDestination Goal;Goal.OwnerID=ActivityID;Goal.DestinationID=FName(*FString::Printf(TEXT("%s.Stage%d"),*ActivityID.ToString(),Progress.ActivityStage));Goal.Label=Title;
 Goal.Location=PointLocation(Progress.ActivityStage==0?0:1);Goal.UseRoadRoute=Kind==ENeonCityIslandActivity::Delivery || Kind==ENeonCityIslandActivity::Taxi;
 if((Kind==ENeonCityIslandActivity::GuidedTour || Kind==ENeonCityIslandActivity::TimedHike || Kind==ENeonCityIslandActivity::WingsuitChallenge) && Progress.ActivityStage>0 && RoutePointsLocal.IsValidIndex(Progress.ActivityCheckpoint))
  Goal.Location=GetActorTransform().TransformPosition(RoutePointsLocal[Progress.ActivityCheckpoint]);
 Nav->SetDestination(Goal);
}
bool AIslandActivityJob::ChooseOpportunity(ENeonCityOpportunityChoice Choice,APlayerController* Player)
{
 if(!Player || !Player->IsLocalController() || Player->GetWorld()!=GetWorld() || !Player->GetPawn())return false;
 if(Choice==ENeonCityOpportunityChoice::Action)return Progress.State==1 && PerformAction(Player);
 if(Choice==ENeonCityOpportunityChoice::Cancel){if(Progress.State!=1)return false;Cancel(FText::FromString(TEXT("Activity cancelled safely. No reward was paid.")));return true;}
 if(Choice==ENeonCityOpportunityChoice::Decline || Choice==ENeonCityOpportunityChoice::Ignore)
 {if(Progress.State==1)return false;Progress.State=Choice==ENeonCityOpportunityChoice::Decline?3:4;return true;}
 if(Choice!=ENeonCityOpportunityChoice::Accept || Progress.State==1)return false;
 // One content run at a time prevents passenger/cargo input and navigation conflicts.
 for(TActorIterator<AIslandActivityJob> It(GetWorld());It;++It)if(*It!=this && It->Progress.State==1)
 {Notify(FText::FromString(TEXT("Finish or cancel your active island activity first.")));return false;}
 ClearPresentation();const float Best=Progress.ActivityBestSeconds;const int32 Count=Progress.ActivityCompletions;
 Progress={};Progress.OpportunityID=ActivityID;Progress.State=1;Progress.ActivityRunID=FGuid::NewGuid();Progress.ActivityBestSeconds=Best;Progress.ActivityCompletions=Count;
 Customer=Player;LastPlayerLocation=Player->GetPawn()->GetActorLocation();StopSeconds=0;
 if(Kind==ENeonCityIslandActivity::Taxi || Kind==ENeonCityIslandActivity::GuidedTour || Kind==ENeonCityIslandActivity::SeaRescue)CreateGuest(PointLocation(0)+FVector(100,80,92),Kind==ENeonCityIslandActivity::SeaRescue);
 if(Points.IsValidIndex(0))Points[0]->Visual->SetHiddenInGame(false);
 UpdateNavigation();Notify(FText::FromString(TEXT("Activity accepted. Follow the existing map guidance and the instructions in P → O.")));return true;
}
bool AIslandActivityJob::CanInteractAt(int32 Index,AActor* Visitor) const
{
 if(Progress.State!=1 || !Visitor || Visitor->GetWorld()!=GetWorld() || !Visitor->HasAuthority())return false;
 const auto* Pawn=Cast<APawn>(Visitor);const auto* PC=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
 return PC && PC->IsLocalController() && FVector::Dist(Visitor->GetActorLocation(),PointLocation(Index)+FVector(0,0,90))<240;
}
FText AIslandActivityJob::GetPointInstruction(int32 Index) const
{
 if(Kind==ENeonCityIslandActivity::Photography)return FText::FromString(Progress.ActivityStage==0?TEXT("P → O → ENTER take the composed photograph"):TEXT("F Submit your saved photograph to the clerk"));
 if(Kind==ENeonCityIslandActivity::FishingOrder)return FText::FromString(TEXT("F Hand over a matching catch from your pouch"));
 if(Kind==ENeonCityIslandActivity::Taxi)return FText::FromString(Index==0?TEXT("F Invite your visitor into a stopped nearby car"):TEXT("F Let the passenger leave at the dropoff"));
 if(Kind==ENeonCityIslandActivity::SeaRescue)return FText::FromString(Index==0?TEXT("P → O → ENTER take the person aboard your stopped boat"):TEXT("F Let the rescued person walk onto the safe dock"));
 if(Kind==ENeonCityIslandActivity::GuidedTour)return FText::FromString(Progress.ActivityStage==0?TEXT("F Meet the visitor and start the walking tour"):TEXT("Walk together; stop at the views. Return to your visitor if separated."));
 if(Kind==ENeonCityIslandActivity::WingsuitChallenge || Kind==ENeonCityIslandActivity::TimedHike)return FText::FromString(Progress.ActivityStage==0?TEXT("F Start the timed route on foot"):TEXT("Follow the ordered route and finish on foot"));
 if(Kind==ENeonCityIslandActivity::HiddenLandmark)return FText::FromString(TEXT("F Inspect and record this landmark"));
 return FText::FromString(Index==0?(Kind==ENeonCityIslandActivity::DiveSalvage?TEXT("F Recover the submerged toolbox"):TEXT("F Take this parcel; carry it to your car")):TEXT("F Hand the actual carried item to the clerk"));
}

APawn* AIslandActivityJob::FindStoppedTransport(FVector At,bool Boat) const
{
 APawn* Best=nullptr;float Distance=450;
 for(TActorIterator<APawn> It(GetWorld());It;++It)
 {
  if(Boat?!It->IsA<ANeonCityMarineVehicleBase>():!It->IsA<ANeonCityBaseVehicle>())continue;
  const float D=FVector::Dist(It->GetActorLocation(),At);
  if(D<Distance && It->GetVelocity().Size()<80 && It->GetController()==Customer.Get()){Best=*It;Distance=D;}
 }
 return Best;
}
void AIslandActivityJob::CreateGuest(FVector Location,bool Swimming)
{
 FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
 Guest=GetWorld()->SpawnActor<ANeonCityNPCCharacter>(Location,GetActorRotation(),P);
 if(!Guest)return;
 // CharacterMovement consumes movement input every frame. A 0.1s job tick
 // left the guest braking between isolated input frames. Only an unseated
 // guest needs the per-frame driver; idle jobs and seated guests stay at 10Hz.
 SetActorTickInterval(0.f);
 Guest->GetCharacterMovement()->AddTickPrerequisiteActor(this);
 // Retain the character's walking animation tick, while the activity owns
 // movement. A generic pedestrian controller must not wander off mid-tour.
 if(auto* Controller=Guest->GetController())Controller->SetActorTickEnabled(false);
 Guest->GetCombatAI()->Disengage();Guest->GetCombatAI()->SetComponentTickEnabled(false);
 if(Swimming)
 {
  auto* Move=Guest->GetCharacterMovement();Move->GetNavAgentPropertiesRef().bCanSwim=true;
  Move->MaxSwimSpeed=250;Move->MaxAcceleration=500;Move->BrakingDecelerationSwimming=300;
  Guest->GetCapsuleComponent()->UpdatePhysicsVolume(true);Move->SetMovementMode(MOVE_Swimming);
 }
}
void AIslandActivityJob::KeepRescueGuestAfloat(float Delta)
{
 if(Kind!=ENeonCityIslandActivity::SeaRescue || !Guest || bGuestBoarded || bGuestExiting || Guest->IsHidden())return;
 auto* Move=Guest->GetCharacterMovement();float Water=0;FVector Flow;
 if(!GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(Guest->GetActorLocation(),Water,Flow))return;
 const float Half=Guest->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 if(Guest->GetActorLocation().Z-Half>=Water)return;
 // The activity visitor uses the same native CMC fluid integration as a
 // swimmer. Ordinary per-frame vertical input follows the sampled live sea;
 // no position/velocity rewrite, invisible floor or invulnerability is used.
 Move->GetNavAgentPropertiesRef().bCanSwim=true;
 if(!Move->IsSwimming())Move->SetMovementMode(MOVE_Swimming);
 const float Immersion=Move->ImmersionDepth();Move->Buoyancy=Immersion>.1f?1.f/Immersion:1.f;
 const float Desired=FMath::Clamp(Flow.Z+(Water-Guest->GetActorLocation().Z-Half*.55f)*4.f,-140.f,160.f);
 const float Gain=Delta>UE_SMALL_NUMBER?(1-FMath::Exp(-8.f*Delta))/Delta:8.f;
 Guest->AddMovementInput(FVector::UpVector,FMath::Clamp((Desired-Move->Velocity.Z)*Gain/FMath::Max(1.f,Move->MaxAcceleration),-1.f,1.f));
}
bool AIslandActivityJob::MoveGuest(FVector Goal,float Delta)
{
 if(!Guest)return false;const FVector Direction=Goal-Guest->GetActorLocation();
 if(Direction.Size2D()<65){Guest->GetCharacterMovement()->StopMovementImmediately();return true;}
 Guest->GetCharacterMovement()->MaxWalkSpeed=250;Guest->AddMovementInput(Direction.GetSafeNormal2D(),1);
 Guest->SetActorRotation(FRotator(0,Direction.Rotation().Yaw,0));return false;
}
bool AIslandActivityJob::BoardGuest(APawn* Vehicle,bool Boat)
{
 if(!Guest || !Vehicle || Vehicle->GetVelocity().Size()>80)return false;
 Passenger=NewObject<UTransportOccupantComponent>(Vehicle,TEXT("IslandGuestSeat"));Vehicle->AddInstanceComponent(Passenger);Passenger->RegisterComponent();
 Passenger->SeatHipLocation=Boat?FVector(45,55,65):FVector(-5,43,35);
 if(!Passenger->ShowDriver(Guest)){Passenger->DestroyComponent();Passenger=nullptr;return false;}
 Transport=Vehicle;TransportPickup=Vehicle->GetActorLocation();bGuestBoarded=true;
 Guest->GetCharacterMovement()->StopMovementImmediately();Guest->GetCharacterMovement()->DisableMovement();Guest->SetActorEnableCollision(false);Guest->SetActorHiddenInGame(true);
 Guest->AttachToComponent(Vehicle->GetRootComponent(),FAttachmentTransformRules::SnapToTargetNotIncludingScale);
 SetActorTickInterval(.1f);
 return true;
}
bool AIslandActivityJob::ExitGuest()
{
 auto* V=Transport.Get();if(!Guest || !V || !bGuestBoarded || bGuestExiting || V->GetVelocity().Size()>65)return false;
 FVector Exit;bool Found=false;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(IslandGuestExit),false,V);Q.AddIgnoredActor(Guest);
 // A real berth can be on either side. The old right-only probe rejected the
 // harbour's left-hand dock, even when the player had safely moored there.
 for(const float Sign:{1.f,-1.f})
 {
  const FVector Side=V->GetActorLocation()+V->GetActorRightVector()*(260*Sign);FHitResult H;
  if(!GetWorld()->LineTraceSingleByChannel(H,Side+FVector(0,0,300),Side-FVector(0,0,550),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)continue;
  if(Kind==ENeonCityIslandActivity::SeaRescue){float WaterHeight=0;FVector Current;if(GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(H.ImpactPoint,WaterHeight,Current) && H.ImpactPoint.Z<WaterHeight+15)continue;}
  const FVector Candidate=H.ImpactPoint+FVector(0,0,Guest->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3);
  if(GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Guest->GetCapsuleComponent()->GetScaledCapsuleRadius(),Guest->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Q))continue;
  Exit=Candidate;Found=true;break;
 }
 if(!Found)return false;
 Guest->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);Guest->SetActorLocation(Exit,false,nullptr,ETeleportType::TeleportPhysics);
 Guest->SetActorEnableCollision(true);Guest->SetActorHiddenInGame(false);Guest->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
 SetActorTickInterval(0.f);
 if(Passenger){Passenger->HideDriver();Passenger->DestroyComponent();Passenger=nullptr;}bGuestBoarded=false;bGuestExiting=true;return true;
}
bool AIslandActivityJob::InteractAt(int32 Index,AActor* Visitor)
{
 if(!CanInteractAt(Index,Visitor))return false;auto* PC=Cast<APlayerController>(Cast<APawn>(Visitor)->GetController());Customer=PC;
 if(Index==1 && Progress.ActivityStage>0)
 {
  if(Kind==ENeonCityIslandActivity::Taxi || Kind==ENeonCityIslandActivity::SeaRescue)
   return Transport.IsValid() && FVector::Dist2D(TransportPickup,Transport->GetActorLocation())>1500 && ExitGuest();
  if(Kind==ENeonCityIslandActivity::Photography)return !Progress.ActivityPhotoFile.IsEmpty() && IFileManager::Get().FileExists(*Progress.ActivityPhotoFile) && Complete();
  if(Kind==ENeonCityIslandActivity::Delivery || Kind==ENeonCityIslandActivity::DiveSalvage)
   return Progress.ActivityItemID.IsValid() && Progress.ActivityCheckpoint>=1 && Complete();
  if(Kind==ENeonCityIslandActivity::TimedHike || Kind==ENeonCityIslandActivity::WingsuitChallenge)
  {
   auto* C=Cast<ACharacter>(Visitor);return C && C->GetCharacterMovement()->IsMovingOnGround() && Progress.ActivityCheckpoint==RoutePointsLocal.Num() && Complete();
  }
 }
 if(Index!=0 || Progress.ActivityStage!=0)return false;
 if(Kind==ENeonCityIslandActivity::Photography)return CapturePhotograph(PC);
 if(Kind==ENeonCityIslandActivity::FishingOrder)
 {
  auto* F=GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();TArray<FNeonCityCaughtFish> Fish=F->GetFish();
  for(const auto& Catch:Fish)if(F->DeliverFishForOrder(Catch.FishID,RequiredSpecies,RequiredWeightKg.X,RequiredWeightKg.Y,Reward,GetReceiptID()))return Complete(true);
  Notify(FText::FromString(TEXT("No raw catch in your pouch matches this order's species and weight.")));return false;
 }
 if(Kind==ENeonCityIslandActivity::HiddenLandmark){Progress.ActivityCheckpoint=1;return Complete();}
 if(Kind==ENeonCityIslandActivity::Taxi || Kind==ENeonCityIslandActivity::SeaRescue)
 {Notify(FText::FromString(TEXT("Board your transport, stop beside the guest, then use P → O → ENTER.")));return false;}
 if(Kind==ENeonCityIslandActivity::Delivery || Kind==ENeonCityIslandActivity::DiveSalvage)
 {
  auto* Player=Cast<ANeonCityPlayerCharacter>(Visitor);
  if(Kind==ENeonCityIslandActivity::DiveSalvage && (!Player || !Player->FindComponentByClass<UDivingComponent>()->IsEquipped()))return false;
  Progress.ActivityItemID=FGuid::NewGuid();Cargo=NewObject<UStaticMeshComponent>(this,TEXT("CarriedIslandParcel"));AddInstanceComponent(Cargo);Cargo->SetStaticMesh(PickupMesh.LoadSynchronous());Cargo->SetCollisionEnabled(ECollisionEnabled::NoCollision);Cargo->RegisterComponent();
  Cargo->AttachToComponent(Visitor->GetRootComponent(),FAttachmentTransformRules::KeepRelativeTransform);Cargo->SetRelativeLocation(FVector(35,0,-10));Cargo->SetRelativeScale3D(FVector(.4));
  Points[0]->Visual->SetHiddenInGame(true);SetProgressStage(1);return true;
 }
 if(Kind==ENeonCityIslandActivity::GuidedTour || Kind==ENeonCityIslandActivity::TimedHike || Kind==ENeonCityIslandActivity::WingsuitChallenge)
 {SetProgressStage(1);Progress.ActivitySeconds=0;return true;}
 return false;
}
bool AIslandActivityJob::PerformAction(APlayerController* Player)
{
 if(Kind==ENeonCityIslandActivity::Photography)return Progress.ActivityStage==0 && CapturePhotograph(Player);
 if((Kind==ENeonCityIslandActivity::Taxi || Kind==ENeonCityIslandActivity::SeaRescue) && Progress.ActivityStage==0)
 {
  auto* Vehicle=FindStoppedTransport(PointLocation(0),Kind==ENeonCityIslandActivity::SeaRescue);
  if(!Vehicle || !Guest){Notify(FText::FromString(TEXT("Stop safely alongside the guest first.")));return false;}
  SetProgressStage(1);Transport=Vehicle;TransportPickup=Vehicle->GetActorLocation();WaitSeconds=0;return true;
 }
 return false;
}
bool AIslandActivityJob::CapturePhotograph(APlayerController* Player)
{
 if(!Player || !Player->PlayerCameraManager || !PendingPhotoFile.IsEmpty() || FScreenshotRequest::IsScreenshotRequested() || !GetWorld()->GetGameViewport())return false;
 const FVector Eye=Player->PlayerCameraManager->GetCameraLocation(),Target=PhotoTarget?PhotoTarget->GetActorLocation():GetActorTransform().TransformPosition(PhotoTargetLocal);
 const FRotator View=Player->PlayerCameraManager->GetCameraRotation();const FVector Direction=Target-Eye;
 FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(IslandPhotoComposition),false,Player->GetPawn());
 const bool Blocked=GetWorld()->LineTraceSingleByChannel(H,Eye,Target,ECC_Visibility,Q) && H.GetActor()!=PhotoTarget && H.Distance<Direction.Size()-100;
 if(Direction.Size()<500 || Direction.Size()>40000 || FVector::DotProduct(Direction.GetSafeNormal(),View.Vector())<.97 || Blocked)
 {
  UE_LOG(LogTemp,Warning,TEXT("Island photo composition refused: eye=%s target=%s distance=%.1f alignment=%.4f blocked=%d hit=%s component=%s impact=%s"),*Eye.ToString(),*Target.ToString(),Direction.Size(),FVector::DotProduct(Direction.GetSafeNormal(),View.Vector()),Blocked,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*H.ImpactPoint.ToString());
  Notify(FText::FromString(TEXT("Keep the requested subject clearly visible near the centre of your camera.")));return false;
 }
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("IslandActivities/Photos"),Name=Progress.ActivityRunID.ToString(EGuidFormats::Digits)+TEXT(".png");IFileManager::Get().MakeDirectory(*Folder,true);
 // Use the actual player's rendered view, including its established exposure,
 // lights and materials. A new one-frame SceneCapture has no exposure history
 // and previously exported an almost white image despite correct composition.
 PendingPhotoFile=Folder/Name;
 // A phone photograph uses the live camera's exposure, but its owner must
 // not obscure the subject. Hide only from this local view for the capture
 // frame; physics, animation, other views and normal input remain active.
 PhotoViewOwner=Player;PhotoSubject=Player->GetPawn();PhotoRequestedAt=GetWorld()->GetTimeSeconds();
 bPhotoSubjectHiddenByUs=PhotoSubject.IsValid() && !Player->HiddenActors.Contains(PhotoSubject.Get());
 if(bPhotoSubjectHiddenByUs)
 {
  Player->HiddenActors.Add(PhotoSubject.Get());
 }
 // Let the live renderer resolve the changed visibility before exporting.
 // A camera-cut reset discarded lighting history and produced dark patches.
 // This brief capture wait preserves the normal exposure and scene history.
 Notify(FText::FromString(TEXT("Taking photograph...")));return true;
}
void AIslandActivityJob::PhotographCaptured(int32 Width,int32 Height,const TArray<FColor>& Pixels)
{
 RestorePhotoView();
 if(PhotoCaptureHandle.IsValid()){UGameViewportClient::OnScreenshotCaptured().Remove(PhotoCaptureHandle);PhotoCaptureHandle.Reset();}
 const FString File=MoveTemp(PendingPhotoFile);PendingPhotoFile.Reset();
 if(File.IsEmpty() || Progress.State!=1 || Progress.ActivityStage!=0 || Pixels.Num()!=Width*Height)return;
 TArray<FColor> Image;
 if(Width==1600 && Height==900)Image=Pixels;
 else FImageUtils::ImageResize(Width,Height,Pixels,1600,900,Image,true);
 for(auto& Pixel:Image)Pixel.A=255;
 TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1600,900,TArrayView64<const FColor>(Image.GetData(),Image.Num()),PNG);
 if(!FFileHelper::SaveArrayToFile(PNG,*File)){Notify(FText::FromString(TEXT("Photograph could not be saved. You can retry.")));return;}
 Progress.ActivityPhotoFile=File;SetProgressStage(1);Notify(FText::FromString(TEXT("Photograph saved. Review the file and return to the clerk to submit it.")));
}
void AIslandActivityJob::RestorePhotoView()
{
 if(bPhotoSubjectHiddenByUs && PhotoViewOwner.IsValid() && PhotoSubject.IsValid())PhotoViewOwner->HiddenActors.Remove(PhotoSubject.Get());
 PhotoViewOwner.Reset();PhotoSubject.Reset();bPhotoSubjectHiddenByUs=false;
}
bool AIslandActivityJob::Complete(bool FishAlreadyPaid)
{
 if(Progress.State!=1 || !Progress.ActivityRunID.IsValid())return false;
 auto* Economy=GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();const FName Receipt=GetReceiptID();
 if(!FishAlreadyPaid && !Economy->CreditOnce(Reward,Receipt) && !Economy->HasCredit(Receipt))return false;
 if(FishAlreadyPaid && !Economy->HasCredit(Receipt))return false;
 Progress.State=2;++Progress.ActivityCompletions;
 if(Progress.ActivitySeconds>0 && (Progress.ActivityBestSeconds==0 || Progress.ActivitySeconds<Progress.ActivityBestSeconds))Progress.ActivityBestSeconds=Progress.ActivitySeconds;
 ClearPresentation();UpdateNavigation();GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->NotifyChanged(this);Notify(FText::FromString(TEXT("Activity complete. Reward recorded once in your existing wallet.")));return true;
}
void AIslandActivityJob::Cancel(const FText& Reason)
{
 if(Progress.State!=1)return;Progress.State=3;ClearPresentation();UpdateNavigation();GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->NotifyChanged(this);Notify(Reason);
}
void AIslandActivityJob::ClearPresentation()
{
 RestorePhotoView();
 const bool OwnedScreenshot=PhotoCaptureHandle.IsValid();
 if(PhotoCaptureHandle.IsValid()){UGameViewportClient::OnScreenshotCaptured().Remove(PhotoCaptureHandle);PhotoCaptureHandle.Reset();}
 if(!PendingPhotoFile.IsEmpty()){if(OwnedScreenshot)FScreenshotRequest::Reset();PendingPhotoFile.Reset();}
 if(Passenger){Passenger->HideDriver();Passenger->DestroyComponent();Passenger=nullptr;}
 if(Guest){Guest->Destroy();Guest=nullptr;}if(Cargo){Cargo->DestroyComponent();Cargo=nullptr;}
 Transport.Reset();bGuestBoarded=false;bGuestExiting=false;
 SetActorTickInterval(.1f);
}
void AIslandActivityJob::Tick(float Delta)
{
 Super::Tick(Delta);
 if(!PendingPhotoFile.IsEmpty() && !PhotoCaptureHandle.IsValid() && GetWorld()->GetTimeSeconds()-PhotoRequestedAt>=.8f && !FScreenshotRequest::IsScreenshotRequested())
 {
  PhotoCaptureHandle=UGameViewportClient::OnScreenshotCaptured().AddUObject(this,&AIslandActivityJob::PhotographCaptured);
  FScreenshotRequest::RequestScreenshot(PendingPhotoFile,false,false,false,FIntRect(),true);
 }
 if(!PendingPhotoFile.IsEmpty() && GetWorld()->GetTimeSeconds()-PhotoRequestedAt>5)
 {
  RestorePhotoView();if(PhotoCaptureHandle.IsValid()){UGameViewportClient::OnScreenshotCaptured().Remove(PhotoCaptureHandle);PhotoCaptureHandle.Reset();}
  FScreenshotRequest::Reset();PendingPhotoFile.Reset();Notify(FText::FromString(TEXT("Photograph capture interrupted. You can retry.")));
 }
 if(Progress.State!=1)return;auto* PC=Customer.Get();if(!PC)PC=GetWorld()->GetFirstPlayerController();APawn* Pawn=PC?PC->GetPawn().Get():nullptr;if(!Pawn)return;
 Customer=PC;Progress.ActivitySeconds+=Delta;
 KeepRescueGuestAfloat(Delta);
 if(Progress.ActivitySeconds>MaximumSeconds || (Guest && Guest->GetHealthComponent()->IsDead())){Cancel(FText::FromString(TEXT("Activity failed: timeout or guest injured. You can accept a fresh run.")));return;}
 if((Kind==ENeonCityIslandActivity::Taxi || Kind==ENeonCityIslandActivity::SeaRescue) && Progress.ActivityStage==1)
 {
  if(!Transport.IsValid()){Cancel(FText::FromString(TEXT("Transport unavailable. Return to the pickup and retry.")));return;}
  if(!bGuestBoarded)
  {
   if(Transport->GetVelocity().Size()>80){Cancel(FText::FromString(TEXT("Keep the transport stopped while the guest boards.")));return;}
   const FVector Door=Transport->GetActorLocation()+Transport->GetActorRightVector()*145;
   if(MoveGuest(Door,Delta) && BoardGuest(Transport.Get(),Kind==ENeonCityIslandActivity::SeaRescue))SetProgressStage(2);
  }
 }
 if(bGuestExiting && MoveGuest(PointLocation(1)+FVector(300,0,95),Delta)){Complete();return;}
 if((Kind==ENeonCityIslandActivity::Delivery || Kind==ENeonCityIslandActivity::DiveSalvage) && Progress.ActivityStage==1)
 {
  if(Cargo && Cargo->GetAttachParent()!=Pawn->GetRootComponent())Cargo->AttachToComponent(Pawn->GetRootComponent(),FAttachmentTransformRules::KeepRelativeTransform);
  if((Kind==ENeonCityIslandActivity::DiveSalvage || Pawn->IsA<ANeonCityBaseVehicle>()) && FVector::Dist2D(Pawn->GetActorLocation(),PointLocation(0))>1500)Progress.ActivityCheckpoint=1;
 }
 if(Progress.ActivityStage==1 && (Kind==ENeonCityIslandActivity::GuidedTour || Kind==ENeonCityIslandActivity::TimedHike || Kind==ENeonCityIslandActivity::WingsuitChallenge))
 {
  if(Kind==ENeonCityIslandActivity::GuidedTour && Guest)
  {
   const float Gap=FVector::Dist2D(Guest->GetActorLocation(),Pawn->GetActorLocation());
   if(Gap>1300){if(NoticeClock<=0){Notify(FText::FromString(TEXT("Your visitor is waiting. Walk back to them to resume.")));NoticeClock=8;}NoticeClock-=Delta;return;}
   MoveGuest(Pawn->GetActorLocation()-Pawn->GetActorForwardVector()*130,Delta);
  }
  if(RoutePointsLocal.IsValidIndex(Progress.ActivityCheckpoint))
  {
   const FVector Goal=GetActorTransform().TransformPosition(RoutePointsLocal[Progress.ActivityCheckpoint]);
   const auto* Walker=Cast<ANeonCityPlayerCharacter>(Pawn);
   const bool CorrectMovement=Kind==ENeonCityIslandActivity::WingsuitChallenge?
    (Walker && Walker->FindComponentByClass<UWingsuitComponent>()->IsFlying()):
    (Walker && Walker->GetCharacterMovement()->IsMovingOnGround());
   const bool Near=CorrectMovement && FVector::Dist(Pawn->GetActorLocation(),Goal)<250 && (!Guest || FVector::Dist2D(Guest->GetActorLocation(),Goal)<450);
   WaitSeconds=Near?WaitSeconds+Delta:0;
   if(WaitSeconds>(Kind==ENeonCityIslandActivity::GuidedTour?4.f:.2f)){++Progress.ActivityCheckpoint;WaitSeconds=0;UpdateNavigation();}
  }
  else if(Kind==ENeonCityIslandActivity::GuidedTour && FVector::Dist2D(Pawn->GetActorLocation(),PointLocation(1))<240)Complete();
 }
 LastPlayerLocation=Pawn->GetActorLocation();
}
FNeonCityOpportunitySaveData AIslandActivityJob::BuildOpportunitySnapshot(bool Defaults) const
{
 if(Defaults){FNeonCityOpportunitySaveData D;D.OpportunityID=ActivityID;return D;}
 auto D=Progress;
 // Progress may have come from a previous active save. Terminal snapshots must
 // not carry its old passenger-only extension into State2/3 validation.
 D.VehicleID=FGuid();D.ActivityTransportPickup=FVector::ZeroVector;D.ActivityTransportPickupValid=false;D.ActivityPassengerExiting=false;
 if(Guest){D.ActivityNPCPosition=Guest->GetActorLocation();D.ActivityNPCPositionValid=true;}
 if(D.State==1 && D.ActivityStage>0 && (Kind==ENeonCityIslandActivity::Taxi || Kind==ENeonCityIslandActivity::SeaRescue) && Transport.IsValid())
 {
  if(const auto* Car=Cast<ANeonCityBaseVehicle>(Transport.Get()))D.VehicleID=Car->VehicleID;
  else if(const auto* Boat=Cast<ANeonCityMarineVehicleBase>(Transport.Get()))D.VehicleID=Boat->VehicleID;
  D.ActivityTransportPickup=TransportPickup;D.ActivityTransportPickupValid=D.VehicleID.IsValid();D.ActivityPassengerExiting=bGuestExiting;
 }
 return D;
}
bool AIslandActivityJob::IsValidOpportunitySnapshot(const FNeonCityOpportunitySaveData& D) const
{
 return D.OpportunityID==ActivityID && D.State<=4 && D.ActivityStage<=2 && D.ActivityCheckpoint>=0 && D.ActivityCheckpoint<=FMath::Max(1,RoutePointsLocal.Num())
 && FMath::IsFinite(D.ActivitySeconds) && D.ActivitySeconds>=0 && FMath::IsFinite(D.ActivityBestSeconds) && D.ActivityBestSeconds>=0 && D.ActivityCompletions>=0
 && !D.ActivityNPCPosition.ContainsNaN() && !D.ActivityTransportPickup.ContainsNaN() && (D.State!=1 || D.ActivityRunID.IsValid())
 && (!D.ActivityTransportPickupValid || (D.VehicleID.IsValid() && D.ActivityNPCPositionValid && D.State==1 && D.ActivityStage>0 && (Kind==ENeonCityIslandActivity::Taxi || Kind==ENeonCityIslandActivity::SeaRescue)))
 && (!D.ActivityPassengerExiting || (D.ActivityTransportPickupValid && D.ActivityStage==2))
 && (D.ActivityPhotoFile.IsEmpty() || (FPaths::IsUnderDirectory(D.ActivityPhotoFile,FPaths::ProjectSavedDir()/TEXT("IslandActivities/Photos"))));
}
void AIslandActivityJob::ApplyOpportunitySnapshot(const FNeonCityOpportunitySaveData& D)
{
 if(!IsValidOpportunitySnapshot(D))return;ClearPresentation();Progress=D;Customer=GetWorld()->GetFirstPlayerController();
 if(Progress.State==1)
 {
  if(Kind==ENeonCityIslandActivity::Taxi || Kind==ENeonCityIslandActivity::SeaRescue)
  {
   const bool Boat=Kind==ENeonCityIslandActivity::SeaRescue;
   if(D.ActivityStage>0 && D.ActivityTransportPickupValid)
   {
    // SaveCoordinator restores the actual vehicle/boat domain before Phone.
    // Preserve this run and its actual transport; no guest or reward is granted
    // by the test harness, and no vehicle pose is changed by activity restore.
    APawn* Restored=nullptr;int32 Matches=0;
    for(TActorIterator<APawn> It(GetWorld());It;++It)
    {
     FGuid ID;
     if(Boat){if(const auto* B=Cast<ANeonCityMarineVehicleBase>(*It))ID=B->VehicleID;}
     else if(const auto* C=Cast<ANeonCityBaseVehicle>(*It))ID=C->VehicleID;
     if(ID==D.VehicleID){Restored=*It;++Matches;}
    }
    if(Matches!=1 || !Restored || Restored->GetVelocity().Size()>80)
    {Cancel(FText::FromString(TEXT("Saved transport is unavailable or unsafe. The run was cancelled without payment.")));return;}
    Transport=Restored;TransportPickup=D.ActivityTransportPickup;
    if(D.ActivityStage==1 || D.ActivityPassengerExiting)
    {
     CreateGuest(D.ActivityNPCPosition,Boat && !D.ActivityPassengerExiting);
     if(!Guest){Cancel(FText::FromString(TEXT("Saved visitor no longer has clear standing space. No reward was paid.")));return;}
     bGuestExiting=D.ActivityPassengerExiting;
    }
    else
    {
     // A seated guest's saved attached origin lies inside the hull. Spawn its
     // ordinary character beside the restored transport before using the same
     // production passenger presentation, avoiding a colliding spawn inside it.
     CreateGuest(Restored->GetActorLocation()+Restored->GetActorRightVector()*260+FVector(0,0,92),Boat);
     if(!Guest || !BoardGuest(Restored,Boat))
     {Cancel(FText::FromString(TEXT("Saved passenger could not be restored safely. No reward was paid.")));return;}
     TransportPickup=D.ActivityTransportPickup;
    }
   }
   else
   {
    // Tagged fields are absent from old saves. Retain their existing safe
    // pickup recovery, but preserve genuine stage0 visitor positions when valid.
    Progress.ActivityStage=0;CreateGuest(D.ActivityStage==0 && D.ActivityNPCPositionValid?D.ActivityNPCPosition:PointLocation(0)+FVector(100,80,92),Boat);
   }
  }
  else if(Kind==ENeonCityIslandActivity::GuidedTour)CreateGuest(D.ActivityNPCPositionValid?D.ActivityNPCPosition:PointLocation(0)+FVector(100,80,92));
  else if(Progress.ActivityStage==1 && Progress.ActivityItemID.IsValid() && (Kind==ENeonCityIslandActivity::Delivery || Kind==ENeonCityIslandActivity::DiveSalvage))
  {
   APawn* Pawn=Customer.IsValid()?Customer->GetPawn().Get():nullptr;
   if(Pawn){Cargo=NewObject<UStaticMeshComponent>(this,TEXT("CarriedIslandParcel"));AddInstanceComponent(Cargo);Cargo->SetStaticMesh(PickupMesh.LoadSynchronous());Cargo->SetCollisionEnabled(ECollisionEnabled::NoCollision);Cargo->RegisterComponent();Cargo->AttachToComponent(Pawn->GetRootComponent(),FAttachmentTransformRules::KeepRelativeTransform);Cargo->SetRelativeLocation(FVector(35,0,-10));Cargo->SetRelativeScale3D(FVector(.4));}
   if(Points.IsValidIndex(0))Points[0]->Visual->SetHiddenInGame(true);
  }
 }
 UpdateNavigation();
}
void AIslandActivityJob::EndPlay(EEndPlayReason::Type Reason)
{if(GetGameInstance())GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->UnregisterProvider(this);ClearPresentation();Super::EndPlay(Reason);}
