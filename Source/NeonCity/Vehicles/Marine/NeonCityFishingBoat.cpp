#include "Vehicles/Marine/NeonCityFishingBoat.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/ReferenceBoatGeometry.h"
#include "Vehicles/Marine/ReferenceBoatHydrodynamics.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Combat/CombatCollision.h"
#include "Fishing/FishingStation.h"
#include "Components/TextRenderComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/HealthComponent.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ANeonCityFishingBoat::ANeonCityFishingBoat()
{
 VehicleData=CreateDefaultSubobject<UReferenceFishingBoatData>(TEXT("ReferenceHullData"));
 GetHull()->SetBoxExtent(VehicleData->HullExtent);
 // The physical hull stays simulated. Character floors use the exact source deck/cabin boxes,
 // so the upper half of a single bounding box can never become an invisible cockpit floor.
 GetHull()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
 TArray<UStaticMeshComponent*> OldParts;GetComponents(OldParts);
 for(auto* Part:OldParts) { Part->SetVisibility(false);Part->SetHiddenInGame(true); }
 for(const auto& Part:NeonCityReferenceBoat::Parts)
 {
  const FString Name=FString(TEXT("Reference_"))+Part.Name;
  auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(*Name);Mesh->SetupAttachment(GetHull());
  const FString Path=FString::Printf(TEXT("/Game/NeonCity/Art/Tidewater/Boat/SM_NCTW_FishingBoat_%s.SM_NCTW_FishingBoat_%s"),Part.Name,Part.Name);
  ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(*Path);Mesh->SetStaticMesh(Asset.Object);
  Mesh->SetRelativeLocation(Part.Pivot);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Mesh->SetCanEverAffectNavigation(false);BoatParts.Add(Mesh);
 }
 TArray<UBoxComponent*> ExistingBoxes;GetComponents(ExistingBoxes);
 for(auto* Box:ExistingBoxes) if(Box->GetFName()==TEXT("InteractionBounds")) Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 int32 Index=0;
 for(const auto& Shape:NeonCityReferenceBoat::Colliders)
 {
  auto* Box=CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("Deck_%02d_%s"),Index++,Shape.Tag));
  Box->SetupAttachment(GetHull());Box->InitBoxExtent(Shape.Half);Box->SetRelativeLocation(Shape.Center);
  Box->BodyInstance.bAutoWeld=true;Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionObjectType(ECC_Vehicle);
  Box->SetCollisionResponseToAllChannels(ECR_Ignore);Box->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
  Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Box->SetCollisionResponseToChannel(NeonCityCombat::WeaponTrace,ECR_Block);
  Box->SetGenerateOverlapEvents(false);Box->SetCanEverAffectNavigation(false);
  Box->CanCharacterStepUpOn=Shape.Walkable?ECB_Yes:ECB_No;DeckCollision.Add(Box);
 }
 // Looking into the wheelhouse must select its controls even when the eye ray
 // passes through a clear window above the physical instrument-panel collider.
 // Query-only: this does not add a floor, hull mass or weapon obstruction.
 auto* HelmTarget=CreateDefaultSubobject<UBoxComponent>(TEXT("HelmInteractionTarget"));
 HelmTarget->SetupAttachment(GetHull());HelmTarget->InitBoxExtent(FVector(65,80,65));
 HelmTarget->SetRelativeLocation(FVector(80,20,160));HelmTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 HelmTarget->SetCollisionResponseToAllChannels(ECR_Ignore);HelmTarget->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 HelmTarget->SetGenerateOverlapEvents(false);HelmTarget->SetCanEverAffectNavigation(false);HelmTarget->CanCharacterStepUpOn=ECB_No;
 DeckLight=CreateDefaultSubobject<USpotLightComponent>(TEXT("DeckFloodlight"));DeckLight->SetupAttachment(GetHull());
 DeckLight->SetRelativeLocation(FVector(-91,0,225));DeckLight->SetRelativeRotation(FRotator(-40,180,0));
 DeckLight->SetIntensityUnits(ELightUnits::Lumens);DeckLight->SetIntensity(0);DeckLight->SetAttenuationRadius(800);DeckLight->SetInnerConeAngle(38);DeckLight->SetOuterConeAngle(64);
 DeckLight->SetLightColor(FLinearColor(1.f,.9f,.75f));DeckLight->SetCastShadows(true);
 if(auto* Occupant=FindComponentByClass<UTransportOccupantComponent>())
 {
  Occupant->SeatHipLocation=FVector(60,55,108);
  Occupant->LeftHandTarget=FVector(91,42,132);Occupant->RightHandTarget=FVector(91,68,132);
  Occupant->LeftFootTarget=FVector(65,40,57);Occupant->RightFootTarget=FVector(65,70,57);
 }
}
void ANeonCityFishingBoat::PostInitializeComponents()
{
 Super::PostInitializeComponents();
 // The displacement/inertia model includes the entire boat. Welded contact geometry
 // must not add its solid-box density a second time to the authored 3204 kg hull.
 for(auto Box:DeckCollision) if(auto* Setup=Box->GetBodySetup())
  for(auto& Shape:Setup->AggGeom.BoxElems) Shape.SetContributeToMass(false);
}
void ANeonCityFishingBoat::BeginPlay()
{
 Super::BeginPlay();
 for(auto Part:BoatParts) if(auto* Material=Part->CreateDynamicMaterialInstance(0)) SurfaceMaterials.Add(Material);
 if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())
 {
  for(auto Material:SurfaceMaterials)Lighting->RegisterNightMaterial(Material);
  FNeonCityLocalLightPolicy Policy;Policy.Intensity=1800;Policy.ShadowDistance=2500;Policy.CullDistance=8000;Policy.Priority=1;
  Lighting->RegisterLight(DeckLight,Policy);
 }
 if(HasAuthority() && VehicleID.IsValid())
 {
  const FTransform Local(FRotator(0,90,0),FVector(-170,105,35));const FTransform World=Local*GetActorTransform();
  DeckFishing=GetWorld()->SpawnActorDeferred<AFishingStation>(AFishingStation::StaticClass(),World,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(DeckFishing)
  {
   DeckFishing->StationID=FName(*FString::Printf(TEXT("Marine.%s.Deck"),*VehicleID.ToString(EGuidFormats::Digits)));
   DeckFishing->DisplayName=NSLOCTEXT("NeonCity","HarborlineDeckFishing","Harborline / Deck fishing");
   DeckFishing->Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   DeckFishing->FinishSpawning(World);DeckFishing->AttachToComponent(GetHull(),FAttachmentTransformRules::KeepWorldTransform);
   DeckFishing->SetActorRelativeTransform(Local);
   // Use the same contextual guide as shore fishing, without a sign floating over the rail.
   TArray<UTextRenderComponent*> Labels;DeckFishing->GetComponents(Labels);for(auto* Label:Labels)Label->SetHiddenInGame(true);
  }
 }
}
void ANeonCityFishingBoat::Tick(float Delta)
{
 Super::Tick(Delta);PresentationTime+=Delta;
 const auto T=GetMarineMovement()->GetTelemetry();
 const float Steer=GetMarineMovement()->GetSteering();
 // Source neutral orientation is baked into each mesh. Rotate only around the retained pivot.
 const FVector WheelAxis=NeonCityReferenceBoat::ToUEPolar(FQuat(.25340132485,0,0,.96736123995).RotateVector(FVector(0,0,1)));
 PropAngle=FMath::Fmod(PropAngle+5*FMath::Tanh(T.SignedThrust*8)*2*PI*Delta,2*PI);
 for(auto Part:BoatParts)
 {
  const FName Name=Part->GetFName();
  if(Name==TEXT("Reference_wheel")) Part->SetRelativeRotation(FQuat(WheelAxis,-Steer*.75f*2*PI));
  else if(Name==TEXT("Reference_throttle")) Part->SetRelativeRotation(FQuat(FVector::RightVector,T.SignedThrust*.6));
  else if(Name==TEXT("Reference_rudder")) Part->SetRelativeRotation(FQuat(FVector::UpVector,-Steer*.6));
  else if(Name==TEXT("Reference_propeller")) Part->SetRelativeRotation(FQuat(FVector::ForwardVector,-PropAngle));
  else if(Name==TEXT("Reference_radar")) Part->SetRelativeRotation(FRotator(0,-PresentationTime*144,0));
 }
}
bool ANeonCityFishingBoat::IsOnDeck(const ACharacter* Character) const
{
 const auto* Base=Character?Cast<UPrimitiveComponent>(Character->GetMovementBaseObject()):nullptr;
 return Base && Base->GetOwner()==this;
}
bool ANeonCityFishingBoat::FindBoardingTransform(const ACharacter* Character,FTransform& Out) const
{
 if(!Character)return false;
 const auto* Capsule=Character->GetCapsuleComponent();
 const FVector Feet=GetActorTransform().TransformPosition(GetProfile()->BoardingPoint);
 const FVector Center=Feet+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+3);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BoatBoarding),false,Character);
 if(GetWorld()->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,
  FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query))return false;
 Out=FTransform(FRotator(0,GetActorRotation().Yaw,0),Center);return true;
}
bool ANeonCityFishingBoat::CanBoard(const ACharacter* Character) const
{
 if(!Character || Character->GetWorld()!=GetWorld() || !HasAuthority() || IsOnDeck(Character) || GetController()
  || GetHealthComponent()->IsDead() || GetSpeed_Implementation()>150 || Character->GetAttachParentActor()) return false;
 const auto* PC=Cast<APlayerController>(Character->GetController());if(!PC || !PC->IsLocalController())return false;
 if(auto* Body=Character->FindComponentByClass<UDamageableCharacterComponent>();Body && !Body->CanAct())return false;
 const FVector Local=GetActorTransform().InverseTransformPosition(Character->GetActorLocation());
 const float Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 const float DeckHeight=GetProfile()->BoardingPoint.Z;
 // Measure reach from the swimmer's body to the working deck. The old fixed
 // centre band rejected surface swimmers as a 6-degree wave rolled the deck,
 // despite their eyes and hands already being beside the gunwale.
 if(FMath::Abs(Local.X)>450 || FMath::Abs(Local.Y)>340
  || Local.Z+Half<DeckHeight-60 || Local.Z-Half>DeckHeight+200)return false;
 if(FMath::Abs(GetActorRotation().Roll)>20 || FMath::Abs(GetActorRotation().Pitch)>20)return false;
 FTransform Destination;return FindBoardingTransform(Character,Destination);
}
bool ANeonCityFishingBoat::Board(ACharacter* Character)
{
 FTransform Destination;if(!CanBoard(Character) || !FindBoardingTransform(Character,Destination))return false;
 // One explicit boarding action. All subsequent deck motion uses native based movement.
 Character->SetActorTransform(Destination,false,nullptr,ETeleportType::TeleportPhysics);
 Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
 Character->GetCharacterMovement()->Velocity=GetHull()->GetPhysicsLinearVelocityAtPoint(Destination.GetLocation());
 Character->GetCapsuleComponent()->UpdatePhysicsVolume(true);return true;
}
bool ANeonCityFishingBoat::CanInteract_Implementation(AActor* Actor) const
{
 const auto* Character=Cast<ACharacter>(Actor);
 return IsOnDeck(Character)?GetMarineInteraction()->CanEnter(Actor):CanBoard(Character);
}
FText ANeonCityFishingBoat::GetInteractionName_Implementation() const
{
 const auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
 return PC && IsOnDeck(Cast<ACharacter>(PC->GetPawn()))?NSLOCTEXT("NeonCity","TakeHelm","Take helm"):NSLOCTEXT("NeonCity","BoardWorkingBoat","Board Harborline 82");
}
void ANeonCityFishingBoat::Interact_Implementation(AActor* Actor)
{
 auto* Character=Cast<ACharacter>(Actor);
 if(IsOnDeck(Character)) GetMarineInteraction()->Enter(Actor);else Board(Character);
}
bool ANeonCityFishingBoat::QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const
{
 const auto* Part=Cast<UBoxComponent>(Hit.GetComponent());
 if(!Character || !Part)return false;
 if(Part==GetHull())
 {
  // Root-body contact events from welded shapes still need support-vs-impact classification.
  const FVector Point=GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);bool Floor=false;
  for(const auto& C:NeonCityReferenceBoat::Colliders)
   if(C.Walkable && FMath::Abs(Point.X-C.Center.X)<=C.Half.X+2 && FMath::Abs(Point.Y-C.Center.Y)<=C.Half.Y+2
    && FMath::Abs(Point.Z-C.Center.Z-C.Half.Z)<8) { Floor=true;break; }
  if(!Floor)return false;
 }
 else if(!DeckCollision.Contains(Part) || Part->CanCharacterStepUpOn!=ECB_Yes)return false;
 if(Part->GetComponentTransform().InverseTransformVectorNoScale(Hit.ImpactNormal).Z<.95
  || Hit.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ()) return false;
 Surface=FDynamicStandingSurface();Surface.PointVelocity=GetHull()->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);return true;
}

void ANeonCityFishingBoat::EndPlay(EEndPlayReason::Type Reason)
{
 if(Reason==EEndPlayReason::Destroyed && IsValid(DeckFishing))DeckFishing->Destroy();
 Super::EndPlay(Reason);
}
