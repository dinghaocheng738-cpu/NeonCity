#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandHomeWardrobe.h"
#include "Adventure/IslandOwnedAppearanceState.h"
#include "Adventure/IslandGarageControl.h"
#include "Adventure/IslandGarageDoor.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Property/PropertySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace WardrobeGarageNative
{
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,int Mode):Test(T),Kind(Mode),Evidence(Mode==0?TEXT("WardrobeSave"):Mode==1?TEXT("WardrobeCold"):TEXT("GarageControls")){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>100)return End(TEXT("Wardrobe/garage native sequence timeout"));if(!P || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(Kind>=3?NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254)):NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31830))))return false;
   for(TActorIterator<AIslandHomeWardrobe> I(W);I;++I)Wardrobe=*I;
   for(TActorIterator<AIslandGarageControl> I(W);I;++I){if(I->Action==EIslandGarageAction::DisplayLighting)Lights=*I;if(I->Action==EIslandGarageAction::Door)Gate=*I;}
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(Kind<2)
   {
    FString SlotSuffix;FParse::Value(FCommandLine::Get(),TEXT("NeonCityWardrobeRun="),SlotSuffix);if(SlotSuffix.IsEmpty())SlotSuffix=TEXT("01");
    if(!Wardrobe || !Save || !Save->ConfigureSlot(FString(TEXT("NeonCity_Test_Wardrobe"))+FPaths::MakeValidFileName(SlotSuffix),false))return End(TEXT("Actual authored owned wardrobe/isolated save missing"));
    if(Kind==1){if(!Save->DoesSaveExist())return End(TEXT("Genuine wardrobe A checkpoint required"));Key(EKeys::F7,true);return Next(20);}
    if(Save->DoesSaveExist())return End(TEXT("Preserve prior genuine wardrobe checkpoint; select a fresh source slot before rerun"));
    Target=Wardrobe;return Place(Wardrobe->GetActorLocation()+FVector(155,0,45),1);
   }
   if(!Lights || !Gate || !Gate->Door || Lights->DisplayLights.Num()!=2)return End(TEXT("Actual display-light/door controls absent"));
   if(Kind==4){Target=Gate;return Place(Gate->GetActorLocation()+Gate->GetActorForwardVector()*125,36);}
   Target=Lights;return Place(Lights->GetActorLocation()+Lights->GetActorForwardVector()*125,31);
  }
  if(Stage==20 && Age>.8)
  {
   Key(EKeys::F7,false);auto* Model=Save->ReadSaveForInspection();State=P->FindComponentByClass<UIslandOwnedAppearanceState>();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || !State || State->GetSelectedPalette()!=1 || State->GetOwnedPaletteMask()!=0x1f || Model->Player.OwnedAppearancePalette!=1 || P->IsHidden() || !P->GetActorEnableCollision() || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Independent genuine F7 lost owned outfit/safe on-foot state"));
   if(State->IsComponentTickEnabled())return End(TEXT("Restored clothing is still retrying instead of completing its material application"));
   if(FVector::Distance(P->GetActorLocation(),Model->Player.Transform.GetLocation())>10)return End(TEXT("Independent restore moved the saved wardrobe approach"));Target=Wardrobe;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wardrobe-Cold-Saved-Outfit.png"));return Next(1);
  }
  if(Stage==1 && Age>.3)
  {
   State=UIslandOwnedAppearanceState::FindOrCreate(P);BeforePalette=State->GetSelectedPalette();Skin=P->GetMesh()->GetSkeletalMeshAsset();CaptureGear();
   if(!Focus(Wardrobe))return End(TEXT("Normal wardrobe F eye trace blocked"));Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.25)
  {
   Key(EKeys::F,false);if(!State || State->GetSelectedPalette()!=(BeforePalette+1)%5 || State->GetOwnedPaletteMask()!=0x1f || P->GetMesh()->GetSkeletalMeshAsset()!=Skin || !GearUnchanged())return End(TEXT("Actual F outfit change lost ownership, skin or gear"));
   FLinearColor Tint;const auto* Mat=P->GetMesh()->GetMaterial(0);if(!Mat || !Mat->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Owned Textile Tint")),Tint))return End(TEXT("Visible shirt has no actual clothing tint"));
   if(!Tint.Equals(Kind==0?FLinearColor(.42,.23,.15):FLinearColor(.32,.36,.23),.005))return End(TEXT("Actual rendered clothing tint did not change to selected owned palette"));
   FNeonCityVisualEvidence::CaptureHUD(W,Kind==0?TEXT("Wardrobe-Native-Terracotta.png"):TEXT("Wardrobe-Cold-Replayed-Olive.png"));
   if(Kind==1)return End(TEXT(""));return Next(3);
  }
  if(Stage==3 && Age>.3){Key(EKeys::F6,true);return Next(4);}
  if(Stage==4 && Age>.8)
  {
   Key(EKeys::F6,false);auto* M=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || M->Player.OwnedAppearancePalette!=1 || M->Player.OwnedAppearanceMask!=0x1f || M->Player.R05Equipped!=DiveWorn || M->Player.WingsuitEquipped!=WingWorn || M->Player.OwnsR05!=OwnSuit || M->Player.OwnsDPV!=OwnDPV)return End(TEXT("Actual F6 lost selected/owned clothing or equipment"));return End(TEXT(""));
  }
  if(Stage==31 && Age>.3){if(!Focus(Lights))return End(TEXT("Actual display lighting F control not focusable"));Key(EKeys::F,true);return Next(32);}
  if(Stage==32 && Age>.3){Key(EKeys::F,false);for(const auto& L:Lights->DisplayLights)if(!L || L->IsVisible())return End(TEXT("Actual F did not turn off all display spots"));return Next(33);}
  if(Stage==33 && Age>.3){if(!Focus(Lights))return End(TEXT("Lighting replay focus missing"));Key(EKeys::F,true);return Next(34);}
  if(Stage==34 && Age>.3)
  {Key(EKeys::F,false);for(const auto& L:Lights->DisplayLights)if(!L || !L->IsVisible() || L->Intensity<=0)return End(TEXT("Actual F failed to restore real display illumination"));Target=Gate;return Next(35);}
  if(Stage==35)
  {
   // Real short on-foot walk between adjacent wall controls, rather than a
   // second pose fixture. The internal driver sends only mapped W input.
   const FVector Approach=Gate->GetActorLocation()+Gate->GetActorForwardVector()*125;
   if(FVector::Dist2D(P->GetActorLocation(),Approach)>18){PC->SetControlRotation((Approach-P->GetActorLocation()).Rotation());Key(EKeys::W,true);return false;}
   Key(EKeys::W,false);return Next(36);
  }
  if(Stage==36 && Age>.3){if(!Focus(Gate))return End(TEXT("Actual garage door F control not focusable"));Key(EKeys::F,true);return Next(37);}
  if(Stage==37 && Age>.25){Key(EKeys::F,false);return Next(38);}
  if(Stage==38 && Age>Gate->Door->TravelSeconds+.5f)
  {
   auto* D=Gate->Door.Get();if(D->GetOpenFraction()>.001 || D->IsTargetOpen())
   {
    FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOneGarageReadOnlyObstruction),false,D);
    TSet<AActor*> Reported;
    for(int32 Step=0;Step<=200;++Step)for(int32 I=0;I<D->Panels.Num();++I)
    {
     FVector Local;FRotator Rotation;D->PanelPose(I,Step/200.f,Local,Rotation);const FTransform T=FTransform(Rotation,Local)*D->GetActorTransform();
     FHitResult H;const auto Shape=FCollisionShape::MakeBox(FVector(D->Width*.5f-.2f,2.8f,D->Height/20.f-.2f));
     if(W->SweepSingleByChannel(H,T.GetLocation(),T.GetLocation()+FVector(0,0,.01),T.GetRotation(),ECC_Pawn,Shape,Q) && !Reported.Contains(H.GetActor()))
     {
      Reported.Add(H.GetActor());const auto* Static=Cast<UStaticMeshComponent>(H.GetComponent());
      UE_LOG(LogTemp,Warning,TEXT("TheOneGarage510 actual blocked fraction%.3f leaf%d actor%s mesh%s component%s impact%s leaf%s"),Step/200.f,I,*GetPathNameSafe(H.GetActor()),*GetPathNameSafe(Static?Static->GetStaticMesh():nullptr),*GetNameSafe(H.GetComponent()),*H.ImpactPoint.ToString(),*T.GetLocation().ToString());
     }
    }
    return End(TEXT("Physical garage leaves did not close; inspect recorded obstruction"));
   }
   for(const auto& Leaf:D->Panels)if(!Leaf->GetStaticMesh() || !Leaf->IsVisible() || Leaf->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics)return End(TEXT("Garage door used missing/hidden/nonphysical leaf"));
   FHitResult H;const auto T=D->GetActorTransform();FCollisionQueryParams Q(SCENE_QUERY_STAT(ActualGarageClosedBarrier),false,P);
   if(!W->LineTraceSingleByChannel(H,T.TransformPosition(FVector(0,-80,100)),T.TransformPosition(FVector(0,80,100)),ECC_Pawn,Q) || H.GetActor()!=D)return End(TEXT("Closed garage only looks solid but has no player safety collision"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Garage-Native-Closed-Real-Collision.png"));return Next(39);
  }
  if(Stage==39 && Age>.3){if(!Focus(Gate))return End(TEXT("Actual opening F focus missing"));Key(EKeys::F,true);return Next(40);}
  if(Stage==40 && Age>.25){Key(EKeys::F,false);return Next(41);}
  if(Stage==41 && Age>Gate->Door->TravelSeconds+.5f)
  {
   if(Gate->Door->GetOpenFraction()<.999 || !Gate->Door->IsTargetOpen())return End(TEXT("Actual articulated garage door did not return fully open"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Garage-Native-Full-Open.png"));return End(TEXT(""));
  }
  return false;
 }
 bool Place(const FVector& AtWorld,int NextStage)
 {
  FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(WardrobeGarageStart),false,P);if(!W->LineTraceSingleByChannel(H,AtWorld+FVector(0,0,130),AtWorld-FVector(0,0,250),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)return End(TEXT("Fixture start has no actual walking support"));
  const FVector Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Fixture start has blocked player circulation"));P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();return Next(NextStage);
 }
 bool Focus(AActor* Actor){auto* Box=Actor->FindComponentByClass<UBoxComponent>();PC->SetControlRotation((Box->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());return P->GetInteractionComponent()->RefreshFocusedInteractable()==Actor;}
 void CaptureGear(){const auto* D=P->FindComponentByClass<UDivingComponent>();const auto* G=P->FindComponentByClass<UWingsuitComponent>();OwnSuit=D->OwnsSuit();OwnDPV=D->OwnsDPV();DiveWorn=D->IsEquipped();WingWorn=G->IsEquipped();}
 bool GearUnchanged()const{const auto* D=P->FindComponentByClass<UDivingComponent>();const auto* G=P->FindComponentByClass<UWingsuitComponent>();return D->OwnsSuit()==OwnSuit && D->OwnsDPV()==OwnDPV && D->IsEquipped()==DiveWorn && G->IsEquipped()==WingWorn;}
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& B=Keys.FindOrAdd(K);if(B!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));B=Down;}}
 bool End(const FString& Error)
 {
  if(PC)for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("selected_palette"),State?State->GetSelectedPalette():-1);
  J->SetStringField(TEXT("scope"),Kind==0?TEXT("One supported wardrobe player-start fixture only. Actual mapped F changes an owned existing rendered clothing palette without changing skin or gear; actual F6 stores selected/owned clothing and equipment in the existing player save domain. No appearance, ownership, money, completion or equipment injection."):Kind==1?TEXT("Independent genuine F7 of Wardrobe A, exact visible standing position and owned palette restore, normal mapped F chooses the next owned palette. Zero pose/state/ownership/reward fixtures. Equipped-gear rejection is a separate pending negative validation."):TEXT("One supported player-start fixture beside real garage controls only. Actual F turns real display spots off/on; mapped W walks to adjacent control; F continuously closes ten physical visible leaves, real player collision ray blocks, F opens again. Original garage slot anchors retained. Retired-owned retrieval and occupancy reversal are separate pending validations."));
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/(Kind==0?TEXT("WardrobeSave"):Kind==1?TEXT("WardrobeCold"):TEXT("GarageControls"));IFileManager::Get().MakeDirectory(*Folder,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Folder/TEXT("receipt.json")));return true;
 }
 FAutomationTestBase* Test;int Kind,Stage=0,BeforePalette=0;double Start=0;float At=0;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandHomeWardrobe* Wardrobe=nullptr;AIslandGarageControl* Lights=nullptr;AIslandGarageControl* Gate=nullptr;AActor* Target=nullptr;UIslandOwnedAppearanceState* State=nullptr;UNeonCitySaveSubsystem* Save=nullptr;USkeletalMesh* Skin=nullptr;bool OwnSuit=false,OwnDPV=false,DiveWorn=false,WingWorn=false;TMap<FKey,bool> Keys;FNeonCityVisualEvidence Evidence;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeNativeSave,"NeonCity.Phase205c.Wardrobe.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeNativeSave::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(WardrobeGarageNative::FFlow(this,0));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeNativeCold,"NeonCity.Phase205c.Wardrobe.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeNativeCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(WardrobeGarageNative::FFlow(this,1));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarageNativeControls,"NeonCity.Phase205c.Garage.ControlsPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGarageNativeControls::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(WardrobeGarageNative::FFlow(this,2));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneGarageControls510,"NeonCity.Phase205cUpgrade.TheOneGarageControlsPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneGarageControls510::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(WardrobeGarageNative::FFlow(this,3));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneGarageDoorOnly,"NeonCity.Phase205cUpgrade.TheOneGarageDoorOnlyPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneGarageDoorOnly::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(WardrobeGarageNative::FFlow(this,4));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
