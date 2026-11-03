#include "Adventure/IslandEstateAmenity.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Property/PropertySubsystem.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Fishing/NeonCityFishingSubsystem.h"

AIslandEstateAmenity::AIslandEstateAmenity()
{
 PrimaryActorTick.bStartWithTickEnabled=false;PrimaryActorTick.TickInterval=.033333f;
 auto MakePart=[&](const TCHAR* Name){auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(RootComponent);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);return C;};
 HeldObject=MakePart(TEXT("MatchedUseProp"));WaterSurface=MakePart(TEXT("FixtureWater"));WaterStream=MakePart(TEXT("FaucetStream"));MovingPart=MakePart(TEXT("MovingFixturePart"));
 Details=CreateDefaultSubobject<UTextRenderComponent>(TEXT("ActualPageOrFixtureStatus"));Details->SetupAttachment(RootComponent);Details->SetWorldSize(3.6f);Details->SetHorizontalAlignment(EHTA_Center);Details->SetTextRenderColor(FColor(206,207,194));
 Feedback=CreateDefaultSubobject<UAudioComponent>(TEXT("SpatialFixtureFeedback"));Feedback->SetupAttachment(RootComponent);Feedback->bAutoActivate=false;Feedback->bAllowSpatialization=true;Feedback->SetVolumeMultiplier(.13f);
 FSoundAttenuationSettings Att;Att.bAttenuate=true;Att.bSpatialize=true;Att.AttenuationShapeExtents=FVector(110);Att.FalloffDistance=650;Feedback->AdjustAttenuation(Att);
 TaskBody=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("ExistingRigTaskPose"));TaskBody->SetupAttachment(RootComponent);TaskBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);TaskBody->SetCanEverAffectNavigation(false);TaskBody->SetVisibility(false);TaskBody->SetComponentTickEnabled(false);
}
bool AIslandEstateAmenity::HasPresentation()const
{
 if(AmenityID.IsNone() || !IsValid(TargetFixture))return false;
 if(Kind==EIslandEstateAmenityKind::Read)return HeldObject->GetStaticMesh() && !Pages.IsEmpty();
 if(Kind==EIslandEstateAmenityKind::Drink)return HeldObject->GetStaticMesh() && WaterSurface->GetStaticMesh();
 if(Kind==EIslandEstateAmenityKind::Bath)return WaterSurface->GetStaticMesh() && WaterStream->GetStaticMesh();
 if(Kind==EIslandEstateAmenityKind::Wash || Kind==EIslandEstateAmenityKind::Shower)return WaterStream->GetStaticMesh()!=nullptr;
 if(Kind==EIslandEstateAmenityKind::Storage)return MovingPart->GetStaticMesh() && HeldObject->GetStaticMesh();
 if(Kind==EIslandEstateAmenityKind::Meal)return WaterSurface->GetStaticMesh()!=nullptr;
 return !Pages.IsEmpty();
}
bool AIslandEstateAmenity::CanInteract_Implementation(AActor* Visitor)const
{
 if(!HasPresentation())return false;
 const auto* P=Cast<ANeonCityPlayerCharacter>(Visitor);const auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 if(!P || !PC || PC->GetPawn()!=P || !PC->IsLocalController() || !HasAuthority() || P->GetHealthComponent()->IsDead())return false;
 if(IsUsing())return ActivePlayer.Get()==P;
 for(TActorIterator<AIslandEstateAmenity> Other(GetWorld());Other;++Other)
  if(*Other!=this && Other->IsUsing() && Other->ActivePlayer.Get()==P)return false;
 if(!GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID))return false;
 if(const auto* B=P->FindComponentByClass<UDamageableCharacterComponent>();B && !B->CanAct())return false;
 if(const auto* D=P->FindComponentByClass<UDivingComponent>();D && (D->IsEquipped() || D->IsChangingGear()))return false;
 if(const auto* W=P->FindComponentByClass<UWingsuitComponent>();W && W->IsEquipped())return false;
 if(const auto* W=P->GetCombatComponent()->GetWeapon();W && W->IsEquipped())return false;
 return bUseSeatedPose?Super::CanInteract_Implementation(Visitor):!P->GetAttachParentActor() && P->GetCharacterMovement()->IsMovingOnGround() && FVector::DistSquared(P->GetActorLocation(),GetActorLocation())<FMath::Square(240.f);
}
bool AIslandEstateAmenity::CanReachFixture(const ANeonCityPlayerCharacter* P)const
{
 if(!P)return false;
 if(bUseSeatedPose)return true; // HomeSeat validates its real seated/exit anchor.
 if(Kind!=EIslandEstateAmenityKind::Wash && Kind!=EIslandEstateAmenityKind::Shower && !bRequireFittedStance)return true;
 const FVector Feet=P->GetActorLocation()-FVector(0,0,P->GetSimpleCollisionHalfHeight());
 const FVector Stand=GetActorTransform().TransformPosition(StandLocal),Face=GetActorTransform().TransformVectorNoScale(FacingLocal).GetSafeNormal2D();
 if(FVector::Dist2D(Feet,Stand)>StandToleranceCm || FMath::Abs(Feet.Z-Stand.Z)>15 || FVector::DotProduct(P->GetActorForwardVector().GetSafeNormal2D(),Face)<.88f)return false;
 if(Kind!=EIslandEstateAmenityKind::Wash)return true;
 const auto* Mesh=P->GetMesh();if(!Mesh || Mesh->GetBoneIndex(TEXT("spine_01"))==INDEX_NONE)return false;
 const FVector Spine=Mesh->GetSocketLocation(TEXT("spine_01"));const FQuat Lean(FVector::CrossProduct(FVector::UpVector,P->GetActorForwardVector()).GetSafeNormal(),FMath::DegreesToRadians(WashLeanDegrees));
 for(int Side=0;Side<2;++Side)
 {
  const FName Upper=Side?TEXT("upperarm_l"):TEXT("upperarm_r"),Lower=Side?TEXT("lowerarm_l"):TEXT("lowerarm_r"),Hand=Side?TEXT("hand_l"):TEXT("hand_r");
  if(Mesh->GetBoneIndex(Upper)==INDEX_NONE || Mesh->GetBoneIndex(Lower)==INDEX_NONE || Mesh->GetBoneIndex(Hand)==INDEX_NONE)return false;
  const FVector Shoulder=Mesh->GetSocketLocation(Upper),Elbow=Mesh->GetSocketLocation(Lower),Wrist=Mesh->GetSocketLocation(Hand);
  const float Reach=FVector::Dist(Shoulder,Elbow)+FVector::Dist(Elbow,Wrist)-1.f;
  const FVector BentShoulder=Spine+Lean.RotateVector(Shoulder-Spine),Target=GetActorTransform().TransformPosition(Side?LeftHandLocal:RightHandLocal);
  if(FVector::Dist(BentShoulder,Target)>Reach)return false;
 }
 return true;
}
FText AIslandEstateAmenity::GetInteractionName_Implementation()const
{
 const FString Name=AmenityName.IsEmpty()?SeatName.ToString():AmenityName.ToString();
 if(IsUsing())return FText::FromString((Kind==EIslandEstateAmenityKind::Read || Kind==EIslandEstateAmenityKind::Inspect)?FString::Printf(TEXT("%s | Left/Right pages | F or movement finish | %.0fs"),*Name,UseSeconds):FString::Printf(TEXT("%s | F or movement finish | %.0fs"),*Name,UseSeconds));
 if(Kind==EIslandEstateAmenityKind::Meal)
 {const auto* Food=GetGameInstance()?GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>():nullptr;if(!Food || Food->GetMeals().IsEmpty())return FText::FromString(TEXT("Dining — bring a stored cooked meal from the kitchen / BBQ"));}
 if(!bUseSeatedPose && (Kind==EIslandEstateAmenityKind::Wash || Kind==EIslandEstateAmenityKind::Shower || bRequireFittedStance))
 {const auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;const auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!CanReachFixture(P))return FText::FromString(bRequireFittedStance?TEXT("Approach the front of the fixture and face it | F checks reach"):TEXT("Approach the basin and face the faucet | F checks reach"));}
 return FText::FromString(TEXT("F ")+Name);
}
void AIslandEstateAmenity::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor))return;
 if(IsUsing()){FinishUse(false);return;}
 auto* Player=CastChecked<ANeonCityPlayerCharacter>(Visitor);
 if(!CanReachFixture(Player)){if(auto* PC=Cast<APlayerController>(Player->GetController()))if(auto* N=PC->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(FText::FromString(bRequireFittedStance?TEXT("Walk to the front of the fixture and face it before using it."):TEXT("Walk to the front of the basin and face the faucet. Both hands must reach the water.")),5);return;}
 if(Kind==EIslandEstateAmenityKind::Meal)
 {
  auto* Food=GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
  if(!Food || Food->GetMeals().IsEmpty() || (Player->GetHealthComponent()->GetCurrentHealth()>=Player->GetHealthComponent()->GetMaxHealth() && Player->GetPlayerDataComponent()->GetStamina()>=100))
  {if(auto* PC=Cast<APlayerController>(Player->GetController()))if(auto* N=PC->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(FText::FromString(TEXT("Bring a stored cooked meal when you need health or stamina. Your food stays in your inventory.")),5);return;}
  PendingMeal=Food->GetMeals()[0].MealID;const auto* Species=Food->FindSpecies(Food->GetMeals()[0].SpeciesID);auto* Mesh=Species?Species->Mesh.LoadSynchronous():nullptr;if(!Mesh)return;
  HeldObject->SetStaticMesh(Mesh);HeldObject->EmptyOverrideMaterials();const FString SpeciesKey=Species->SpeciesID.ToString().Replace(TEXT("."),TEXT("_"));
  for(int32 I=0;I<Mesh->GetStaticMaterials().Num();++I){const FString Path=FString::Printf(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/M_GrillFish_%s_%d.M_GrillFish_%s_%d"),*SpeciesKey,I,*SpeciesKey,I);if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,*Path)){auto* Cooked=UMaterialInstanceDynamic::Create(Mat,this);Cooked->SetScalarParameterValue(TEXT("CookFraction"),1);HeldObject->SetMaterial(I,Cooked);}}
  const auto B=Mesh->GetBounds();HeldObject->SetRelativeScale3D(FVector(28.f/FMath::Max(1.f,2*B.BoxExtent.X)));bMealConsumed=false;
 }
 if(bUseSeatedPose){Super::Interact_Implementation(Visitor);if(!IsOccupied())return;}
 BeginUse(CastChecked<ANeonCityPlayerCharacter>(Visitor));
}
void AIslandEstateAmenity::BeginUse(ANeonCityPlayerCharacter* Player)
{
 ActivePlayer=Player;StartingLocation=Player->GetActorLocation();UseSeconds=WaterFraction=0;DrinkFraction=1;PageIndex=0;Stage=EIslandEstateAmenityStage::Entering;bLeftBefore=bRightBefore=false;
 PoseSource=bUseSeatedPose?Occupant->GetDriverMesh():Player->GetMesh();bRestoreMesh=false;
 if((Kind==EIslandEstateAmenityKind::Read || Kind==EIslandEstateAmenityKind::Drink || Kind==EIslandEstateAmenityKind::Wash || Kind==EIslandEstateAmenityKind::Meal) && PoseSource.IsValid())
 {
  auto* Source=PoseSource.Get();bSourceVisible=Source->IsVisible();TaskBody->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset());TaskBody->SetWorldTransform(Source->GetComponentTransform());TaskBody->EmptyOverrideMaterials();
  for(int32 I=0;I<Source->GetNumMaterials();++I)TaskBody->SetMaterial(I,Source->GetMaterial(I));
  TaskBody->CopyPoseFromSkeletalComponent(Source);SourceTickBefore=uint8(Source->VisibilityBasedAnimTickOption);Source->SetVisibility(false,false);Source->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;TaskBody->SetVisibility(true);bRestoreMesh=true;
 }
 HeldObject->SetVisibility(Kind==EIslandEstateAmenityKind::Read || Kind==EIslandEstateAmenityKind::Drink || Kind==EIslandEstateAmenityKind::Meal,false);WaterStream->SetVisibility(false);WaterSurface->SetVisibility(Kind==EIslandEstateAmenityKind::Meal);Details->SetVisibility(true);MovingPartClosed=MovingPart->GetRelativeTransform();
 if(Sound){Sound->ResetAudio();Sound->QueueAudio(reinterpret_cast<const uint8*>(SoundPCM.GetData()),SoundPCM.Num()*sizeof(int16));Feedback->Play();}
 SetActorTickEnabled(true);Present(0);Notice(GetInteractionName_Implementation());
}
bool AIslandEstateAmenity::FinishUse(bool Completed)
{
 if(!IsUsing())return true;
 if(bUseSeatedPose && IsOccupied() && !Leave())return false;
 if(Completed)++CompletedUses;
 Cleanup();return true;
}
void AIslandEstateAmenity::Cleanup()
{
 if(bRestoreMesh && PoseSource.IsValid()){PoseSource->SetVisibility(bSourceVisible,false);PoseSource->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption(SourceTickBefore);}
 bRestoreMesh=false;PoseSource.Reset();TaskBody->SetVisibility(false);TaskBody->SetSkinnedAssetAndUpdate(nullptr);
 HeldObject->SetVisibility(false);WaterStream->SetVisibility(false);WaterStream->SetRelativeScale3D(StreamScaleBefore);WaterSurface->SetVisibility(false);Details->SetVisibility(false);MovingPart->SetRelativeTransform(MovingPartClosed);Feedback->Stop();if(Sound)Sound->ResetAudio();
 Stage=EIslandEstateAmenityStage::Idle;ActivePlayer.Reset();WaterFraction=0;PendingMeal.Invalidate();SetActorTickEnabled(false);
}
void AIslandEstateAmenity::Tick(float Delta)
{
 // HomeSeat owns supported cancellation/save exit. Only genuine seats/baths
 // use its rest behavior; standing fixtures never award seated rest.
 if(bUseSeatedPose)Super::Tick(Delta);else AActor::Tick(Delta);
 auto* P=ActivePlayer.Get();auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 if(!IsUsing())return;
 // If the real standing exit is temporarily blocked, retain the lifecycle
 // until HomeSeat can release it. Never suspend its tick while still attached.
 if(bUseSeatedPose && IsOccupied() && P && P->GetHealthComponent()->IsDead()){FinishUse(false);return;}
 if(!P || !PC || PC->GetPawn()!=P || P->GetHealthComponent()->IsDead() || (bUseSeatedPose && !IsOccupied())){Cleanup();return;}
 if(!bUseSeatedPose && (FVector::DistSquared(P->GetActorLocation(),StartingLocation)>FMath::Square(35.f) || P->GetCharacterMovement()->IsFalling() || P->GetCharacterMovement()->IsSwimming())){FinishUse(false);return;}
 if(!bUseSeatedPose && !CanReachFixture(P)){FinishUse(false);return;}
 for(const auto K:{EKeys::W,EKeys::A,EKeys::S,EKeys::D,EKeys::SpaceBar,EKeys::Escape,EKeys::F6,EKeys::F7})if(PC->IsInputKeyDown(K)){FinishUse(false);return;}
 const bool Left=PC->IsInputKeyDown(EKeys::Left),Right=PC->IsInputKeyDown(EKeys::Right);
 if((Kind==EIslandEstateAmenityKind::Read || Kind==EIslandEstateAmenityKind::Inspect) && !Pages.IsEmpty())
 {if(Left && !bLeftBefore)PageIndex=(PageIndex+Pages.Num()-1)%Pages.Num();if(Right && !bRightBefore)PageIndex=(PageIndex+1)%Pages.Num();}
 bLeftBefore=Left;bRightBefore=Right;UseSeconds+=Delta;
 Stage=UseSeconds<1?EIslandEstateAmenityStage::Entering:UseSeconds>SessionSeconds-2?EIslandEstateAmenityStage::Leaving:EIslandEstateAmenityStage::Active;
 Present(Delta);
 if(UseSeconds>=SessionSeconds){Notice(FText::FromString(AmenityName.ToString()+TEXT(" finished. Normal walking restored.")));FinishUse(true);}
}
void AIslandEstateAmenity::Present(float Delta)
{
 const bool Wet=Kind==EIslandEstateAmenityKind::Wash || Kind==EIslandEstateAmenityKind::Shower || Kind==EIslandEstateAmenityKind::Bath;
 const float In=FMath::Clamp(UseSeconds,0.f,1.f),Out=FMath::Clamp((SessionSeconds-UseSeconds)/2.f,0.f,1.f);
 if(Wet)
 {
  WaterFraction=Kind==EIslandEstateAmenityKind::Bath?FMath::Clamp((UseSeconds-1)/6.f,0.f,1.f)*Out:In*Out;
  WaterStream->SetVisibility(UseSeconds>.4f && UseSeconds<SessionSeconds-2);WaterStream->SetRelativeScale3D(StreamScaleBefore*FVector(.88f+.06f*FMath::Sin(UseSeconds*19),1,1));
  WaterSurface->SetVisibility(Kind==EIslandEstateAmenityKind::Bath && WaterFraction>.01f);WaterSurface->SetRelativeLocation(FMath::Lerp(WaterEmptyLocal,WaterFullLocal,WaterFraction));
  Details->SetText(FText::FromString(Kind==EIslandEstateAmenityKind::Bath?FString::Printf(TEXT("WARM BATH 38 C\n%s %.0f%%"),Stage==EIslandEstateAmenityStage::Leaving?TEXT("DRAIN"):WaterFraction<.99f?TEXT("FILL"):TEXT("REST"),WaterFraction*100):Kind==EIslandEstateAmenityKind::Wash?TEXT("RUNNING WATER\nWASH / RINSE"):TEXT("WARM SHOWER\n38 C")));
 }
 else if(Kind==EIslandEstateAmenityKind::Read || Kind==EIslandEstateAmenityKind::Inspect)
 {
  Details->SetText(Pages.IsEmpty()?FText():FText::FromString(FString::Printf(TEXT("%d / %d\n%s"),PageIndex+1,Pages.Num(),*Pages[PageIndex].ToString())));
  HeldObject->SetRelativeLocation(ObjectRestLocal+FVector(0,0,In*4*Out));HeldObject->SetRelativeRotation(FRotator(-12+2*FMath::Sin(UseSeconds*.7f),0,0));
 }
 else if(Kind==EIslandEstateAmenityKind::Drink)
 {
  const float Raise=FMath::Clamp((UseSeconds-1)/2,0.f,1.f)*FMath::Clamp((8-UseSeconds)/2,0.f,1.f);
  DrinkFraction=1-FMath::Clamp((UseSeconds-3)/4.f,0.f,1.f);
  HeldObject->SetRelativeLocation(ObjectRestLocal+FVector(-8*Raise,0,21*Raise));HeldObject->SetRelativeRotation(FRotator(-30*Raise,0,0));
  WaterSurface->SetVisibility(DrinkFraction>.01f);WaterSurface->SetRelativeLocation(FMath::Lerp(WaterEmptyLocal,WaterFullLocal,DrinkFraction));
  if(auto* Mat=Cast<UMaterialInstanceDynamic>(HeldObject->GetMaterial(0)))Mat->SetScalarParameterValue(TEXT("FillFraction"),DrinkFraction);
  Details->SetText(FText::FromString(FString::Printf(TEXT("%s\n%.0f%%"),*AmenityName.ToString(),DrinkFraction*100)));
 }
 else if(Kind==EIslandEstateAmenityKind::Storage)
 {
  auto T=MovingPartClosed;T.AddToTranslation(OpenPartOffset*In*Out);MovingPart->SetRelativeTransform(T);HeldObject->SetVisibility(In*Out>.6f);Details->SetText(Pages.IsEmpty()?FText::FromString(TEXT("LINEN / HOUSE SUPPLIES")):Pages[0]);
 }
 else if(Kind==EIslandEstateAmenityKind::Meal)
 {
  HeldObject->SetRelativeLocation(ObjectRestLocal);WaterSurface->SetRelativeLocation(ObjectRestLocal-FVector(0,0,3));
  if(!bMealConsumed && UseSeconds>=4)
  {auto* Food=GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();if(!Food || !Food->EatMeal(PendingMeal,ActivePlayer.Get())){Notice(FText::FromString(TEXT("Meal kept: you are full or it is no longer in your food inventory.")));FinishUse(false);return;}bMealConsumed=true;}
  HeldObject->SetVisibility(!bMealConsumed);Details->SetText(FText::FromString(bMealConsumed?TEXT("MEAL ENJOYED\nHealth / stamina restored"):TEXT("YOUR STORED GRILLED FISH\nF leaves before eating")));
 }
 if(bRestoreMesh)PoseHands();
 if(auto* P=ActivePlayer.Get())if(auto* PC=Cast<APlayerController>(P->GetController())){FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);Details->SetWorldRotation((Eye-Details->GetComponentLocation()).Rotation());}
 if(Sound && Feedback->IsPlaying() && Sound->GetAvailableAudioByteCount()<12000)Sound->QueueAudio(reinterpret_cast<const uint8*>(SoundPCM.GetData()),SoundPCM.Num()*sizeof(int16));
}
void AIslandEstateAmenity::FitArm(FName Upper,FName Lower,FName Hand,const FVector& Target,const FVector& Pole)
{
 const FVector Shoulder=TaskBody->GetBoneLocationByName(Upper,EBoneSpaces::WorldSpace),Elbow=TaskBody->GetBoneLocationByName(Lower,EBoneSpaces::WorldSpace),Wrist=TaskBody->GetBoneLocationByName(Hand,EBoneSpaces::WorldSpace);
 const float A=FVector::Dist(Shoulder,Elbow),B=FVector::Dist(Elbow,Wrist);if(A<1 || B<1)return;
 const FVector Dir=(Target-Shoulder).GetSafeNormal();const float D=FMath::Clamp(FVector::Dist(Target,Shoulder),FMath::Abs(A-B)+.1f,A+B-.1f),X=(A*A-B*B+D*D)/(2*D);
 FVector Side=(Pole-Shoulder)-Dir*FVector::DotProduct(Pole-Shoulder,Dir);Side.Normalize();const FVector Joint=Shoulder+Dir*X+Side*FMath::Sqrt(FMath::Max(0.f,A*A-X*X));
 auto T=TaskBody->GetBoneTransformByName(Upper,EBoneSpaces::WorldSpace);T.SetRotation(FQuat::FindBetweenVectors(Elbow-Shoulder,Joint-Shoulder)*T.GetRotation());TaskBody->SetBoneTransformByName(Upper,T,EBoneSpaces::WorldSpace);TaskBody->RefreshBoneTransforms();
 const FVector NewElbow=TaskBody->GetBoneLocationByName(Lower,EBoneSpaces::WorldSpace),NewWrist=TaskBody->GetBoneLocationByName(Hand,EBoneSpaces::WorldSpace);T=TaskBody->GetBoneTransformByName(Lower,EBoneSpaces::WorldSpace);T.SetRotation(FQuat::FindBetweenVectors(NewWrist-NewElbow,Shoulder+Dir*D-NewElbow)*T.GetRotation());TaskBody->SetBoneTransformByName(Lower,T,EBoneSpaces::WorldSpace);TaskBody->RefreshBoneTransforms();
}
void AIslandEstateAmenity::PoseHands()
{
 auto* Source=PoseSource.Get();if(!Source)return;TaskBody->SetWorldTransform(Source->GetComponentTransform());TaskBody->CopyPoseFromSkeletalComponent(Source);
 if(Kind==EIslandEstateAmenityKind::Wash && !bUseSeatedPose)if(auto* P=ActivePlayer.Get())
 {auto Spine=TaskBody->GetBoneTransformByName(TEXT("spine_01"),EBoneSpaces::WorldSpace);const FVector Axis=FVector::CrossProduct(FVector::UpVector,P->GetActorForwardVector()).GetSafeNormal();Spine.SetRotation(FQuat(Axis,FMath::DegreesToRadians(WashLeanDegrees))*Spine.GetRotation());TaskBody->SetBoneTransformByName(TEXT("spine_01"),Spine,EBoneSpaces::WorldSpace);TaskBody->RefreshBoneTransforms();}
 FVector Right=RightHandLocal,Left=LeftHandLocal;
 if(Kind==EIslandEstateAmenityKind::Drink)Right+=HeldObject->GetRelativeLocation()-ObjectRestLocal;
 if(Kind==EIslandEstateAmenityKind::Wash){Right.Y+=4*FMath::Sin(UseSeconds*5);Left.Y-=4*FMath::Sin(UseSeconds*5);}
 if(Kind==EIslandEstateAmenityKind::Meal){Right=ObjectRestLocal+FVector(0,10,7);Left=ObjectRestLocal+FVector(0,-15,2);Right.Z+=FMath::Sin(FMath::Clamp((UseSeconds-1)/3.f,0.f,1.f)*PI)*22;}
 FitArm(TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r"),GetActorTransform().TransformPosition(Right),GetActorTransform().TransformPosition(Right+FVector(-20,45,-20)));
 FitArm(TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),GetActorTransform().TransformPosition(Left),GetActorTransform().TransformPosition(Left+FVector(-20,-45,-20)));
}
void AIslandEstateAmenity::PrepareAudio()
{
 constexpr int32 Rate=24000;SoundPCM.SetNum(Rate*2);FRandomStream Random(GetTypeHash(AmenityID));float Low=0;
 const bool Wet=Kind==EIslandEstateAmenityKind::Wash || Kind==EIslandEstateAmenityKind::Shower || Kind==EIslandEstateAmenityKind::Bath;
 for(int32 I=0;I<SoundPCM.Num();++I){const float T=I/float(Rate);Low=.92f*Low+.08f*Random.FRandRange(-1,1);const float Pulse=Wet?.20f:Kind==EIslandEstateAmenityKind::Read?.025f:Kind==EIslandEstateAmenityKind::Drink?.055f:.018f;const float Envelope=FMath::Min(1.f,FMath::Min(T/.04f,(2-T)/.04f));SoundPCM[I]=FMath::RoundToInt(Low*Pulse*Envelope*32767);}
 Sound=NewObject<USoundWaveProcedural>(this);Sound->SetSampleRate(Rate);Sound->NumChannels=1;Sound->Duration=INDEFINITELY_LOOPING_DURATION;Feedback->SetSound(Sound);
}
void AIslandEstateAmenity::BeginPlay()
{
 Super::BeginPlay();MovingPartClosed=MovingPart->GetRelativeTransform();StreamScaleBefore=WaterStream->GetRelativeScale3D();HeldObject->SetVisibility(false);WaterSurface->SetVisibility(false);WaterStream->SetVisibility(false);Details->SetVisibility(false);TaskBody->SetVisibility(false);if(Kind==EIslandEstateAmenityKind::Drink){WaterSurface->AttachToComponent(HeldObject,FAttachmentTransformRules::KeepRelativeTransform);if(HeldObject->GetMaterial(0))HeldObject->CreateDynamicMaterialInstance(0);}PrepareAudio();SetActorTickEnabled(false);
}
void AIslandEstateAmenity::EndPlay(EEndPlayReason::Type Reason){Cleanup();Super::EndPlay(Reason);}
bool AIslandEstateAmenity::IsFeedbackPlaying()const{return Feedback && Feedback->IsPlaying();}
void AIslandEstateAmenity::Notice(const FText& Text)const
{if(const auto* P=ActivePlayer.Get())if(auto* PC=Cast<APlayerController>(P->GetController()))if(auto* N=PC->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(Text,5);}

AIslandEstateCooktop::AIslandEstateCooktop()
{StationID=TEXT("Estate701.Kitchen.Cooktop");DisplayName=FText::FromString(TEXT("Kitchen fresh-catch cooktop"));bIndoorCooktop=true;Pan=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ActualCookingPan"));Pan->SetupAttachment(RootComponent);Pan->SetCollisionEnabled(ECollisionEnabled::NoCollision);Pan->SetCanEverAffectNavigation(false);Pan->SetRelativeLocation(FVector(0,0,90));}
void AIslandEstateCooktop::BeginPlay()
{
 Super::BeginPlay();
 // Existing manufactured serving/grill hardware at the fitted counter,
 // never a second cooking inventory or timer-created food.
 Visual->SetStaticMesh(CooktopMesh);Visual->SetRelativeLocation(FVector(-14,0,88));Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Visual->SetVisibility(CooktopMesh!=nullptr); // Authored original induction surface remains the actual visible fixture.
 Lid->SetVisibility(false);Coals->SetVisibility(false);
 TInlineComponentArray<UPointLightComponent*> Lights(this);for(auto* L:Lights)if(L->GetFName()==TEXT("EmberGlow"))
 {L->SetIntensity(0);if(auto* S=GetWorld()->GetSubsystem<UWorldLightingSubsystem>()){FNeonCityLocalLightPolicy Policy;Policy.Intensity=0;Policy.bCastShadows=false;S->RegisterLight(L,Policy);}}
}
