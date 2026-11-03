#include "IslandLandAuthoringLibrary.h"
#include "LandscapeProxy.h"
#include "Editor.h"
#include "Containers/Ticker.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "DynamicRHI.h"
#include "RHIStats.h"
#include "ShaderCompiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "LandscapeComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "ComponentReregisterContext.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Engine/GameInstance.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Engine/Texture2D.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
namespace
{
 FTSTicker::FDelegateHandle LandMetrics;
 FString MetricsFile,Rows;
 double MetricsStart=0,PreviousFrame=0;
 double MemorySampleAt=0;
 FRHIMemoryStats ReviewMemory;
 int32 BufferedFrames=0;
 void FlushMetrics()
 {
  TRACE_CPUPROFILER_EVENT_SCOPE(LandPass1_FlushMetrics);
  if(!Rows.IsEmpty()){FFileHelper::SaveStringToFile(Rows,*MetricsFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);Rows.Reset();BufferedFrames=0;}
 }
 bool SampleLandMetrics(float)
 {
  TRACE_CPUPROFILER_EVENT_SCOPE(LandPass1_ReviewMetrics);
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;APawn* P=PC?PC->GetPawn():nullptr;
  const double Now=FPlatformTime::Seconds();
  if(!P){PreviousFrame=Now;return true;}
  auto* V=W->GetGameViewport();const FIntPoint Size=V && V->Viewport?V->Viewport->GetSizeXY():FIntPoint::ZeroValue;
  const FVector At=P->GetActorLocation();
  // Read the driver's existing D3D memory snapshot at 1 Hz. This records the
  // editor process's local VRAM usage/budget, including the real PIE viewport;
  // it is neither texture-pool size nor adapter-wide usage from other apps.
  if(GDynamicRHI && Now-MemorySampleAt>=1)
  {
   TRACE_CPUPROFILER_EVENT_SCOPE(LandPass1_ReadDriverMemory);
   RHIGetMemoryStats(ReviewMemory);MemorySampleAt=Now;
  }
  const double MiB=1024.0*1024.0;
  // Capture the actual editor throttle decision, including hidden windows,
  // rather than assuming that its background preference is the only cause.
  Rows+=FString::Printf(TEXT("%.6f,%.6f,%.4f,%.4f,%s,%.3f,%.3f,%.3f,%d,%d,%.3f,%.3f,%.3f,%d,%d,%d,%d\n"),
   Now-MetricsStart,W->GetTimeSeconds(),(Now-PreviousFrame)*1000,double(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())),*P->GetName(),At.X,At.Y,At.Z,Size.X,Size.Y,
   ReviewMemory.UsedLocal/MiB,ReviewMemory.BudgetLocal/MiB,ReviewMemory.DemotedLocal/MiB,ReviewMemory.IsValid()?1:0,
   GEditor->ShouldThrottleCPUUsage()?1:0,GShaderCompilingManager && GShaderCompilingManager->IsCompiling()?1:0,IsAsyncLoading()?1:0);
  PreviousFrame=Now;if(++BufferedFrames>=120)FlushMetrics();
  return true;
 }
}
double UIslandLandAuthoringLibrary::TerrainHeightCm(ALandscapeProxy* Terrain,FVector2D PositionCm)
{
 if(!IsValid(Terrain) || PositionCm.ContainsNaN())return -1.e9;
 const auto Height=Terrain->GetHeightAtLocation(FVector(PositionCm.X,PositionCm.Y,0));
 return Height.IsSet()?Height.GetValue():-1.e9;
}
bool UIslandLandAuthoringLibrary::HoldReviewKey(FName Name,float Seconds)
{
 return HoldReviewKeys({Name},Seconds);
}
bool UIslandLandAuthoringLibrary::HoldReviewKeys(const TArray<FName>& Names,float Seconds)
{
 const TArray<FKey> Allowed={EKeys::W,EKeys::A,EKeys::S,EKeys::D,EKeys::SpaceBar,EKeys::C,EKeys::F,EKeys::Enter,EKeys::LeftMouseButton,EKeys::RightMouseButton,EKeys::R,EKeys::Q,EKeys::E,EKeys::LeftShift,EKeys::LeftControl,EKeys::One,EKeys::Two,EKeys::Left,EKeys::Right};
 auto* World=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=World?World->GetFirstPlayerController():nullptr;
 if(!PC || Names.IsEmpty() || Names.Num()>3 || Seconds<=0 || Seconds>12 || !FMath::IsFinite(Seconds))return false;
 TArray<FKey> Keys;
 for(FName Name:Names)
 {
  const FKey Key(Name);if(!Allowed.Contains(Key) || Keys.Contains(Key))return false;
  Keys.Add(Key);
 }
 for(const FKey& Key:Keys)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,IE_Pressed,FPlatformTime::Cycles64()));
 TWeakObjectPtr<APlayerController> Weak=PC;
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,Keys](float)
 {
  if(auto* Controller=Weak.Get())for(const FKey& Key:Keys)Controller->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,IE_Released,FPlatformTime::Cycles64()));
  return false;
 }),Seconds);
 return true;
}
bool UIslandLandAuthoringLibrary::StartReviewMetrics(const FString& Label)
{
 StopReviewMetrics();if(!GEditor || !GEditor->PlayWorld || Label.IsEmpty() || Label!=FPaths::MakeValidFileName(Label))return false;
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1/Metrics");IFileManager::Get().MakeDirectory(*Folder,true);
 MetricsFile=Folder/(Label+TEXT(".csv"));if(IFileManager::Get().FileExists(*MetricsFile))return false;
 if(!FFileHelper::SaveStringToFile(TEXT("wall_seconds,game_seconds,frame_ms,gpu_ms,pawn,x,y,z,width,height,vram_used_mib,vram_budget_mib,vram_demoted_mib,vram_valid,editor_throttled,shader_compiling,async_loading\n"),*MetricsFile))return false;
 ReviewMemory=FRHIMemoryStats();MemorySampleAt=0;
 MetricsStart=PreviousFrame=FPlatformTime::Seconds();Rows.Reset();BufferedFrames=0;
 LandMetrics=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&SampleLandMetrics));return true;
}
void UIslandLandAuthoringLibrary::StopReviewMetrics()
{
 if(LandMetrics.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(LandMetrics);LandMetrics.Reset();}
 FlushMetrics();
}
FString UIslandLandAuthoringLibrary::InspectReviewFishing()
{
 auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
 auto* Fishing=PC?PC->FindComponentByClass<UFishingComponent>():nullptr;
 if(!Fishing)return TEXT("{}");
 const auto& S=Fishing->GetSession();
 FString FishRows,MealRows;int32 Pouch=0;
 if(auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>())
 {
  Pouch=Inventory->PouchCount();
  for(const auto& F:Inventory->GetFish())
  {
   if(!FishRows.IsEmpty())FishRows+=TEXT(",");
   FishRows+=FString::Printf(TEXT("{\"id\":\"%s\",\"species\":\"%s\",\"weight_kg\":%.3f,\"point\":\"%s\"}"),*F.FishID.ToString(),*F.SpeciesID.ToString(),F.WeightKg,*F.FishingPointID.ToString());
  }
  for(const auto& Meal:Inventory->GetMeals())
  {
   if(!MealRows.IsEmpty())MealRows+=TEXT(",");
   MealRows+=FString::Printf(TEXT("{\"id\":\"%s\",\"species\":\"%s\",\"recipe\":\"%s\"}"),*Meal.MealID.ToString(),*Meal.SpeciesID.ToString(),*Meal.RecipeID.ToString());
  }
 }
 return FString::Printf(TEXT("{\"state\":\"%s\",\"failure\":\"%s\",\"elapsed\":%.3f,\"tension\":%.4f,\"progress\":%.4f,\"remaining_line_m\":%.3f,\"fish_stamina\":%.4f,\"reeling\":%s,\"pouch\":%d,\"fish\":[%s],\"meals\":[%s],\"cook_seconds\":%.3f}"),
  *StaticEnum<ENeonCityFishingState>()->GetNameStringByValue(int64(S.State)),*StaticEnum<ENeonCityFishingFailure>()->GetNameStringByValue(int64(S.Failure)),S.Elapsed,S.Tension,S.Progress,S.RemainingLineM,S.FishStamina,S.Reeling?TEXT("true"):TEXT("false"),Pouch,*FishRows,*MealRows,Fishing->GetCookingSeconds());
}
FString UIslandLandAuthoringLibrary::InspectTerrainMaterials(ALandscapeProxy* Terrain)
{
 if(!IsValid(Terrain))return TEXT("Missing terrain");
 FString Report=FString::Printf(TEXT("actor=%s material=%s components=%d dynamic=%d\n"),*Terrain->GetName(),*GetNameSafe(Terrain->LandscapeMaterial),Terrain->LandscapeComponents.Num(),Terrain->bUseDynamicMaterialInstance);
 for(ULandscapeComponent* C:Terrain->LandscapeComponents)if(C)
 {
  Report+=C->GetName();
  for(bool Dynamic:{false,true})
   for(int32 I=0;I<C->GetMaterialInstanceCount(Dynamic);++I)
   {
    auto* M=C->GetMaterialInstance(I,Dynamic);
    Report+=FString::Printf(TEXT(" %s[%d]=%s"),Dynamic?TEXT("dynamic"):TEXT("constant"),I,*GetNameSafe(M?M->GetMaterial():nullptr));
   }
  Report+=TEXT("\n");
 }
 return Report;
}
FString UIslandLandAuthoringLibrary::InspectReviewSave(const FString& Slot)
{
 if(Slot!=TEXT("NeonCity_LandPass1_NormalTour_20261006") && Slot!=TEXT("NeonCity_LandPass1_NormalTour_20261006_Backup"))return TEXT("{\"error\":\"Unsupported review slot\"}");
 auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;
 auto* Player=W?Cast<ANeonCityPlayerCharacter>(UGameplayStatics::GetPlayerPawn(W,0)):nullptr;
 auto* Model=Cast<UNeonCitySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0));
 if(!W || !Player || !Model)return TEXT("{\"error\":\"Normal PIE player and existing save required\"}");
 const FVector P=Model->Player.Transform.GetLocation();
 const auto* Capsule=Player->GetCapsuleComponent();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(LandPass1SavedPlayer),false,Player);
 for(TActorIterator<ANeonCityBaseVehicle> It(W);It;++It)Q.AddIgnoredActor(*It);
 for(TActorIterator<ANeonCityMarineVehicleBase> It(W);It;++It)Q.AddIgnoredActor(*It);
 for(TActorIterator<ANeonCityAircraftVehicleBase> It(W);It;++It)Q.AddIgnoredActor(*It);
 TArray<FOverlapResult> Hits;
 W->OverlapMultiByChannel(Hits,P,FQuat::Identity,ECC_Pawn,
  FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Q);
 FString Contacts,VehicleRows;
 for(const auto& H:Hits)if(H.bBlockingHit)
 {
  if(!Contacts.IsEmpty())Contacts+=TEXT(",");
  Contacts+=FString::Printf(TEXT("{\"actor\":\"%s\",\"component\":\"%s\"}"),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()));
 }
 for(const auto& V:Model->Vehicles)
 {
  const FVector D=V.Transform.InverseTransformPosition(P);
  if(!VehicleRows.IsEmpty())VehicleRows+=TEXT(",");
  VehicleRows+=FString::Printf(TEXT("{\"id\":\"%s\",\"player_local\":[%.6f,%.6f,%.6f],\"generic_box_rejected\":%s}"),
   *V.VehicleID.ToString(EGuidFormats::Digits),D.X,D.Y,D.Z,
   FMath::Abs(D.X)<285 && FMath::Abs(D.Y)<145 && FMath::Abs(D.Z)<220?TEXT("true"):TEXT("false"));
 }
 FHitResult Floor;
 const bool HasFloor=W->LineTraceSingleByChannel(Floor,P,P-FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+20),ECC_Visibility,Q);
 return FString::Printf(TEXT("{\"stored_player\":[%.6f,%.6f,%.6f],\"blocking_contacts\":[%s],\"vehicle_clearance\":[%s],\"floor_hit\":%s,\"floor_actor\":\"%s\",\"floor_component\":\"%s\",\"floor_normal_z\":%.6f,\"walkable_normal_z\":%.6f}"),
  P.X,P.Y,P.Z,*Contacts,*VehicleRows,HasFloor?TEXT("true"):TEXT("false"),*GetNameSafe(Floor.GetActor()),*GetNameSafe(Floor.GetComponent()),Floor.ImpactNormal.Z,Player->GetCharacterMovement()->GetWalkableFloorZ());
}
bool UIslandLandAuthoringLibrary::RefreshTerrainDynamicMaterials(ALandscapeProxy* Terrain)
{
 if(!IsValid(Terrain) || !Terrain->GetWorld() || Terrain->GetWorld()->WorldType!=EWorldType::Editor
  || !Terrain->ActorHasTag(TEXT("NeonCity.Tidewater.Island")) || GetNameSafe(Terrain->LandscapeMaterial)!=TEXT("M_LandTerrain"))return false;
 for(ULandscapeComponent* C:Terrain->LandscapeComponents)if(C){FComponentReregisterContext ReRegister(C);}
 Terrain->MarkComponentsRenderStateDirty();Terrain->MarkPackageDirty();return true;
}
bool UIslandLandAuthoringLibrary::ApplyMountainRelief(ALandscape* Terrain)
{
 if(!Terrain || !Terrain->GetWorld() || Terrain->GetWorld()->WorldType!=EWorldType::Editor
  || !Terrain->ActorHasTag(TEXT("NeonCity.Tidewater.Island")) || Terrain->LandscapeComponents.Num()!=64)return false;
 auto* Info=Terrain->GetLandscapeInfo();int32 X1,Y1,X2,Y2;
 if(!Info || !Info->GetLandscapeExtent(X1,Y1,X2,Y2) || X1!=0 || Y1!=0 || X2!=2040 || Y2!=2040)return false;
 TArray<uint8> Previous,Next,Mask;
 if(!FFileHelper::LoadFileToArray(Previous,*(FPaths::ProjectDir()/TEXT("ArtSource/Polish/Adventure/Road/layby-landscape-2041.r16")))
  || !FFileHelper::LoadFileToArray(Next,*(FPaths::ProjectDir()/TEXT("ArtSource/IslandLandPass1/mountain-relief-2041.r16")))
  || !FFileHelper::LoadFileToArray(Mask,*(FPaths::ProjectDir()/TEXT("ArtSource/IslandLandPass1/mountain-relief-mask.u8")))
  || Previous.Num()!=2041*2041*2 || Next.Num()!=Previous.Num() || Mask.Num()!=2041*2041)return false;
 TArray<uint16> Heights;Heights.SetNumUninitialized(2041*2041);int32 Changed=0;
 for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X)
 {
  const int32 Source=(2040-Y)*2041+X,Byte=Source*2;
  const uint16 Old=uint16(Previous[Byte])|(uint16(Previous[Byte+1])<<8);
  const uint16 New=uint16(Next[Byte])|(uint16(Next[Byte+1])<<8);
  if(Mask[Source]>1 || (!Mask[Source] && Old!=New) || (Old<=20480 && Old!=New) || FMath::Abs(int32(New)-int32(Old))>22*128)return false;
  Changed+=Old!=New;Heights[Y*2041+X]=New;
 }
 if(Changed==0)return false;
 Terrain->Modify();
 {
  const auto* Base=Terrain->GetEditLayerConst(0);if(!Base)return false;
  FScopedSetLandscapeEditingLayer Scope(Terrain,Base->GetGuid());
  FLandscapeEditDataInterface Edit(Info);Edit.SetEditLayer(Base->GetGuid());
  Edit.SetHeightData(0,0,2040,2040,Heights.GetData(),0,true);Edit.Flush();
 }
 Terrain->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_All,true);
 Terrain->ForceUpdateLayersContent();Terrain->RecreateCollisionComponents();Terrain->MarkPackageDirty();return true;
}
bool UIslandLandAuthoringLibrary::ApplySummitRoadRelief(ALandscape* Terrain)
{
 auto Fail=[](const TCHAR* Reason){UE_LOG(LogTemp,Error,TEXT("Summit earthwork refused: %s"),Reason);return false;};
 if(!Terrain || !Terrain->GetWorld() || Terrain->GetWorld()->WorldType!=EWorldType::Editor
  || !Terrain->ActorHasTag(TEXT("NeonCity.Tidewater.Island")) || Terrain->LandscapeComponents.Num()!=64)return Fail(TEXT("unexpected landscape/world/tag/component count"));
 auto* Info=Terrain->GetLandscapeInfo();int32 X1,Y1,X2,Y2;
 if(!Info || !Info->GetLandscapeExtent(X1,Y1,X2,Y2) || X1!=0 || Y1!=0 || X2!=2040 || Y2!=2040)return Fail(TEXT("unexpected extent"));
 TArray<uint8> Previous,Next,Mask;
 if(!FFileHelper::LoadFileToArray(Previous,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/native-summit-base-2041.r16")))
  || !FFileHelper::LoadFileToArray(Next,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-terrain-2041.r16")))
  || !FFileHelper::LoadFileToArray(Mask,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-terrain-mask.u8")))
  || Previous.Num()!=2041*2041*2 || Next.Num()!=Previous.Num() || Mask.Num()!=2041*2041)
 {
  const auto* Layer=Terrain->GetEditLayerConst(0);if(!Layer)return Fail(TEXT("missing baseline layer"));
  FScopedSetLandscapeEditingLayer Scope(Terrain,Layer->GetGuid());FLandscapeEditDataInterface Edit(Info);Edit.SetEditLayer(Layer->GetGuid());
  TArray<uint16> Raw;Raw.SetNumUninitialized(2041*2041);Edit.GetHeightDataFast(0,0,2040,2040,Raw.GetData(),0);
  TArray<uint8> Bytes;Bytes.SetNumUninitialized(2041*2041*2);
  for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X){const uint16 H=Raw[(2040-Y)*2041+X];const int32 B=(Y*2041+X)*2;Bytes[B]=H&255;Bytes[B+1]=H>>8;}
  const FString File=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/SummitNativeBaseline.r16");IFileManager::Get().MakeDirectory(*(FPaths::GetPath(File)),true);
  FFileHelper::SaveArrayToFile(Bytes,*File);return Fail(TEXT("Native baseline exported; regenerate the scoped candidate against this exact physical map"));
 }
 TArray<uint16> Heights;Heights.SetNumUninitialized(2041*2041);int32 Changed=0;
 int32 MinX=2040,MinY=2040,MaxX=0,MaxY=0;
 for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X)
 {
  const int32 SourceY=2040-Y,Source=SourceY*2041+X,Byte=Source*2;
  const uint16 Old=uint16(Previous[Byte])|(uint16(Previous[Byte+1])<<8);
  const uint16 New=uint16(Next[Byte])|(uint16(Next[Byte+1])<<8);
  const float Xm=X*2047.f/2040.f-1023.5f,Ym=SourceY*2047.f/2040.f-1023.5f;
  if(Mask[Source]>1 || (!Mask[Source] && Old!=New) || FMath::Abs(int32(New)-int32(Old))>42*128
   || (Old!=New && (Xm<-128 || Xm>26 || Ym<-566 || Ym>-420)))return Fail(TEXT("mask/bounds/max-delta mismatch"));
  Heights[Y*2041+X]=New;
  if(Old!=New){++Changed;MinX=FMath::Min(MinX,X);MinY=FMath::Min(MinY,Y);MaxX=FMath::Max(MaxX,X);MaxY=FMath::Max(MaxY,Y);}
 }
 if(Changed==0 || Changed>22500)return Fail(TEXT("changed sample count"));
 // A repair may follow the explicitly retained, previously imported scoped
 // candidate. Match those exact bytes rather than weakening the native-base
 // check or silently overwriting an unrelated sculpt.
 TArray<uint8> AcceptedPrevious;
 const FString PriorFile=FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/Previous/SummitBeforeContactRepair/summit-terrain-2041.r16");
 if(IFileManager::Get().FileExists(*PriorFile))
 {
  if(!FFileHelper::LoadFileToArray(AcceptedPrevious,*PriorFile) || AcceptedPrevious.Num()!=Previous.Num())return Fail(TEXT("invalid retained previous candidate"));
  for(int32 S=0;S<2041*2041;++S)
  {
   const int32 B=S*2,Old=int32(Previous[B])|(int32(Previous[B+1])<<8),Prior=int32(AcceptedPrevious[B])|(int32(AcceptedPrevious[B+1])<<8);
   const float Xm=(S%2041)*2047.f/2040.f-1023.5f,Ym=(S/2041)*2047.f/2040.f-1023.5f;
   if(FMath::Abs(Prior-Old)>42*128 || (Prior!=Old && (Xm<-128 || Xm>26 || Ym<-566 || Ym>-420)))return Fail(TEXT("retained previous candidate exceeds protected bounds"));
  }
 }
 const auto* Base=Terrain->GetEditLayerConst(0);if(!Base)return Fail(TEXT("missing base edit layer"));
 Terrain->Modify();
 {
  FScopedSetLandscapeEditingLayer Scope(Terrain,Base->GetGuid());FLandscapeEditDataInterface Edit(Info);Edit.SetEditLayer(Base->GetGuid());
  TArray<uint16> Current;Current.SetNumUninitialized(2041*2041);Edit.GetHeightDataFast(0,0,2040,2040,Current.GetData(),0);
  for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X)
  {
   const int32 Source=(2040-Y)*2041+X,Byte=Source*2;const uint16 Old=uint16(Previous[Byte])|(uint16(Previous[Byte+1])<<8);
   // Native component import rounding can differ by a few 1/128m units. Keep
   // every unedited sample exactly native, including samples inside this rect.
   if(!Mask[Source]){Heights[Y*2041+X]=Current[Y*2041+X];continue;}
   const uint16 Prior=AcceptedPrevious.Num()?uint16(AcceptedPrevious[Byte])|(uint16(AcceptedPrevious[Byte+1])<<8):Old;
   if(FMath::Abs(int32(Current[Y*2041+X])-int32(Old))>4 && FMath::Abs(int32(Current[Y*2041+X])-int32(Prior))>4 && Current[Y*2041+X]!=Heights[Y*2041+X])
   {UE_LOG(LogTemp,Error,TEXT("Summit native base mismatch x%d y%d current%u source%u candidate%u"),X,Y,Current[Y*2041+X],Old,Heights[Y*2041+X]);return Fail(TEXT("native base mismatch"));}
  }
  Edit.SetHeightData(MinX,MinY,MaxX,MaxY,Heights.GetData()+MinY*2041+MinX,2041,true);Edit.Flush();
 }
 Terrain->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_All,true);
 Terrain->ForceUpdateLayersContent();Terrain->RecreateCollisionComponents();Terrain->MarkPackageDirty();return true;
}
bool UIslandLandAuthoringLibrary::ApplyHikeRoadBatter(ALandscape* Terrain)
{
 auto Fail=[](const TCHAR* Reason){UE_LOG(LogTemp,Error,TEXT("Hike batter refused: %s"),Reason);return false;};
 if(!IFileManager::Get().FileExists(*(FPaths::ProjectDir()/TEXT("qa-upgrade-authoring.json")))
  || !Terrain || !Terrain->GetWorld() || Terrain->GetWorld()->WorldType!=EWorldType::Editor
  || !Terrain->ActorHasTag(TEXT("NeonCity.Tidewater.Island")) || Terrain->LandscapeComponents.Num()!=64)return Fail(TEXT("QA/world/tag/component guard"));
 auto* Info=Terrain->GetLandscapeInfo();int32 X1,Y1,X2,Y2;
 if(!Info || !Info->GetLandscapeExtent(X1,Y1,X2,Y2) || X1!=0 || Y1!=0 || X2!=2040 || Y2!=2040)return Fail(TEXT("native extent"));
 const FString Folder=FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/HikeRoadBatter02");TArray<uint8> Before,After,Mask;
 if(!FFileHelper::LoadFileToArray(Before,*(Folder/TEXT("before.r16"))) || !FFileHelper::LoadFileToArray(After,*(Folder/TEXT("after.r16")))
  || !FFileHelper::LoadFileToArray(Mask,*(Folder/TEXT("mask.u8"))) || Before.Num()!=2041*2041*2 || After.Num()!=Before.Num() || Mask.Num()!=2041*2041)return Fail(TEXT("source byte counts"));
 const auto* Layer=Terrain->GetEditLayerConst(0);if(!Layer)return Fail(TEXT("missing base layer"));
 FScopedSetLandscapeEditingLayer Scope(Terrain,Layer->GetGuid());FLandscapeEditDataInterface Edit(Info);Edit.SetEditLayer(Layer->GetGuid());
 TArray<uint16> Current;Current.SetNumUninitialized(2041*2041);Edit.GetHeightDataFast(0,0,2040,2040,Current.GetData(),0);
 TArray<uint16> VisibleBefore;VisibleBefore.SetNumUninitialized(Current.Num());
 FLandscapeEditDataInterface Merged(Info,FGuid());Merged.GetHeightDataFast(0,0,2040,2040,VisibleBefore.GetData(),0);
 // The source is the composed physical surface, not the base edit layer. Keep
 // existing edit layers intact and apply the requested delta to the base only.
 // Retain the exact native baseline for diagnosing an upstream source mismatch.
 // This read occurs before any terrain mutation and does not save the map.
 TArray<uint8> NativeBytes;NativeBytes.SetNumUninitialized(Before.Num());
 for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X){const uint16 H=VisibleBefore[(2040-Y)*2041+X];const int32 B=(Y*2041+X)*2;NativeBytes[B]=H&255;NativeBytes[B+1]=H>>8;}
 const FString Baseline=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/HikeBatterMergedNativeBaseline.r16");
 if(!IFileManager::Get().FileExists(*Baseline))FFileHelper::SaveArrayToFile(NativeBytes,*Baseline);
 int32 Count=0,MinX=2040,MinY=2040,MaxX=0,MaxY=0;
 for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X)
 {
  const int32 S=(2040-Y)*2041+X,B=S*2;const int32 Old=int32(Before[B])|(int32(Before[B+1])<<8),New=int32(After[B])|(int32(After[B+1])<<8);
  if(Mask[S]>1 || (!Mask[S] && Old!=New) || New-Old>24*128 || Old-New>32*128)return Fail(TEXT("mask/max-delta guard"));
  if(!Mask[S])continue;
  // Fixed local envelope excludes the village, coast and mansion.
  if(X<1002 || X>1052 || 2040-Y<457 || 2040-Y>537)return Fail(TEXT("bounded envelope"));
  if(FMath::Abs(int32(VisibleBefore[Y*2041+X])-Old)>4){UE_LOG(LogTemp,Error,TEXT("Hike composed baseline x%d source_y%d native%u before%d after%d"),X,2040-Y,VisibleBefore[Y*2041+X],Old,New);return Fail(TEXT("exact composed native baseline mismatch; read retained export"));}
  const int32 Updated=int32(Current[Y*2041+X])+New-Old;if(Updated<0 || Updated>65535)return Fail(TEXT("base-layer delta range"));
  Current[Y*2041+X]=uint16(Updated);++Count;MinX=FMath::Min(MinX,X);MinY=FMath::Min(MinY,Y);MaxX=FMath::Max(MaxX,X);MaxY=FMath::Max(MaxY,Y);
 }
 if(Count!=1848)return Fail(TEXT("changed sample count"));
 Terrain->Modify();Edit.SetHeightData(MinX,MinY,MaxX,MaxY,Current.GetData()+MinY*2041+MinX,2041,true);Edit.Flush();
 Terrain->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_All,true);Terrain->ForceUpdateLayersContent();Terrain->RecreateCollisionComponents();Terrain->MarkPackageDirty();return true;
}

bool UIslandLandAuthoringLibrary::RemoveHikeDownhillNeedles(ALandscape* Terrain)
{
 auto Fail=[](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("Hike needle correction refused: %s"),Why);return false;};
 if(!IFileManager::Get().FileExists(*(FPaths::ProjectDir()/TEXT("qa-upgrade-authoring.json"))) || !Terrain || !Terrain->GetWorld() || Terrain->GetWorld()->WorldType!=EWorldType::Editor || !Terrain->ActorHasTag(TEXT("NeonCity.Tidewater.Island")) || Terrain->LandscapeComponents.Num()!=64)return Fail(TEXT("QA native terrain guard"));
 auto* Info=Terrain->GetLandscapeInfo();int32 X1,Y1,X2,Y2;if(!Info || !Info->GetLandscapeExtent(X1,Y1,X2,Y2) || X1!=0 || Y1!=0 || X2!=2040 || Y2!=2040)return Fail(TEXT("extent"));
 const FString Folder=FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/HikeShoulderNeedles01");TArray<uint8> Before,After,Mask;
 if(!FFileHelper::LoadFileToArray(Before,*(Folder/TEXT("before.r16"))) || !FFileHelper::LoadFileToArray(After,*(Folder/TEXT("after.r16"))) || !FFileHelper::LoadFileToArray(Mask,*(Folder/TEXT("mask.u8"))) || Before.Num()!=2041*2041*2 || After.Num()!=Before.Num() || Mask.Num()!=2041*2041)return Fail(TEXT("byte counts"));
 const auto* Layer=Terrain->GetEditLayerConst(0);if(!Layer)return Fail(TEXT("base layer"));
 FScopedSetLandscapeEditingLayer Scope(Terrain,Layer->GetGuid());FLandscapeEditDataInterface Edit(Info);Edit.SetEditLayer(Layer->GetGuid());
 TArray<uint16> Base,Visible;Base.SetNumUninitialized(2041*2041);Visible.SetNumUninitialized(Base.Num());Edit.GetHeightDataFast(0,0,2040,2040,Base.GetData(),0);FLandscapeEditDataInterface Merged(Info,FGuid());Merged.GetHeightDataFast(0,0,2040,2040,Visible.GetData(),0);
 int32 Count=0,MinX=2040,MinY=2040,MaxX=0,MaxY=0;
 for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X)
 {
  const int32 S=(2040-Y)*2041+X,B=S*2,Old=Before[B]|(Before[B+1]<<8),New=After[B]|(After[B+1]<<8);
  if(Mask[S]>1 || (!Mask[S] && Old!=New) || New>Old || Old-New>4*128)return Fail(TEXT("outside/delta mask"));
  if(FMath::Abs(int32(Visible[Y*2041+X])-Old)>4){UE_LOG(LogTemp,Error,TEXT("Hike needle baseline differs x%d source_y%d current%u expected%d"),X,2040-Y,Visible[Y*2041+X],Old);return Fail(TEXT("full composed native baseline"));}
  if(!Mask[S])continue;
  if(X<1024 || X>1045 || 2040-Y<471 || 2040-Y>535)return Fail(TEXT("local envelope"));
  const int32 Updated=int32(Base[Y*2041+X])+New-Old;if(Updated<0 || Updated>65535)return Fail(TEXT("height range"));Base[Y*2041+X]=uint16(Updated);++Count;MinX=FMath::Min(MinX,X);MinY=FMath::Min(MinY,Y);MaxX=FMath::Max(MaxX,X);MaxY=FMath::Max(MaxY,Y);
 }
 if(Count!=36)return Fail(TEXT("36 actual teeth samples"));
 Terrain->Modify();Edit.SetHeightData(MinX,MinY,MaxX,MaxY,Base.GetData()+MinY*2041+MinX,2041,true);Edit.Flush();Terrain->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_All,true);Terrain->ForceUpdateLayersContent();Terrain->RecreateCollisionComponents();Terrain->MarkPackageDirty();
 TArray<uint16> Result;Result.SetNumUninitialized(Visible.Num());Merged.GetHeightDataFast(0,0,2040,2040,Result.GetData(),0);
 for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X)
 {const int32 S=(2040-Y)*2041+X,B=S*2,Expected=After[B]|(After[B+1]<<8);if(FMath::Abs(int32(Result[Y*2041+X])-Expected)>4 || (!Mask[S] && Result[Y*2041+X]!=Visible[Y*2041+X]))return Fail(TEXT("composed final/outside conservation"));}
 return true;
}

bool UIslandLandAuthoringLibrary::ExportTheOneLandscapeBaseline(ALandscape* Terrain)
{
 if(!Terrain || !Terrain->ActorHasTag(TEXT("NeonCity.Tidewater.Island")) || Terrain->LandscapeComponents.Num()!=64)return false;
 auto* Info=Terrain->GetLandscapeInfo();int32 X1,Y1,X2,Y2;
 if(!Info || !Info->GetLandscapeExtent(X1,Y1,X2,Y2) || X1!=0 || Y1!=0 || X2!=2040 || Y2!=2040)return false;
 const auto* Layer=Terrain->GetEditLayerConst(0);if(!Layer)return false;
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/TheOneLayer449");
 if(IFileManager::Get().DirectoryExists(*Folder))return false;
 TArray<uint16> Base,Visible;Base.SetNumUninitialized(2041*2041);Visible.SetNumUninitialized(Base.Num());
 {
  FScopedSetLandscapeEditingLayer Scope(Terrain,Layer->GetGuid());
  FLandscapeEditDataInterface Edit(Info,false);Edit.SetShouldDirtyPackage(false);Edit.SetEditLayer(Layer->GetGuid());
  Edit.GetHeightDataFast(0,0,2040,2040,Base.GetData(),0);
 }
 FLandscapeEditDataInterface Merged(Info,FGuid(),false);Merged.SetShouldDirtyPackage(false);Merged.GetHeightDataFast(0,0,2040,2040,Visible.GetData(),0);
 auto Bytes=[](const TArray<uint16>& Values)
 {
  TArray<uint8> Data;Data.SetNumUninitialized(2041*2041*2);
  for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X)
  {
   const uint16 V=Values[(2040-Y)*2041+X];const int32 B=(Y*2041+X)*2;
   Data[B]=V&255;Data[B+1]=V>>8;
  }
  return Data;
 };
 if(!IFileManager::Get().MakeDirectory(*Folder,true))return false;
 return FFileHelper::SaveArrayToFile(Bytes(Base),*(Folder/TEXT("base-layer.r16")))
     && FFileHelper::SaveArrayToFile(Bytes(Visible),*(Folder/TEXT("composed.r16")));
}

bool UIslandLandAuthoringLibrary::ApplyTheOneLandscapeCandidate(ALandscape* Terrain,FName Revision)
{
 auto Fail=[](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("The One sculpt refused: %s"),Why);return false;};
 const bool ShoulderRepair=Revision==TEXT("SummitShoulders790");
 const bool Shoulders=ShoulderRepair || Revision==TEXT("SummitShoulders788");
 // The root development revision has its own explicit command-line opt-in
 // and exact whole-grid guards. Older QA publication gates remain intact.
 const bool Authorized=Shoulders?FParse::Param(FCommandLine::Get(),TEXT("NeonCityEstateDevelopmentAuthoring")):
  IFileManager::Get().FileExists(*(FPaths::ProjectDir()/TEXT("qa-upgrade-authoring.json")));
 if(!Authorized
  || !Terrain || !Terrain->GetWorld() || Terrain->GetWorld()->WorldType!=EWorldType::Editor
  || !Terrain->ActorHasTag(TEXT("NeonCity.Tidewater.Island")) || Terrain->LandscapeComponents.Num()!=64)return Fail(TEXT("QA/world/landscape guard"));
 auto* Info=Terrain->GetLandscapeInfo();int32 X1,Y1,X2,Y2;
 if(!Info || !Info->GetLandscapeExtent(X1,Y1,X2,Y2) || X1!=0 || Y1!=0 || X2!=2040 || Y2!=2040)return Fail(TEXT("extent"));
 const auto* Layer=Terrain->GetEditLayerConst(0);if(!Layer)return Fail(TEXT("missing base layer"));
 const bool Refinement=Revision==TEXT("Hillside522");
 const bool Facilities=Revision==TEXT("RoomTerrain634");
 if(!Revision.IsNone() && !Refinement && !Facilities && !Shoulders)return Fail(TEXT("unknown estate candidate revision"));
 const FString Folder=FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade")/(ShoulderRepair?TEXT("SummitShoulders790"):Shoulders?TEXT("SummitShoulders788"):Facilities?TEXT("TheOne634dRoomTerrain"):Refinement?TEXT("TheOne522Hillside"):TEXT("TheOne472Arrival"));
 if(!Terrain->GetActorLocation().Equals(FVector(-102350,-102350,12800),.01)
  || !Terrain->GetActorScale3D().Equals(FVector(100.343137254902,100.343137254902,100),.00001)
  || !Terrain->GetActorRotation().IsNearlyZero(.00001))return Fail(TEXT("baseline landscape transform changed"));
 TArray<uint8> Before,After,Mask;
 if(!FFileHelper::LoadFileToArray(Before,*(Folder/TEXT("before.r16"))) || !FFileHelper::LoadFileToArray(After,*(Folder/TEXT("after.r16")))
  || !FFileHelper::LoadFileToArray(Mask,*(Folder/TEXT("mask.u8"))) || Before.Num()!=2041*2041*2 || After.Num()!=Before.Num() || Mask.Num()!=2041*2041)return Fail(TEXT("source sizes"));
 if(!FSHA1::HashBuffer(Before.GetData(),Before.Num()).ToString().Equals(ShoulderRepair?TEXT("FBCF0585386AF9A5DB6EE44524539B5EF243D96F"):Shoulders?TEXT("2CD2ED1D8D6C08E7D73CF42B255BEC9079EECD3B"):Facilities?TEXT("19046D565DA767B92D35815E0A7446FE4DF348CF"):Refinement?TEXT("6806853EDB58787E85011435DDEA30046C73D6F1"):TEXT("EA34B94F47F3FECE736051BDE95991DA15FAE33E"),ESearchCase::IgnoreCase)
  || !FSHA1::HashBuffer(After.GetData(),After.Num()).ToString().Equals(ShoulderRepair?TEXT("0E91E9B935D1EB976FF7FE525F1E475A249DAAC9"):Shoulders?TEXT("FBCF0585386AF9A5DB6EE44524539B5EF243D96F"):Facilities?TEXT("2CD2ED1D8D6C08E7D73CF42B255BEC9079EECD3B"):Refinement?TEXT("19046D565DA767B92D35815E0A7446FE4DF348CF"):TEXT("6806853EDB58787E85011435DDEA30046C73D6F1"),ESearchCase::IgnoreCase)
  || !FSHA1::HashBuffer(Mask.GetData(),Mask.Num()).ToString().Equals(ShoulderRepair?TEXT("DF2041AC19F44BF54BD9621A9BD288B2F5113ACF"):Shoulders?TEXT("56ECCBC22B1E6E6234BD3102BFC8793D19B5E0BF"):Facilities?TEXT("CBFB659D82E002F2F5F56167A5FA5C2DA140BB4B"):Refinement?TEXT("4719920722095E8319581226CFA6CDDD0CC1086D"):TEXT("58DA26301F3A56331426C01A275CFC012A8F2199"),ESearchCase::IgnoreCase))return Fail(TEXT("specific reviewed candidate bytes changed"));
 TArray<uint8> Original;
 if(ShoulderRepair && (!FFileHelper::LoadFileToArray(Original,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/TheOne634dRoomTerrain/after.r16")))
  || Original.Num()!=Before.Num() || !FSHA1::HashBuffer(Original.GetData(),Original.Num()).ToString().Equals(TEXT("2CD2ED1D8D6C08E7D73CF42B255BEC9079EECD3B"),ESearchCase::IgnoreCase)))return Fail(TEXT("original island repair floor guard"));
 TArray<uint16> Current,Composed;Current.SetNumUninitialized(2041*2041);Composed.SetNumUninitialized(Current.Num());
 {
  FScopedSetLandscapeEditingLayer Scope(Terrain,Layer->GetGuid());
  FLandscapeEditDataInterface Read(Info,false);Read.SetShouldDirtyPackage(false);Read.SetEditLayer(Layer->GetGuid());
  Read.GetHeightDataFast(0,0,2040,2040,Current.GetData(),0);
 }
 {
  FLandscapeEditDataInterface Read(Info,FGuid(),false);Read.SetShouldDirtyPackage(false);
  Read.GetHeightDataFast(0,0,2040,2040,Composed.GetData(),0);
 }
 int32 Count=0,MinX=2040,MinY=2040,MaxX=0,MaxY=0;
 for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X)
 {
  const int32 S=(2040-Y)*2041+X,B=S*2,I=Y*2041+X;
  const uint16 Old=uint16(Before[B])|(uint16(Before[B+1])<<8),Next=uint16(After[B])|(uint16(After[B+1])<<8);
  if(ShoulderRepair && Next<(uint16(Original[B])|(uint16(Original[B+1])<<8)))return Fail(TEXT("repair must not cut original island"));
  if(Current[I]!=Old || Composed[I]!=Old)return Fail(TEXT("exact base/composed baseline changed"));
  if(Mask[S]>1 || bool(Mask[S])!=(Old!=Next))return Fail(TEXT("mask does not exactly match changed bytes"));
  if(Old==Next)continue;
  const double Xm=X*2047.0/2040-1023.5,Ym=(2040-Y)*2047.0/2040-1023.5;
  if(Xm<(Shoulders?-320:-265) || Xm>(Shoulders?360:315) || Ym<(Shoulders?-840:-780) || Ym>(Shoulders?-210:-260)
   || FMath::Abs(int32(Next)-int32(Old))>(ShoulderRepair?110:Shoulders?100:Facilities?70:Refinement?65:240)*128 || (Shoulders && !ShoulderRepair && Next<Old))return Fail(TEXT("scoped bounds/delta"));
  ++Count;MinX=FMath::Min(MinX,X);MinY=FMath::Min(MinY,Y);MaxX=FMath::Max(MaxX,X);MaxY=FMath::Max(MaxY,Y);
 }
 if(Count!=(ShoulderRepair?67524:Shoulders?99485:Facilities?13666:Refinement?270121:150145))return Fail(TEXT("candidate changed count"));
 // The complete read and every invariant above finish before Modify/SetHeight.
 for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X)
 {
  const int32 S=(2040-Y)*2041+X,B=S*2;
  if(Mask[S])Current[Y*2041+X]=uint16(After[B])|(uint16(After[B+1])<<8);
 }
 Terrain->Modify();
 {
  FScopedSetLandscapeEditingLayer Scope(Terrain,Layer->GetGuid());FLandscapeEditDataInterface Write(Info);Write.SetEditLayer(Layer->GetGuid());
  Write.SetHeightData(MinX,MinY,MaxX,MaxY,Current.GetData()+MinY*2041+MinX,2041,true);Write.Flush();
 }
 Terrain->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_All,true);
 Terrain->ForceUpdateLayersContent();Terrain->RecreateCollisionComponents();Terrain->MarkPackageDirty();
 {
  FScopedSetLandscapeEditingLayer Scope(Terrain,Layer->GetGuid());
  FLandscapeEditDataInterface Read(Info,false);Read.SetShouldDirtyPackage(false);Read.SetEditLayer(Layer->GetGuid());
  Read.GetHeightDataFast(0,0,2040,2040,Current.GetData(),0);
 }
 {
  FLandscapeEditDataInterface Read(Info,FGuid(),false);Read.SetShouldDirtyPackage(false);
  Read.GetHeightDataFast(0,0,2040,2040,Composed.GetData(),0);
 }
 for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X)
 {
  const int32 B=((2040-Y)*2041+X)*2,I=Y*2041+X;const uint16 Expected=uint16(After[B])|(uint16(After[B+1])<<8);
  if(Current[I]!=Expected || Composed[I]!=Expected)return Fail(TEXT("post-write whole-grid base/composed mismatch; do not save this map"));
 }
 UE_LOG(LogTemp,Display,TEXT("THE_ONE_NATIVE_HEIGHTFIELD_EXACT %d changed; all untouched bytes preserved"),Count);return true;
}

bool UIslandLandAuthoringLibrary::BindGrassHeightDerivative(UMaterial* Material,UTexture2D* Height)
{
 if(!Material || !Height)return false;
 const bool Land=Material->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/IslandLandPass1/")) && Height->GetName()==TEXT("T_LandGrassHeights");
 const bool Summit=Material->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/")) && (Height->GetName()==TEXT("T_SummitGrassHeights") || Height->GetName()==TEXT("T_HikeGrassHeights") || Height->GetName()==TEXT("T_TheOneGrassHeights472") || Height->GetName()==TEXT("T_TheOneGrassHeights522") || Height->GetName()==TEXT("T_TheOneGrassHeights634") || Height->GetName()==TEXT("T_TheOneGrassHeights788") || Height->GetName()==TEXT("T_TheOneGrassHeights790"));
 if(!Land && !Summit)return false;
 int32 Changed=0;
 for(auto Expression:Material->GetExpressionCollection().Expressions)
  if(auto* Texture=Cast<UMaterialExpressionTextureObject>(Expression.Get());Texture && Texture->Texture && (Texture->Texture->GetName()==TEXT("T_NCTW_GrassHeights") || (Summit && (Texture->Texture->GetName()==TEXT("T_LandGrassHeights") || Texture->Texture->GetName()==TEXT("T_SummitGrassHeights") || Texture->Texture->GetName()==TEXT("T_HikeGrassHeights") || Texture->Texture->GetName()==TEXT("T_TheOneGrassHeights472") || Texture->Texture->GetName()==TEXT("T_TheOneGrassHeights522") || Texture->Texture->GetName()==TEXT("T_TheOneGrassHeights634") || Texture->Texture->GetName()==TEXT("T_TheOneGrassHeights788")))))
  {Texture->Texture=Height;++Changed;}
 if(Changed!=1)return false;
 Material->PostEditChange();Material->MarkPackageDirty();return true;
}
