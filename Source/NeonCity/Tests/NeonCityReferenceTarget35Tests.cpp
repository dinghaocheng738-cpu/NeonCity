#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerController.h"
#include "World/NeonCityDayNightCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/OverlapResult.h"
#include "ShaderCompiler.h"
#include "SceneView.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "AI/PedestrianSpawnPoint.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/Texture2D.h"
#include "World/NeonCityMainlandSite.h"
#include "Water/CoastalOceanSurface.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "AI/NPCStateComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tests/NeonCityMarinaWaterAttribution56.h"
#include "Tests/NeonCityMarinaSeabedAB65.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "World/Foliage/NeonCityFoliageInteractionComponent.h"

namespace MarinaReference35
{
class FCapture : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ANeonCityPlayerController> PC;
 TWeakObjectPtr<ANeonCityDayNightCycle> Clock;TWeakObjectPtr<ACharacter> Player;TWeakObjectPtr<ACameraActor> Camera;
 TWeakObjectPtr<AActor> OldView;FTransform OldPlayer;float OldHour=12;bool OldCycle=true,Started=false,Cleaned=false;
 double Began=FPlatformTime::Seconds(),AtWall=0;float AtGame=0;int32 Frames=0,Shot=0,TotalShots=1,ColdMoorTextures=0,ColdBroadGroups=0,ColdBroadTrees=0;bool Series=false;FString Directory,ActualWaterParent;FTransform CameraOriginal;
 // -NeonCityReferenceHour=<h> moves the reference/asset-view shot (default 16.5).
 static float ReferenceHour(){float H=16.5f;FParse::Value(FCommandLine::Get(),TEXT("NeonCityReferenceHour="),H);return FMath::IsFinite(H)&&H>=0&&H<24?H:16.5f;}
 const float Hours[4]={ReferenceHour(),12.f,17.8f,22.f};
 struct FAssetView {FString Name,IsolateTag,NPCIdentity,ExpectedNPCMesh,Lighting=TEXT("target16.5"),Detail,PavingAB;FVector Eye,Aim;int32 IsolateCount=1;double SettleSeconds=20,MotionSeconds=0,FixedExposure=0;bool GroundedEye=false;};
 TArray<FAssetView> AssetViews;
 struct FPavingOverride {TWeakObjectPtr<UStaticMeshComponent> Component;int32 Slot;TObjectPtr<UMaterialInterface> Material;};
 TArray<FPavingOverride> PavingOverrides;
 FPostProcessSettings OriginalCameraPP;float OriginalPPWeight=0;bool PavingDiagnosticActive=false;int32 PavingRestorations=0;bool PavingRestorationVerified=true;
 void RestorePavingDiagnostic()
 {
  TSet<UStaticMeshComponent*> Components;
  for(const auto& S:PavingOverrides)if(S.Component.IsValid()){S.Component->SetMaterial(S.Slot,S.Material);PavingRestorationVerified&=S.Component->GetMaterial(S.Slot)==S.Material;Components.Add(S.Component.Get());}
  // The bounded diagnostic admits only original empty override arrays.
  // Restoring equivalent materials alone would leave runtime overrides behind.
  for(auto* C:Components){C->EmptyOverrideMaterials();PavingRestorationVerified&=C->GetNumOverrideMaterials()==0;}
  PavingOverrides.Reset();
  if(PavingDiagnosticActive && Camera.IsValid())
  {
   auto* C=Camera->GetCameraComponent();C->PostProcessSettings=OriginalCameraPP;C->PostProcessBlendWeight=OriginalPPWeight;
   PavingRestorationVerified&=C->PostProcessBlendWeight==OriginalPPWeight && C->PostProcessSettings.bOverride_AutoExposureMinBrightness==OriginalCameraPP.bOverride_AutoExposureMinBrightness && C->PostProcessSettings.bOverride_AutoExposureMaxBrightness==OriginalCameraPP.bOverride_AutoExposureMaxBrightness && C->PostProcessSettings.AutoExposureMinBrightness==OriginalCameraPP.AutoExposureMinBrightness && C->PostProcessSettings.AutoExposureMaxBrightness==OriginalCameraPP.AutoExposureMaxBrightness;
   ++PavingRestorations;
  }
  PavingDiagnosticActive=false;
 }
 bool ApplyPavingDiagnostic(UWorld* W,const FAssetView& V)
 {
  if(V.PavingAB.IsEmpty())return true;
  auto* C=Camera->GetCameraComponent();OriginalCameraPP=C->PostProcessSettings;OriginalPPWeight=C->PostProcessBlendWeight;PavingDiagnosticActive=true;
  // Lock the existing shot's measured exposure, not a new artistic exposure.
  // UE converts fixed white-point luminance to average by *.18; its
  // exposure's .18 numerator cancels, giving 2^bias/(Lmax*2^EV100).
  auto* Lens=IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.LensAttenuation"));
  const double Lmax=.78/FMath::Max(.01, Lens?double(Lens->GetFloat()):.78);
  const float EV=FMath::Log2(FMath::Pow(2.,.25)/(Lmax*V.FixedExposure));
  C->PostProcessSettings.bOverride_AutoExposureMinBrightness=true;C->PostProcessSettings.AutoExposureMinBrightness=EV;
  C->PostProcessSettings.bOverride_AutoExposureMaxBrightness=true;C->PostProcessSettings.AutoExposureMaxBrightness=EV;
  C->PostProcessBlendWeight=1;
  if(V.PavingAB==TEXT("baseline") || V.PavingAB==TEXT("restore"))return true;
  int32 Count=0;
  for(TActorIterator<AActor> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.MainlandC923.P1Port197")))
  {
   TInlineComponentArray<UStaticMeshComponent*> Components(*I);
   for(auto* Mesh:Components)for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
   {
    auto* Old=Mesh->GetMaterial(Slot);if(!Old || !Old->GetName().StartsWith(TEXT("M_Port274_Pave236_")))continue;
    const FString Name=FString(TEXT("M_Port278_AB_"))+Old->GetName().Right(1);
    auto* Parent=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/"))+Name+TEXT(".")+Name));if(!Parent)return false;
    auto* MID=UMaterialInstanceDynamic::Create(Parent,Mesh);if(!MID)return false;
    MID->SetScalarParameterValue(TEXT("ABColor"),V.PavingAB==TEXT("color")?1:0);
    MID->SetScalarParameterValue(TEXT("ABRoughness"),V.PavingAB==TEXT("roughness")?1:0);
    MID->SetScalarParameterValue(TEXT("ABNormal"),V.PavingAB==TEXT("normal")?1:0);
    if(!PavingOverrides.ContainsByPredicate([Mesh](const FPavingOverride& S){return S.Component.Get()==Mesh;}) && Mesh->GetNumOverrideMaterials()!=0)return false;
    PavingOverrides.Add({Mesh,Slot,Old});Mesh->SetMaterial(Slot,MID);++Count;
   }
  }
  return Count==5;
 }
 struct FInspectionLightState
 {
  TWeakObjectPtr<UDirectionalLightComponent> Sun;TWeakObjectPtr<USkyLightComponent> Sky;
  FTransform Transform;FLinearColor Color;float Intensity=0,Temperature=0,SourceAngle=0,SkyIntensity=0;bool UseTemperature=false,Active=false;
 } Inspection;
 void RestoreInspectionLighting()
 {
  if(!Inspection.Active)return;
  if(Inspection.Sun.IsValid())
  {
   Inspection.Sun->SetWorldTransform(Inspection.Transform);Inspection.Sun->SetIntensity(Inspection.Intensity);
   Inspection.Sun->SetLightColor(Inspection.Color);Inspection.Sun->SetUseTemperature(Inspection.UseTemperature);
   Inspection.Sun->SetTemperature(Inspection.Temperature);Inspection.Sun->SetLightSourceAngle(Inspection.SourceAngle);
  }
  if(Inspection.Sky.IsValid())Inspection.Sky->SetIntensity(Inspection.SkyIntensity);
  Inspection.Active=false;
 }
 bool ApplyInspectionLighting()
 {
  auto* Sun=Clock->GetSun();if(!Sun)return false;
  TInlineComponentArray<USkyLightComponent*> Skies(Clock.Get());if(Skies.Num()!=1)return false;
  Inspection.Sun=Sun;Inspection.Sky=Skies[0];Inspection.Transform=Sun->GetComponentTransform();Inspection.Color=Sun->GetLightColor();
  Inspection.Intensity=Sun->Intensity;Inspection.Temperature=Sun->Temperature;Inspection.UseTemperature=Sun->bUseTemperature;
  Inspection.SourceAngle=Sun->LightSourceAngle;Inspection.SkyIntensity=Skies[0]->Intensity;Inspection.Active=true;
  // Explicit P1-only fixed neutral key. Never saved and never used for the
  // reference or performance image; original clock light state is restored.
  Sun->SetWorldRotation(FRotator(-45,265,0));Sun->SetLightColor(FLinearColor::White);Sun->SetUseTemperature(true);
  Sun->SetTemperature(6500);Sun->SetIntensity(30000);Sun->SetLightSourceAngle(4);Skies[0]->SetIntensity(.85f);
  return true;
 }
 int32 AssetMotionFrame=0;float AssetMotionLast=-1;
 struct FVisibility {TWeakObjectPtr<UMeshComponent> Component;bool Visible=false;};
 TArray<FVisibility> AssetVisibility;
 void RestoreAssetVisibility()
 {
  for(const auto& V:AssetVisibility)if(V.Component.IsValid())V.Component->SetVisibility(V.Visible,false);
  AssetVisibility.Reset();
 }
 bool ApplyAssetView(UWorld* W,const FAssetView& V)
 {
  RestorePavingDiagnostic();
  if(!PavingRestorationVerified)return false;
  RestoreInspectionLighting();
  RestoreAssetVisibility();
  if(!V.IsolateTag.IsEmpty())
  {
   int32 Samples=0;
   for(TActorIterator<AActor> I(W);I;++I)
   {
    if(I->ActorHasTag(FName(*V.IsolateTag))){++Samples;continue;}
    TInlineComponentArray<UMeshComponent*> Components(*I);
    for(auto* C:Components)if(C && C->IsVisible()){AssetVisibility.Add({C,true});C->SetVisibility(false,false);}
   }
   if(Samples!=V.IsolateCount){RestoreAssetVisibility();return false;}
  }
  FVector ActualEye=V.Eye;
  if(V.GroundedEye)
  {
   FHitResult Floor;FCollisionQueryParams Query(SCENE_QUERY_STAT(C923P1GroundEye),true,Player.Get());
   if(W->LineTraceSingleByObjectType(Floor,V.Eye,V.Eye-FVector(0,0,3000),FCollisionObjectQueryParams(ECC_WorldStatic),Query) && Floor.ImpactNormal.Z>.707f)
    ActualEye.Z=Floor.ImpactPoint.Z+160.f;
  }
  Camera->SetActorLocationAndRotation(ActualEye,(V.Aim-ActualEye).Rotation());Clock->SetTimeOfDay(Hours[0]);
  return (V.Lighting!=TEXT("inspection_neutral235") || ApplyInspectionLighting()) && ApplyPavingDiagnostic(W,V);
 }
 TMap<TWeakObjectPtr<UUserWidget>,ESlateVisibility> Widgets;
 TMap<TWeakObjectPtr<UUserWidget>,float> Opacities;
 TMap<FString,FVector> StrollFirst,StrollPrevious;TMap<FString,double> StrollPath;TSet<FString> NativeStrollInputs;
 bool CouplePhase351=false;FString CouplePhase351Manifest;TSharedPtr<FJsonObject> CouplePhase351Policy,CouplePhase351Last;
 double CouplePhase351Start=0,CouplePhase351LastLog=0;int32 CouplePhase351Consecutive=0;
 TArray<TSharedPtr<FJsonValue>> CouplePhase351History;
 bool LoadCouplePhase351Policy()
 {
  CouplePhase351=FParse::Param(FCommandLine::Get(),TEXT("NeonCityCouplePhase351"));if(!CouplePhase351)return true;
  FString Text;
  if(!FParse::Value(FCommandLine::Get(),TEXT("NeonCityCouplePhase351Manifest="),CouplePhase351Manifest)
   || !FFileHelper::LoadFileToString(Text,*CouplePhase351Manifest)
   || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),CouplePhase351Policy) || !CouplePhase351Policy)return false;
  FString Schema;if(!CouplePhase351Policy->TryGetStringField(TEXT("schema"),Schema) || Schema!=TEXT("NeonCity.ReferenceCouplePhase351.v1"))return false;
  struct FBound351{const TCHAR* Key;double Low,High;};
  const FBound351 Bounds[]={
   {TEXT("max_wait_seconds"),1,90},{TEXT("minimum_speed_cm_s"),1,220},
   {TEXT("minimum_away_dot"),.5,1},{TEXT("minimum_facing_velocity_dot"),0,1},
   {TEXT("maximum_ground_gap_cm"),1,10},{TEXT("minimum_consecutive_frames"),1,30},
   {TEXT("minimum_pair_foot_spacing_pixels"),1,1774},{TEXT("maximum_pair_foot_spacing_pixels"),1,1774},
   {TEXT("maximum_pair_foot_height_difference_pixels"),1,100}};
  for(const auto& B:Bounds){double V=0;if(!CouplePhase351Policy->TryGetNumberField(B.Key,V) || !FMath::IsFinite(V) || V<B.Low || V>B.High)return false;}
  if(CouplePhase351Policy->GetNumberField(TEXT("minimum_pair_foot_spacing_pixels"))>CouplePhase351Policy->GetNumberField(TEXT("maximum_pair_foot_spacing_pixels")))return false;
  for(const TCHAR* Key:{TEXT("male_foot_window_xyxy"),TEXT("female_foot_window_xyxy")})
  {
   const TArray<TSharedPtr<FJsonValue>>* Box=nullptr;if(!CouplePhase351Policy->TryGetArrayField(Key,Box) || !Box || Box->Num()!=4)return false;
   double V[4];for(int32 I=0;I<4;++I)if(!(*Box)[I].IsValid() || !(*Box)[I]->TryGetNumber(V[I]) || !FMath::IsFinite(V[I]))return false;
   if(V[0]<0 || V[1]<0 || V[2]>1774 || V[3]>887 || V[0]>=V[2] || V[1]>=V[3])return false;
  }
  return true;
 }
 bool ObserveCouplePhase351(UWorld* W,const FSceneView* View,const FVector& Eye)
 {
  const double Now=FPlatformTime::Seconds();if(CouplePhase351Start==0)CouplePhase351Start=Now;
  const auto Number=[this](const TCHAR* Key){return CouplePhase351Policy->GetNumberField(Key);};
  auto Sample=MakeShared<FJsonObject>();Sample->SetNumberField(TEXT("wall_seconds_waiting"),Now-CouplePhase351Start);Sample->SetNumberField(TEXT("game_seconds"),W->GetTimeSeconds());
  TArray<TSharedPtr<FJsonValue>> People;FVector2D Feet[2]={FVector2D::ZeroVector,FVector2D::ZeroVector};bool Passed=true;
  for(int32 Index=0;Index<2;++Index)
  {
   const FString Id=FString::Printf(TEXT("SouthBeach.Pedestrian.%02d"),Index+1);ANeonCityNPCCharacter* N=nullptr;int32 Count=0;
   for(TActorIterator<APedestrianSpawnPoint> I(W);I;++I)if(I->PedestrianId.ToString()==Id){N=I->GetSpawnedPedestrian();++Count;}
   auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("id"),Id);
   const bool Admitted=Count==1 && N && !N->IsPopulationDormant() && !N->IsHidden() && N->GetMesh()->GetAnimInstance();R->SetBoolField(TEXT("normal_admitted_animated_actor"),Admitted);
   bool PersonPassed=false;
   if(Admitted)
   {
    const FVector P=N->GetActorLocation(),Velocity=N->GetVelocity(),Direction=Velocity.GetSafeNormal2D();const float Half=N->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const double Away=FVector::DotProduct(Direction,(P-Eye).GetSafeNormal2D()),Facing=FVector::DotProduct(N->GetActorForwardVector().GetSafeNormal2D(),Direction);
    FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(C923LiveCouplePhase351Ground),false,N);
    const bool HasFloor=W->LineTraceSingleByChannel(Floor,P+FVector(0,0,20),P-FVector(0,0,Half+25),ECC_Pawn,Q);
    const double Gap=HasFloor?P.Z-Half-Floor.ImpactPoint.Z:TNumericLimits<double>::Max();
    const bool Grounded=HasFloor && Floor.ImpactNormal.Z>=.94 && FMath::Abs(Gap)<=Number(TEXT("maximum_ground_gap_cm")) && N->GetCharacterMovement()->IsMovingOnGround();
    const FVector Foot=HasFloor?Floor.ImpactPoint:P-FVector(0,0,Half);
    const bool Projected=View->WorldToPixel(Foot,Feet[Index]);
    const auto& Box=CouplePhase351Policy->GetArrayField(Index==0?TEXT("male_foot_window_xyxy"):TEXT("female_foot_window_xyxy"));
    const bool InWindow=Projected && Feet[Index].X>=Box[0]->AsNumber() && Feet[Index].Y>=Box[1]->AsNumber() && Feet[Index].X<=Box[2]->AsNumber() && Feet[Index].Y<=Box[3]->AsNumber();
    PersonPassed=Grounded && InWindow && Velocity.Size2D()>=Number(TEXT("minimum_speed_cm_s")) && Away>=Number(TEXT("minimum_away_dot")) && Facing>=Number(TEXT("minimum_facing_velocity_dot"));
    R->SetStringField(TEXT("position_cm"),P.ToString());R->SetStringField(TEXT("velocity_cm_s"),Velocity.ToString());R->SetStringField(TEXT("actor_rotation"),N->GetActorRotation().ToString());R->SetStringField(TEXT("projected_floor_foot_pixels"),Feet[Index].ToString());
    R->SetNumberField(TEXT("speed_cm_s"),Velocity.Size2D());R->SetNumberField(TEXT("away_from_camera_dot"),Away);R->SetNumberField(TEXT("facing_velocity_dot"),Facing);
    R->SetBoolField(TEXT("normal_grounded"),Grounded);R->SetBoolField(TEXT("in_target_foot_window"),InWindow);R->SetBoolField(TEXT("actual_anim_root_motion"),N->IsPlayingRootMotion());
    if(HasFloor){R->SetStringField(TEXT("floor_cm"),Floor.ImpactPoint.ToString());R->SetStringField(TEXT("floor_actor"),GetPathNameSafe(Floor.GetActor()));R->SetNumberField(TEXT("capsule_floor_gap_cm"),Gap);}
   }
   R->SetBoolField(TEXT("eligible"),PersonPassed);Passed&=PersonPassed;People.Add(MakeShared<FJsonValueObject>(R));
  }
  const double PairSpacing=Feet[1].X-Feet[0].X,PairHeight=FMath::Abs(Feet[1].Y-Feet[0].Y);
  Passed&=PairSpacing>=Number(TEXT("minimum_pair_foot_spacing_pixels")) && PairSpacing<=Number(TEXT("maximum_pair_foot_spacing_pixels")) && PairHeight<=Number(TEXT("maximum_pair_foot_height_difference_pixels"));
  CouplePhase351Consecutive=Passed?CouplePhase351Consecutive+1:0;
  Sample->SetArrayField(TEXT("people"),People);Sample->SetNumberField(TEXT("pair_foot_spacing_pixels"),PairSpacing);Sample->SetNumberField(TEXT("pair_foot_height_difference_pixels"),PairHeight);
  Sample->SetNumberField(TEXT("consecutive_eligible_frames"),CouplePhase351Consecutive);const bool Ready=CouplePhase351Consecutive>=Number(TEXT("minimum_consecutive_frames"));Sample->SetBoolField(TEXT("ready"),Ready);CouplePhase351Last=Sample;
  if(CouplePhase351History.IsEmpty() || Now-CouplePhase351LastLog>=1 || Ready){CouplePhase351History.Add(MakeShared<FJsonValueObject>(Sample));CouplePhase351LastLog=Now;}
  return Ready;
 }
 TSharedRef<FJsonObject> CouplePhase351Receipt(bool Ready)const
 {
  auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("enabled"),CouplePhase351);R->SetBoolField(TEXT("eligible_capture_pose_observed"),Ready);
  R->SetStringField(TEXT("manifest"),CouplePhase351Manifest);R->SetObjectField(TEXT("policy"),CouplePhase351Policy);R->SetArrayField(TEXT("bounded_live_observations"),CouplePhase351History);
  if(CouplePhase351Last)R->SetObjectField(TEXT("actual_capture_or_last_pose"),CouplePhase351Last);
  R->SetBoolField(TEXT("npc_state_transform_input_or_animation_changed_by_gate"),false);
  R->SetStringField(TEXT("scope"),TEXT("Live pose eligibility only, after original warmup. No freeze, NPC teleport, force yaw, animation seek, route/input change or garment/continuous-motion visual PASS."));return R;
 }
 C923MarinaWaterAttribution::FSeabedAB65 SeabedAB62;
 TWeakObjectPtr<UDirectionalLightComponent> DiagnosticSun;float OriginalSpecular=1.f;bool WaterAB62=false;
 void Cleanup()
 {
  if(Cleaned)return;Cleaned=true;
  RestorePavingDiagnostic();
  if(PavingRestorations)
  {
   Test->TestTrue(TEXT("Paving278 original materials, empty override arrays and exposure overrides restored"),PavingRestorationVerified);
   FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"verified\":%s,\"restored_views\":%d,\"material_overrides_cleared\":true}\n"),PavingRestorationVerified?TEXT("true"):TEXT("false"),PavingRestorations),*(Directory/TEXT("Paving278-restoration.json")));
  }
  RestoreInspectionLighting();
  RestoreAssetVisibility();
  if(!SeabedAB62.Restore())Test->AddError(TEXT("Diagnostic62 must restore both seabed components"));
  if(DiagnosticSun.IsValid()){DiagnosticSun->SetSpecularScale(OriginalSpecular);Test->TestEqual(TEXT("Diagnostic62 restores original sun specular scale"),DiagnosticSun->SpecularScale,OriginalSpecular);}
  if(Started && PC.IsValid()){PC->SetPhotographyHUDHidden(false);PC->MarinaReference();}
  if(Started && Camera.IsValid())Camera->SetActorTransform(CameraOriginal);
  if(Clock.IsValid() && Started){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
  if(Player.IsValid() && Started)Player->SetActorTransform(OldPlayer,false,nullptr,ETeleportType::TeleportPhysics);
 }
 bool End(const FString& Error=FString())
 {
  Cleanup();if(!Error.IsEmpty())Test->AddError(Error);
  if(Started && PC.IsValid())
  {
   Test->TestTrue(TEXT("Reference camera returns to the original normal view"),PC->GetViewTarget()==OldView.Get());
   Test->TestFalse(TEXT("Photography HUD toggle restored"),PC->IsPhotographyHUDHidden());
   for(const auto& V:Widgets)if(V.Key.IsValid())Test->TestTrue(TEXT("Original widget visibility restored"),V.Key->GetVisibility()==V.Value);
   for(const auto& V:Opacities)if(V.Key.IsValid())Test->TestTrue(TEXT("Original widget render opacity restored"),V.Key->GetRenderOpacity()==V.Value);
  }
  return true;
 }
public:
 explicit FCapture(FAutomationTestBase* In):Test(In){}
 ~FCapture(){Cleanup();}
 bool Update()override
 {
  const double CaptureDeadline=AssetViews.IsEmpty()?360.:90.+TotalShots*45.;
  if(FPlatformTime::Seconds()-Began>CaptureDeadline)return End(TEXT("Reference target capture timed out"));
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<8)return false;World=W;
  if(!Started)
  {
   if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityReferenceTarget")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))
    return End(TEXT("Reference capture requires its exact opt-in stills launch"));
   auto* Controller=Cast<ANeonCityPlayerController>(W->GetFirstPlayerController());
   auto* Character=Controller?Cast<ACharacter>(Controller->GetPawn()):nullptr;if(!Controller || !Character)return false;
   PC=Controller;Player=Character;
   if(Controller->IsPhotographyHUDHidden())return End(TEXT("Reference starts from the normal visible-HUD state"));
   int32 Count=0;for(TActorIterator<ACameraActor> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel && I->ActorHasTag(TEXT("NeonCity.MainlandC923.ReferenceTarget"))){Camera=*I;++Count;}
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel){Clock=*I;break;}
   if(Count!=1 || !Camera.IsValid() || !Clock.IsValid())return End(TEXT("Exactly one real authored reference camera and persistent clock are required"));
   auto* C=Camera->GetCameraComponent();if(!C || !C->bConstrainAspectRatio || !FMath::IsNearlyEqual(C->AspectRatio,2.f,.00001f) || C->PostProcessBlendWeight!=0.f)
    return End(TEXT("Native reference camera requires original2:1 framing and no private colour override"));
   FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(C923Reference35Eye),false,Character);const FVector Eye=Camera->GetActorLocation();
   const bool Reference79=Camera->ActorHasTag(TEXT("NeonCity.MainlandC923.Reference79"));
   if(Reference79 && (!Eye.Equals(FVector(-5300,-2662,275),.01f) || !Camera->GetActorRotation().Equals(FRotator(-1.5f,4.004173f,0),.0001f)))
    return End(TEXT("Reference79 must retain the calibrated elevated photograph camera; no per-shot pose changes"));
   const float MinimumEyeHeight=Reference79?219.f:150.f,MaximumEyeHeight=Reference79?221.f:170.f;
   if(!W->LineTraceSingleByChannel(H,Eye,Eye-FVector(0,0,250),ECC_Pawn,Q) || H.ImpactNormal.Z<.94f || Eye.Z-H.ImpactPoint.Z<MinimumEyeHeight || Eye.Z-H.ImpactPoint.Z>MaximumEyeHeight
      || W->OverlapBlockingTestByChannel(Eye,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(25),Q))
    return End(TEXT("Reference camera must stand at a clear real promenade eye height"));
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);if(Label.IsEmpty())return End(TEXT("Unique evidence label required"));
   Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("ReferenceTarget35");
   if(IFileManager::Get().DirectoryExists(*Directory))return End(TEXT("Preserve previous reference evidence"));IFileManager::Get().MakeDirectory(*Directory,true);
   if(!LoadCouplePhase351Policy())return End(TEXT("Optional live couple phase351 requires its complete valid explicit policy manifest"));
   OldView=Controller->GetViewTarget();OldPlayer=Character->GetActorTransform();OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;CameraOriginal=Camera->GetActorTransform();
   FVector Observer(-2200,-2250,200);if(!W->LineTraceSingleByChannel(H,Observer+FVector(0,0,100),Observer-FVector(0,0,400),ECC_Pawn,Q))return End(TEXT("Existing cafe observer lost real floor"));
   Observer.Z=H.ImpactPoint.Z+Character->GetSimpleCollisionHalfHeight()+2;
   if(W->OverlapBlockingTestByChannel(Observer,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Character->GetSimpleCollisionRadius(),Character->GetSimpleCollisionHalfHeight()-1),Q))return End(TEXT("Existing cafe observer is blocked"));
   TArray<UUserWidget*> Existing;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller,Existing,UUserWidget::StaticClass(),true);
   for(auto* Widget:Existing)if(Widget && Widget->GetOwningPlayer()==Controller){Widgets.Add(Widget,Widget->GetVisibility());Opacities.Add(Widget,Widget->GetRenderOpacity());}
   if(Widgets.IsEmpty())return End(TEXT("Actual normal HUD widgets must exist before hiding and restoring them"));
   bool MooringsPresent=false;for(TActorIterator<AActor> I(W);I;++I)MooringsPresent|=I->ActorHasTag(TEXT("NeonCity.MainlandC923.Moorings38"));
   if(MooringsPresent)
   {
    const TCHAR* Names[]={TEXT("T_fabric_pattern_07_nor_dx_1k_normal"),TEXT("T_fabric_pattern_07_rough_1k_roughness")};
    for(const TCHAR* Name:Names)
    {
     const FString Path=FString::Printf(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/%s.%s"),Name,Name);
     auto* Texture=LoadObject<UTexture2D>(nullptr,*Path);
     if(!Texture || Texture->GetSizeX()!=1024 || Texture->GetSizeY()!=1024 || Texture->GetNumMips()<2 || !Texture->GetResource() || Texture->SRGB)
      return End(TEXT("New mooring texture cold runtime source/platform/resource gate failed"));
     ++ColdMoorTextures;
    }
   }
   // Newly authored instances must survive a real process/map reload with
   // their semantic buffer intact. A render of zero/default custom data can
   // look plausible while placing wind roots and LOD logic in the wrong place.
   const bool PrivateCanopy63=FParse::Param(FCommandLine::Get(),TEXT("C923Exterior63"));
   const bool ExpandedCanopy=PrivateCanopy63 || FParse::Param(FCommandLine::Get(),TEXT("C923Exterior58"));
   if(ExpandedCanopy || FParse::Param(FCommandLine::Get(),TEXT("C923Exterior51")))
   {
    const int32 ExpectedPerGroup=ExpandedCanopy?72:24;
    for(TActorIterator<AActor> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel && I->ActorHasTag(TEXT("NeonCity.MainlandC923.BroadCanopy47")))
    {
     ++ColdBroadGroups;TInlineComponentArray<UInstancedStaticMeshComponent*> Groups(*I);
     if(Groups.Num()!=1)return End(TEXT("Authored broad canopy must retain one native instance group"));
     auto* G=Groups[0];auto* Material=Cast<UMaterialInstance>(G->GetMaterial(0));
     if(G->GetInstanceCount()!=ExpectedPerGroup || G->NumCustomDataFloats!=11 || G->PerInstanceSMCustomData.Num()!=ExpectedPerGroup*11 || !Material || Material->GetName()!=(PrivateCanopy63?TEXT("MI_C923_BroadCanopy63"):TEXT("MI_C923_BroadCanopy47")) || G->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)
      return End(TEXT("Cold broad canopy lost its exact authored count,11 values,private material or collision policy"));
     if(PrivateCanopy63 && GetNameSafe(Material->Parent)!=TEXT("M_C923_BroadCanopy63"))return End(TEXT("Cold canopy must load the actual private distance-facing surface parent"));
     for(const auto& Parameter:TArray<TPair<FName,float>>{{TEXT("CanopyImpostorsEnabled"),0.f},{TEXT("UseGameplayLODView"),0.f},{TEXT("WindSpeed"),3.f},{TEXT("IndependentCacheSurface"),1.f}})
     {
      float Value=0;if(!Material->GetScalarParameterValue(FMaterialParameterInfo(Parameter.Key),Value) || Value!=Parameter.Value)
       return End(TEXT("Cold private canopy parameter override did not survive saved-map loading"));
     }
     for(int32 Index=0;Index<ExpectedPerGroup;++Index)
     {
      FTransform Pose;if(!G->GetInstanceTransform(Index,Pose,true))return End(TEXT("Cold native canopy transform absent"));
      const float* D=G->PerInstanceSMCustomData.GetData()+Index*11;
      for(int32 J=0;J<11;++J)if(!FMath::IsFinite(D[J]))return End(TEXT("Cold canopy has nonfinite semantic data"));
      const FVector Decoded(D[5]*100.,-D[7]*100.,D[6]*100.);
      if(FVector::Dist(Decoded,Pose.GetLocation())>.1 || D[1]<10.f || D[1]>16.f || D[2]<.89f || D[2]>1.11f || D[8]!=0.f || D[9]!=0.f || D[10]!=0.f)
       return End(TEXT("Cold canopy semantic roots,height,scale or zero native spring mismatch"));
      ++ColdBroadTrees;
     }
    }
    if(ColdBroadGroups!=3 || ColdBroadTrees!=3*ExpectedPerGroup)return End(TEXT("All three authored broad canopy groups and exact saved trees must be present after cold reload"));
   }
   Character->SetActorLocation(Observer,false,nullptr,ETeleportType::TeleportPhysics);Controller->SetPhotographyHUDHidden(true);Controller->MarinaReference();
   Started=true;Series=FParse::Param(FCommandLine::Get(),TEXT("NeonCityReferenceSeries37"));TotalShots=Series?(FParse::Param(FCommandLine::Get(),TEXT("NeonCitySummerClose37"))?8:4):1;
   FString SurveyFile;
   if(FParse::Value(FCommandLine::Get(),TEXT("NeonCityAssetSurvey127="),SurveyFile))
   {
    const FString ProductionRoot=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/MainlandC923/ReferenceProduction127/P1/"));
    SurveyFile=FPaths::ConvertRelativePathToFull(SurveyFile);
    FString Data;TSharedPtr<FJsonObject> Survey;const TArray<TSharedPtr<FJsonValue>>* Views=nullptr;
    if(Series || !SurveyFile.StartsWith(ProductionRoot) || !FFileHelper::LoadFileToString(Data,*SurveyFile)
       || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data),Survey) || !Survey.IsValid()
       || !Survey->TryGetArrayField(TEXT("views"),Views) || Views->Num()<1 || Views->Num()>32)
     return End(TEXT("Asset survey requires a bounded production camera manifest and no other series"));
    TSet<FString> Names;
    for(const auto& Value:*Views)
    {
     const auto Row=Value->AsObject();const TArray<TSharedPtr<FJsonValue>> *EyeValues=nullptr,*AimValues=nullptr;FAssetView V;
     if(!Row.IsValid() || !Row->TryGetStringField(TEXT("name"),V.Name) || !V.Name.StartsWith(TEXT("Asset127-")) || FPaths::MakeValidFileName(V.Name)!=V.Name || Names.Contains(V.Name)
        || !Row->TryGetArrayField(TEXT("eye_cm"),EyeValues) || EyeValues->Num()!=3
        || !Row->TryGetArrayField(TEXT("aim_cm"),AimValues) || AimValues->Num()!=3)
      return End(TEXT("Asset survey view must have a unique safe name and explicit three-component coordinates"));
     for(int32 Axis=0;Axis<3;++Axis)
     {
      double E=0,A=0;if(!(*EyeValues)[Axis]->TryGetNumber(E) || !(*AimValues)[Axis]->TryGetNumber(A) || !FMath::IsFinite(E) || !FMath::IsFinite(A)
                         || FMath::Abs(E)>1000000 || FMath::Abs(A)>1000000)return End(TEXT("Asset survey coordinates must be finite and local"));
      V.Eye[Axis]=E;V.Aim[Axis]=A;
     }
     if(FVector::Dist(V.Eye,V.Aim)<50)return End(TEXT("Asset survey eye and aim must be separated"));
     Row->TryGetStringField(TEXT("isolate_actor_tag"),V.IsolateTag);
     Row->TryGetBoolField(TEXT("player_height_grounded"),V.GroundedEye);
     Row->TryGetStringField(TEXT("expected_npc_mesh"),V.ExpectedNPCMesh);
     Row->TryGetStringField(TEXT("lighting_mode"),V.Lighting);Row->TryGetStringField(TEXT("detail_target"),V.Detail);
     Row->TryGetStringField(TEXT("paving_ab278"),V.PavingAB);Row->TryGetNumberField(TEXT("fixed_exposure278"),V.FixedExposure);
     if(!V.PavingAB.IsEmpty() && (!TArray<FString>{TEXT("baseline"),TEXT("flat"),TEXT("color"),TEXT("roughness"),TEXT("normal"),TEXT("restore")}.Contains(V.PavingAB) || V.FixedExposure<.0001 || V.FixedExposure>.001 || V.Lighting!=TEXT("target16.5") || !V.IsolateTag.IsEmpty()))return End(TEXT("Paving278 diagnostic requires exact bounded variants, original target lighting and measured exposure"));
     if(V.Lighting!=TEXT("target16.5") && V.Lighting!=TEXT("inspection_neutral235"))return End(TEXT("Unknown P1 lighting profile"));
     if(V.Lighting==TEXT("inspection_neutral235") && !V.Name.Contains(TEXT("Inspection")))return End(TEXT("Neutral inspection evidence must be visibly named Inspection"));
     if(!V.IsolateTag.IsEmpty() && !V.IsolateTag.StartsWith(TEXT("NeonCity.MainlandC923.P1")))return End(TEXT("Isolation is restricted to one explicit P1 sample tag"));
     double Number=0;
     if(Row->TryGetNumberField(TEXT("isolate_actor_count"),Number)){if(Number<1 || Number>4 || Number!=FMath::FloorToDouble(Number))return End(TEXT("P1 isolation count must be1..4"));V.IsolateCount=int32(Number);}
     if(Row->TryGetNumberField(TEXT("settle_seconds"),Number)){if(!FMath::IsFinite(Number) || Number<6 || Number>20)return End(TEXT("P1-only settling must retain at least6 real seconds"));V.SettleSeconds=Number;}
     if(Row->TryGetNumberField(TEXT("motion_seconds"),Number)){if(!FMath::IsFinite(Number) || Number<0 || Number>12)return End(TEXT("P1 motion must be bounded0..12seconds"));V.MotionSeconds=Number;}
     Row->TryGetStringField(TEXT("npc_identity"),V.NPCIdentity);
     if(!V.NPCIdentity.IsEmpty() && (!V.IsolateTag.IsEmpty() || (V.NPCIdentity!=TEXT("SouthBeach.Pedestrian.01") && V.NPCIdentity!=TEXT("SouthBeach.Pedestrian.02"))))return End(TEXT("P1 tracking is limited to the two existing couple identities, with normal world visibility"));
     Names.Add(V.Name);AssetViews.Add(V);
    }
    TotalShots=1+AssetViews.Num();Series=true;
   }
   WaterAB62=FParse::Param(FCommandLine::Get(),TEXT("C923WaterAB62"));
   if(WaterAB62)
   {
    TotalShots=3;Series=true;
    TInlineComponentArray<UDirectionalLightComponent*> Lights(Clock.Get());
    for(auto* Light:Lights)if(Light->GetFName()==TEXT("Sun"))DiagnosticSun=Light;
    if(!DiagnosticSun.IsValid())return End(TEXT("Diagnostic62 requires actual sun"));
    OriginalSpecular=DiagnosticSun->SpecularScale;DiagnosticSun->SetSpecularScale(0.f);
    if(FParse::Param(FCommandLine::Get(),TEXT("C923SeabedOnly65")))
    {
     Shot=2;DiagnosticSun->SetSpecularScale(OriginalSpecular);
     if(!SeabedAB62.Begin(W,Directory/TEXT("SeabedAB65")))return End(TEXT("Diagnostic65 exact current cosmetic seabed guard refused"));
    }
   }
   if(FParse::Param(FCommandLine::Get(),TEXT("C923WaterReadOnly56"))){Series=true;TotalShots=3;}
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCitySummerCloseOnly46"))){Shot=4;TotalShots=8;}
   Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(Shot<4?Hours[Shot]:12.f);AtWall=FPlatformTime::Seconds();AtGame=W->GetTimeSeconds();return false;
  }
  ANeonCityNPCCharacter* CloseNPC=nullptr;
  const FAssetView* P1View=Shot>0 && !AssetViews.IsEmpty()?&AssetViews[Shot-1]:nullptr;
  const double RequiredSettle=P1View?P1View->SettleSeconds:20.;
  const double RequiredDuration=RequiredSettle+(P1View?P1View->MotionSeconds:0.);
  if(P1View && !P1View->NPCIdentity.IsEmpty())
  {
   for(TActorIterator<APedestrianSpawnPoint> I(W);I;++I)if(I->PedestrianId.ToString()==P1View->NPCIdentity){CloseNPC=I->GetSpawnedPedestrian();break;}
   const FString SkinName=CloseNPC?GetNameSafe(CloseNPC->GetMesh()->GetSkeletalMeshAsset()):FString();
   const FString ExpectedSkin=P1View->ExpectedNPCMesh.IsEmpty()?(P1View->NPCIdentity==TEXT("SouthBeach.Pedestrian.01")?TEXT("SK_C923_Couple196_Male"):TEXT("SK_C923_Couple196_Female")):P1View->ExpectedNPCMesh;
   if(!CloseNPC || CloseNPC->IsPopulationDormant() || !CloseNPC->GetMesh()->GetAnimInstance() || SkinName!=ExpectedSkin)return End(TEXT("P1 tracking requires its exact admitted skin revision and normal animation instance"));
   if(FPlatformTime::Seconds()-AtWall<RequiredDuration || W->GetTimeSeconds()-AtGame<RequiredDuration || Frames<24)
   {
    const FVector Aim=CloseNPC->GetActorTransform().TransformPosition(P1View->Aim),Eye=CloseNPC->GetActorTransform().TransformPosition(P1View->Eye);
    Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
   }
  }
  if(Shot==0)for(TActorIterator<APedestrianSpawnPoint> I(W);I;++I)
  {
   if(I->AmbientActivity!=ECoastalAmbientActivity::Stroll)continue;
   auto* N=I->GetSpawnedPedestrian();if(!N || N->IsPopulationDormant())continue;
   const FString Id=I->PedestrianId.ToString();const FVector P=N->GetActorLocation();StrollFirst.FindOrAdd(Id,P);
   if(const FVector* Previous=StrollPrevious.Find(Id))StrollPath.FindOrAdd(Id)+=FVector::Dist2D(*Previous,P);
   StrollPrevious.Add(Id,P);
   if(N->GetCharacterMovement()->IsMovingOnGround() && N->GetLastMovementInputVector().Size2D()>.1f && N->GetVelocity().Size2D()>1.f)NativeStrollInputs.Add(Id);
  }
  if(Shot>=4 && AssetViews.IsEmpty())
  {
   const int32 IDs[4]={1,2,3,8};const FString Identity=FString::Printf(TEXT("SouthBeach.Pedestrian.%02d"),IDs[Shot-4]);
   for(TActorIterator<APedestrianSpawnPoint> I(W);I;++I)if(I->PedestrianId.ToString()==Identity){CloseNPC=I->GetSpawnedPedestrian();break;}
   if(!CloseNPC || !CloseNPC->GetMesh()->GetSkeletalMeshAsset() || !CloseNPC->GetMesh()->GetAnimInstance())return End(TEXT("New summer skin must be active with its normal runtime animation"));
   const FString Mesh=CloseNPC->GetMesh()->GetSkeletalMeshAsset()->GetPathName();
   if(!Mesh.StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/SK_C923_Summer37_")))return End(TEXT("Summer close view found an old or placeholder skin"));
   const FVector Aim=CloseNPC->GetActorLocation();const FVector Eye=Aim+CloseNPC->GetActorForwardVector()*350.+CloseNPC->GetActorRightVector()*75.+FVector(0,0,60);
   // The controller caches its view earlier in this frame. On the readback
   // frame retain the preceding authored pose so the verified projection is
   // the pose actually rendered, without ticking gameplay/camera twice.
   if(FPlatformTime::Seconds()-AtWall<20 || W->GetTimeSeconds()-AtGame<20 || Frames<24)
    Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
  }
  ++Frames;if(GShaderCompilingManager && GShaderCompilingManager->IsCompiling()){AtWall=FPlatformTime::Seconds();AtGame=W->GetTimeSeconds();Frames=0;return false;}
  const double Age=FPlatformTime::Seconds()-AtWall;
  if(P1View && P1View->MotionSeconds>0 && Age>=RequiredSettle && W->GetTimeSeconds()-AtGame>=RequiredSettle && W->GetTimeSeconds()-AssetMotionLast>=1.f/12.f)
  {
   auto* MotionClient=W->GetGameViewport();auto MotionWidget=MotionClient?MotionClient->GetGameViewportWidget():nullptr;TArray<FColor> MotionPixels;FIntVector MotionSize;
   if(!MotionWidget || !FSlateApplication::Get().TakeScreenshot(MotionWidget.ToSharedRef(),MotionPixels,MotionSize) || MotionSize.X!=1774 || MotionSize.Y!=887 || MotionPixels.Num()!=1774*887)return End(TEXT("P1 actual runtime motion readback failed"));
   const FString Folder=Directory/TEXT("P1Motion")/P1View->Name;IFileManager::Get().MakeDirectory(*Folder,true);
   if(AssetMotionFrame==0 && !FFileHelper::SaveStringToFile(TEXT("frame,game_seconds,wall_seconds,camera_x,camera_y,camera_z,npc_speed_cm_s\n"),*(Folder/TEXT("frames.csv"))))return End(TEXT("P1 motion header save failed"));
   for(auto& Pixel:MotionPixels)Pixel.A=255;TArray64<uint8> Bytes;
   if(!FImageUtils::CompressImage(Bytes,TEXT("jpg"),FImageView(MotionPixels.GetData(),1774,887),92) || !FFileHelper::SaveArrayToFile(Bytes,*(Folder/FString::Printf(TEXT("%06d.jpg"),AssetMotionFrame))))return End(TEXT("P1 motion JPEG save failed"));
   FVector E;FRotator ActualMotionRotation;PC->GetPlayerViewPoint(E,ActualMotionRotation);const FString Row=FString::Printf(TEXT("%d,%.6f,%.6f,%.4f,%.4f,%.4f,%.4f\n"),AssetMotionFrame,W->GetTimeSeconds(),FPlatformTime::Seconds(),E.X,E.Y,E.Z,CloseNPC?CloseNPC->GetVelocity().Size2D():0.f);
   if(!FFileHelper::SaveStringToFile(Row,*(Folder/TEXT("frames.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append))return End(TEXT("P1 motion timestamp save failed"));
   ++AssetMotionFrame;AssetMotionLast=W->GetTimeSeconds();
  }
  if(Age<RequiredDuration || W->GetTimeSeconds()-AtGame<RequiredDuration || Frames<24)return false;
  if(P1View && P1View->MotionSeconds>0 && AssetMotionFrame<24)return End(TEXT("P1 wind/gait evidence requires at least24 actual rendered frames"));
  if(!PC.IsValid() || PC->GetViewTarget()!=Camera.Get() || !PC->IsPhotographyHUDHidden())return End(TEXT("Reference view/HUD state changed before actual capture"));
  for(const auto& V:Opacities)if(V.Key.IsValid() && V.Key->GetRenderOpacity()!=0.f)return End(TEXT("An existing HUD widget became visible in the reference frame"));
  auto* Client=W->GetGameViewport();auto* Local=PC->GetLocalPlayer();if(!Client || !Client->Viewport || !Local || Client->ViewModeIndex!=VMI_Lit)return End(TEXT("Normal Lit game viewport required"));
  FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Client->Viewport,W->Scene,Client->EngineShowFlags).SetRealtimeUpdate(true));
  FVector Eye;FRotator Rotation;auto* View=Local->CalcSceneView(&Family,Eye,Rotation,Client->Viewport);
  if(!View || !Eye.Equals(Camera->GetActorLocation(),.1f) || View->UnconstrainedViewRect.Size()!=FIntPoint(1774,887) || View->UnscaledViewRect.Size()!=FIntPoint(1774,887))
   return End(TEXT("Actual camera projection must fill1774x887 without stretching, crop or letterbox"));
  if(CouplePhase351 && Shot==0 && !ObserveCouplePhase351(W,View,Eye))
  {
   if(FPlatformTime::Seconds()-CouplePhase351Start<CouplePhase351Policy->GetNumberField(TEXT("max_wait_seconds")))return false;
   FString Text;FJsonSerializer::Serialize(CouplePhase351Receipt(false),TJsonWriterFactory<>::Create(&Text));
   if(!FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("couple-phase351-timeout.json"))))return End(TEXT("Live couple phase351 timed out and failed to retain its actual pose observations"));
   return End(TEXT("Live couple phase351 never reached the explicit away-facing grounded target windows within its bounded wait; no NPC state or transform was changed"));
  }
  const auto* Site=ANeonCityMainlandSite::Find(W);int32 Providers=0;
  if(!Site || !Site->OceanRenderingMaterial)return End(TEXT("Authored mainland optical material must remain bound in the actual runtime world"));
  for(TActorIterator<ACoastalOceanSurface> I(W);I;++I)
  {
   ++Providers;TInlineComponentArray<UInstancedStaticMeshComponent*> Surfaces(*I);
   UMaterialInstanceDynamic* MID=nullptr;
   for(auto* Surface:Surfaces)if(Surface->GetFName()==TEXT("OceanRenderGrid"))MID=Cast<UMaterialInstanceDynamic>(Surface->GetMaterial(0));
   if(!MID || MID->GetMaterial()!=Site->OceanRenderingMaterial->GetMaterial() || I->GetField()!=Site->OceanField || !I->IsReady() || I->GetStrength()<=0.f)
    return End(TEXT("Actual ocean provider must render the authored local derivative with its original ready physical field"));
   ActualWaterParent=MID->GetMaterial()->GetPathName();
  }
  if(Providers!=1)return End(TEXT("Reference capture requires the original single ocean provider"));
  if((WaterAB62 || FParse::Param(FCommandLine::Get(),TEXT("C923WaterReadOnly56"))) && !C923MarinaWaterAttribution56::Write(W,PC.Get(),Shot,Directory))
   return End(TEXT("Actual read-only water diagnostic could not be saved"));
  auto Widget=Client->GetGameViewportWidget();TArray<FColor> Pixels;FIntVector Size;
  if(!Widget || !FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(),Pixels,Size) || Size.X!=1774 || Size.Y!=887 || Pixels.Num()!=1774*887)
   return End(TEXT("Original-aspect rendered frame readback failed"));
  for(auto& Pixel:Pixels)Pixel.A=255;TArray64<uint8> JPEG;
  const TCHAR* Names[8]={TEXT("Marina-Reference-Target"),TEXT("Marina-Reference-Day"),TEXT("Marina-Reference-Sunset"),TEXT("Marina-Reference-Night"),TEXT("Summer37-Male-Shirt-Gameplay"),TEXT("Summer37-Female-Skirt-Gameplay"),TEXT("Summer37-Female-Jeans-Gameplay"),TEXT("Summer37-Service-Shirt-Gameplay")};
  const FString Stem=Shot>0 && !AssetViews.IsEmpty()?AssetViews[Shot-1].Name:FString(Names[Shot]);
  if(!FImageUtils::CompressImage(JPEG,TEXT("jpg"),FImageView(Pixels.GetData(),1774,887),97) || !FFileHelper::SaveArrayToFile(JPEG,*(Directory/(Stem+TEXT(".jpg")))))return End(TEXT("Actual reference JPEG save failed"));
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("capture_technical_pass"),true);J->SetBoolField(TEXT("visual_acceptance_claimed"),false);
  J->SetStringField(TEXT("camera"),Camera->GetPathName());J->SetStringField(TEXT("eye_cm"),Eye.ToString());J->SetStringField(TEXT("rotation"),Rotation.ToString());
  J->SetNumberField(TEXT("horizontal_fov"),Camera->GetCameraComponent()->FieldOfView);J->SetNumberField(TEXT("width"),1774);J->SetNumberField(TEXT("height"),887);
  J->SetBoolField(TEXT("hud_hidden"),true);J->SetBoolField(TEXT("normal_lit_game_view"),true);J->SetBoolField(TEXT("image_stretched_or_background_replaced"),false);
  J->SetNumberField(TEXT("hour"),Clock->GetTimeOfDay());J->SetNumberField(TEXT("stable_wall_seconds"),Age);J->SetNumberField(TEXT("stable_frames"),Frames);
  J->SetNumberField(TEXT("actual_exposure_bias"),View->FinalPostProcessSettings.AutoExposureBias);J->SetNumberField(TEXT("actual_eye_adaptation_exposure"),View->GetLastEyeAdaptationExposure());
  J->SetBoolField(TEXT("fixed_reference_comparison"),Shot==0 || (AssetViews.IsEmpty() && Shot<4));
  if(CouplePhase351 && Shot==0)J->SetObjectField(TEXT("live_couple_phase351"),CouplePhase351Receipt(true));
  J->SetBoolField(TEXT("asset_survey_transient_camera"),!AssetViews.IsEmpty() && Shot>0);
  J->SetStringField(TEXT("isolated_asset_tag"),Shot>0 && !AssetViews.IsEmpty()?AssetViews[Shot-1].IsolateTag:FString());
  J->SetBoolField(TEXT("normal_world_visibility"),AssetVisibility.IsEmpty());
  J->SetStringField(TEXT("lighting_mode"),P1View?P1View->Lighting:FString(TEXT("target16.5")));
  if(P1View && !P1View->PavingAB.IsEmpty())
  {
   if(FMath::Abs(View->GetLastEyeAdaptationExposure()-P1View->FixedExposure)>P1View->FixedExposure*.005)return End(TEXT("Paving278 exposure must remain within0.5percent of the original measured shot"));
   J->SetStringField(TEXT("paving_ab278"),P1View->PavingAB);J->SetNumberField(TEXT("requested_existing_exposure278"),P1View->FixedExposure);J->SetBoolField(TEXT("temporary_override_restoration_scheduled"),true);
  }
  J->SetBoolField(TEXT("inspection_lighting_not_target_match"),Inspection.Active);
  if(P1View)
  {
   // NPC manifests store aim offsets in the tracked actor's local space.
   // Static inspection manifests already store world coordinates.
   const FVector ObservationAim=CloseNPC?CloseNPC->GetActorTransform().TransformPosition(P1View->Aim):P1View->Aim;
   J->SetStringField(TEXT("detail_target"),P1View->Detail);J->SetNumberField(TEXT("eye_to_aim_cm"),FVector::Dist(Eye,ObservationAim));
   FHitResult Surface;FCollisionQueryParams SurfaceQuery(SCENE_QUERY_STAT(C923P1SurfaceDistance),true,Player.Get());
   const bool Hit=W->LineTraceSingleByObjectType(Surface,Eye,Eye+(ObservationAim-Eye).GetSafeNormal()*10000.f,FCollisionObjectQueryParams(ECC_WorldStatic),SurfaceQuery);
   J->SetBoolField(TEXT("observation_surface_hit"),Hit);
   if(Hit){J->SetNumberField(TEXT("eye_to_actual_surface_cm"),Surface.Distance);J->SetStringField(TEXT("observation_surface_actor"),Surface.GetActor()->GetPathName());}
   if(Inspection.Active){J->SetNumberField(TEXT("inspection_key_lux"),30000);J->SetNumberField(TEXT("inspection_key_kelvin"),6500);J->SetStringField(TEXT("inspection_sun_rotation"),TEXT("P=-45 Y=265 R=0"));J->SetNumberField(TEXT("inspection_sky_intensity"),.85);}
  }
  if(P1View){J->SetNumberField(TEXT("required_settle_seconds"),RequiredSettle);J->SetNumberField(TEXT("actual_motion_frames"),AssetMotionFrame);J->SetStringField(TEXT("tracked_existing_npc"),P1View->NPCIdentity);J->SetBoolField(TEXT("P1_single_asset_visual_acceptance_claimed"),false);}
  if(P1View && P1View->Name.Contains(TEXT("Nearest")))
  {
   FHitResult Floor;FCollisionQueryParams Query(SCENE_QUERY_STAT(C923P1NearestProof),true,Player.Get());
   const bool Hit=W->LineTraceSingleByObjectType(Floor,Eye,Eye-FVector(0,0,3000),FCollisionObjectQueryParams(ECC_WorldStatic),Query);
   J->SetBoolField(TEXT("nearest_ground_found"),Hit);
   if(Hit)
   {
    const double Height=Eye.Z-Floor.ImpactPoint.Z;const float Half=Player->GetSimpleCollisionHalfHeight(),Radius=Player->GetSimpleCollisionRadius();
    const bool Blocked=W->OverlapBlockingTestByChannel(FVector(Eye.X,Eye.Y,Floor.ImpactPoint.Z+Half+2.f),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half-1.f),Query);
    J->SetNumberField(TEXT("nearest_eye_above_floor_cm"),Height);J->SetNumberField(TEXT("nearest_floor_normal_z"),Floor.ImpactNormal.Z);J->SetStringField(TEXT("nearest_floor_actor"),GetPathNameSafe(Floor.GetActor()));J->SetBoolField(TEXT("nearest_player_capsule_clear"),!Blocked);
    J->SetBoolField(TEXT("nearest_player_observation_supported"),Height>=150 && Height<=180 && Floor.ImpactNormal.Z>.707f && !Blocked);
   }
  }
  J->SetBoolField(TEXT("water_read_only_diagnostic56"),FParse::Param(FCommandLine::Get(),TEXT("C923WaterReadOnly56")));
  J->SetBoolField(TEXT("diagnostic62_sun_specular_off"),WaterAB62 && Shot==0);
  J->SetBoolField(TEXT("diagnostic62_two_seabeds_hidden"),WaterAB62 && Shot==2);
  if(DiagnosticSun.IsValid())J->SetNumberField(TEXT("actual_sun_specular_scale"),DiagnosticSun->SpecularScale);
  J->SetNumberField(TEXT("cold_mooring_textures_verified"),ColdMoorTextures);
  J->SetNumberField(TEXT("cold_broad_canopy_groups_verified"),ColdBroadGroups);J->SetNumberField(TEXT("cold_broad_canopy_instances_and_semantic_buffers_verified"),ColdBroadTrees);
  J->SetStringField(TEXT("actual_runtime_water_material"),ActualWaterParent);
  J->SetBoolField(TEXT("actual_original_single_water_provider_and_field"),true);
  TArray<TSharedPtr<FJsonValue>> AuthoredPlantSlots;int32 AuthoredPalmOwners=0;
  const bool RequireAuthoredPalm=AssetViews.ContainsByPredicate([](const FAssetView& V){return V.Name.StartsWith(TEXT("Asset127-Palm148-"));});
  for(TActorIterator<ANeonCityReferenceVegetation> I(W);I;++I)
   if(I->Data && I->Data->GetName().StartsWith(TEXT("DA_C923_HeroPalm148")))
   {
    ++AuthoredPalmOwners;
    auto* Plant=I->GetPlantGroup(0);
    if(!Plant || I->GetPlantCount()!=1 || I->GetFarPalmCount()!=0 || Plant->GetNumMaterials()!=5)return End(TEXT("Authored palm requires one actual full-geometry instance and all five real material slots"));
    FTransform ActualPalmPose;if(!Plant->GetInstanceTransform(0,ActualPalmPose,true))return End(TEXT("Actual palm world transform missing"));
    J->SetStringField(TEXT("actual_authored_palm_owner"),I->GetPathName());J->SetStringField(TEXT("actual_authored_palm_pose"),ActualPalmPose.ToString());
    if(RequireAuthoredPalm && Shot==0)
    {
     auto* Contact=I->GetFoliageInteraction();
     if(!Contact || Contact->GetActivePalmCount()!=1)return End(TEXT("The real cafe observer must activate the authored palm wood contact"));
     FCollisionQueryParams OnlyPalm(SCENE_QUERY_STAT(Production148ActualWood),false);
     for(TActorIterator<AActor> Other(W);Other;++Other)if(*Other!=*I)OnlyPalm.AddIgnoredActor(*Other);
     const FVector Root=ActualPalmPose.GetLocation();FHitResult Wood;
     const bool WoodBlocked=W->LineTraceSingleByChannel(Wood,Root+FVector(0,-200,110),Root+FVector(0,200,110),ECC_Pawn,OnlyPalm);
     if(!WoodBlocked || Wood.GetActor()!=*I || !Wood.GetComponent() || !Wood.GetComponent()->ComponentHasTag(TEXT("NeonCity.Foliage.PalmWood")))return End(TEXT("Actual authored palm trunk requires the native wood blocker"));
     const auto Shape=FCollisionShape::MakeCapsule(Player->GetSimpleCollisionRadius(),Player->GetSimpleCollisionHalfHeight()-1);
     for(int32 Probe=0;Probe<8;++Probe)
     {
      const double A=Probe*UE_TWO_PI/8.;const FVector Point=Root+FVector(FMath::Cos(A)*120,FMath::Sin(A)*120,Player->GetSimpleCollisionHalfHeight()+2);
      if(W->OverlapBlockingTestByChannel(Point,FQuat::Identity,ECC_Pawn,Shape,OnlyPalm))return End(TEXT("No palm crown box may block the actual player capsule 1.2m from the trunk"));
     }
     J->SetBoolField(TEXT("actual_palm_wood_block_and_eight_crown_clearance_queries_pass"),true);
    }
    for(int32 Slot=0;Slot<Plant->GetNumMaterials();++Slot)
    {
     auto* MID=Cast<UMaterialInstanceDynamic>(Plant->GetMaterial(Slot));
     if(!MID)return End(TEXT("All authored plant slots require the normal shared wind/observer binding"));
     auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("slot"),Slot);Row->SetStringField(TEXT("parent"),GetPathNameSafe(MID->Parent));
     if(Slot>0)
     {
      const float Wind=MID->K2_GetScalarParameterValue(TEXT("WindSpeed"));
      if(!FMath::IsNearlyEqual(Wind,I->WindSpeed,.001f))return End(TEXT("Actual leaf/rachis wind differs from the owner weather input"));
      Row->SetNumberField(TEXT("actual_wind_speed"),Wind);
     }
     AuthoredPlantSlots.Add(MakeShared<FJsonValueObject>(Row));
    }
   }
  if(RequireAuthoredPalm && AuthoredPalmOwners!=1)return End(TEXT("Palm148 qualification requires exactly one actual saved owner; missing or duplicate binding cannot pass"));
  J->SetBoolField(TEXT("authored_palm_gate_required"),RequireAuthoredPalm);J->SetNumberField(TEXT("actual_authored_palm_owners"),AuthoredPalmOwners);
  J->SetArrayField(TEXT("actual_authored_palm_material_bindings"),AuthoredPlantSlots);
  TArray<TSharedPtr<FJsonValue>> StrollRows;int32 ActualStrollMovers=0,ActualCoupleMovers=0;
  for(TActorIterator<APedestrianSpawnPoint> I(W);I;++I)if(I->AmbientActivity==ECoastalAmbientActivity::Stroll)
  {
   auto R=MakeShared<FJsonObject>();const FString Id=I->PedestrianId.ToString();auto* N=I->GetSpawnedPedestrian();
   R->SetStringField(TEXT("id"),Id);R->SetBoolField(TEXT("actual_admitted"),N && !N->IsPopulationDormant());
   R->SetNumberField(TEXT("observed_native_path_cm"),StrollPath.FindRef(Id));R->SetBoolField(TEXT("actual_grounded_movement_input_observed"),NativeStrollInputs.Contains(Id));
   if(const FVector* First=StrollFirst.Find(Id))R->SetStringField(TEXT("first_position_cm"),First->ToString());
   if(N){R->SetStringField(TEXT("current_position_cm"),N->GetActorLocation().ToString());R->SetStringField(TEXT("actual_velocity_cm_s"),N->GetVelocity().ToString());R->SetStringField(TEXT("actual_state"),StaticEnum<ENeonCityNPCState>()->GetNameStringByValue(int64(N->GetNPCState()->GetState())));}
   if(N && (Id==TEXT("SouthBeach.Pedestrian.01") || Id==TEXT("SouthBeach.Pedestrian.02")))
   {
    const auto* Capsule=N->GetCapsuleComponent();
    R->SetStringField(TEXT("authored_marker_cm"),I->GetActorLocation().ToString());
    R->SetNumberField(TEXT("capsule_radius_cm"),Capsule->GetScaledCapsuleRadius());R->SetNumberField(TEXT("capsule_halfheight_cm"),Capsule->GetScaledCapsuleHalfHeight());
    R->SetBoolField(TEXT("actual_anim_root_motion"),N->IsPlayingRootMotion());
    TArray<TSharedPtr<FJsonValue>> Route;for(const FVector& P:N->GetRoute())Route.Add(MakeShared<FJsonValueString>(P.ToString()));R->SetArrayField(TEXT("actual_route_cm"),Route);
    // Unlike the offline WorldStatic-only authoring proof, this queries the
    // actual Pawn channel and includes dynamic scene furniture. Ignore only
    // characters for the environment test; record actual body overlap below.
    FCollisionQueryParams Q(SCENE_QUERY_STAT(C923CoupleEnvironmentRoute),false,N);
    for(TActorIterator<ACharacter> Body(W);Body;++Body)Q.AddIgnoredActor(*Body);
    const FCollisionShape Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-1.f);
    for(int32 Index=1;Index<N->GetRoute().Num();++Index)
    {
     FHitResult Hit;if(!W->SweepSingleByChannel(Hit,N->GetRoute()[Index-1],N->GetRoute()[Index],FQuat::Identity,ECC_Pawn,Shape,Q))continue;
     auto Block=MakeShared<FJsonObject>();Block->SetNumberField(TEXT("segment"),Index-1);Block->SetStringField(TEXT("actor"),GetPathNameSafe(Hit.GetActor()));Block->SetStringField(TEXT("component"),GetPathNameSafe(Hit.GetComponent()));Block->SetStringField(TEXT("point_cm"),Hit.ImpactPoint.ToString());
     if(const auto* Mesh=Cast<UStaticMeshComponent>(Hit.GetComponent()))Block->SetStringField(TEXT("mesh"),GetPathNameSafe(Mesh->GetStaticMesh()));
     R->SetObjectField(TEXT("first_actual_pawn_environment_blocker"),Block);break;
    }
    FCollisionQueryParams BodyQuery(SCENE_QUERY_STAT(C923CoupleBodyOverlap),false,N);TArray<FOverlapResult> Overlaps;
    W->OverlapMultiByChannel(Overlaps,I->GetActorLocation(),FQuat::Identity,ECC_Pawn,Shape,BodyQuery);
    TArray<TSharedPtr<FJsonValue>> Bodies;for(const auto& O:Overlaps)if(O.bBlockingHit)Bodies.Add(MakeShared<FJsonValueString>(GetPathNameSafe(O.GetActor())));R->SetArrayField(TEXT("actual_authored_start_blocking_bodies"),Bodies);
   }
   if(StrollPath.FindRef(Id)>=80. && NativeStrollInputs.Contains(Id))
   {
    ++ActualStrollMovers;
    if(Id==TEXT("SouthBeach.Pedestrian.01") || Id==TEXT("SouthBeach.Pedestrian.02"))++ActualCoupleMovers;
   }
   StrollRows.Add(MakeShared<FJsonValueObject>(R));
  }
  J->SetArrayField(TEXT("actual_unpaused_stroll_observations"),StrollRows);J->SetNumberField(TEXT("actual_native_stroll_movers"),ActualStrollMovers);
  J->SetNumberField(TEXT("actual_couple_native_movers"),ActualCoupleMovers);
  if(Shot==0)Test->TestEqual(TEXT("Both reference couple members walk through native gameplay"),ActualCoupleMovers,2);
  if(CloseNPC){J->SetStringField(TEXT("actual_npc"),CloseNPC->GetPathName());J->SetStringField(TEXT("actual_skin"),CloseNPC->GetMesh()->GetSkeletalMeshAsset()->GetPathName());J->SetStringField(TEXT("actual_animation"),CloseNPC->GetMesh()->GetAnimInstance()->GetClass()->GetPathName());J->SetStringField(TEXT("actual_velocity_cm_s"),CloseNPC->GetVelocity().ToString());J->SetStringField(TEXT("scope"),TEXT("Unpaused normal NPC animation; following camera fixture restored at cleanup. Additional native skin close view, not a fixed reference comparison."));}
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));if(!FFileHelper::SaveStringToFile(Text,*(Directory/(Shot==0?FString(TEXT("receipt.json")):Stem+TEXT(".json")))))return End(TEXT("Actual reference receipt save failed"));
  if(++Shot<TotalShots)
  {
   if((WaterAB62 || FParse::Param(FCommandLine::Get(),TEXT("C923WaterReadOnly56"))) && Shot==1)Shot=2;
   if(WaterAB62 && Shot==2){DiagnosticSun->SetSpecularScale(OriginalSpecular);if(!SeabedAB62.Begin(W,Directory/TEXT("SeabedAB62")))return End(TEXT("Diagnostic62 exact cosmetic seabed guard refused"));}
   if(!AssetViews.IsEmpty())
   {
    if(!ApplyAssetView(W,AssetViews[Shot-1]))return End(TEXT("An isolated asset view requires exactly one actual P1 sample; previous visibility restored"));
   }
   else Clock->SetTimeOfDay(Shot<4?Hours[Shot]:12.f);
   AtWall=FPlatformTime::Seconds();AtGame=W->GetTimeSeconds();Frames=0;AssetMotionFrame=0;AssetMotionLast=-1;return false;
  }
  return End();
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarinaReferenceTarget35,"NeonCity.Phase205c.MainlandC923.Standalone.ReferenceTarget",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMarinaReferenceTarget35::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(MarinaReference35::FCapture(this));return true;}
#endif
