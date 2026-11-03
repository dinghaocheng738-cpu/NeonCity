#include "Adventure/MarineEcologySubsystem.h"
#include "Adventure/OceanWildlifeCatalog.h"
#include "Adventure/MarineSchool.h"
#include "Adventure/OceanHabitat.h"
#include "Fishing/FishData.h"
#include "Engine/StaticMesh.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

TStatId UMarineEcologySubsystem::GetStatId() const
{RETURN_QUICK_DECLARE_CYCLE_STAT(UMarineEcologySubsystem,STATGROUP_Tickables);}
void UMarineEcologySubsystem::Deinitialize()
{
 Population.Reset();Depleted.Reset();ZoneRetryAfter.Reset();Catalog=nullptr;Hooks.Reset();Schools.Reset();Members.Reset();
 Super::Deinitialize();
}
void UMarineEcologySubsystem::Tick(float DeltaTime)
{
 auto* W=GetWorld();if(!W || !W->IsGameWorld() || W->GetNetMode()==NM_Client || W->GetTimeSeconds()<NextPopulationUpdate)return;
 NextPopulationUpdate=W->GetTimeSeconds()+.35;
 const auto* PC=W->GetFirstPlayerController();if(!PC || !PC->GetPawn())return;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 FWaterSurfaceSample Surface;
 if(!W->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Eye,Surface) || Eye.Z>Surface.SurfacePoint.Z+2000)return;
 if(!Catalog)
 {
  if(W->GetTimeSeconds()<CatalogRetryAt)return;CatalogRetryAt=W->GetTimeSeconds()+10;
  Catalog=LoadObject<UOceanWildlifeCatalog>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/Ecology/DA_OceanWildlifeCatalog"));
  if(!Catalog)return;
 }
 const float PopulationZoneSize=Catalog->ZoneSize;
 // A wide retirement band prevents the same animals popping as a zone edge
 // is crossed. Hooked/speared individuals remain resident until resolved.
 for(int32 I=Population.Num()-1;I>=0;--I)
 {
  auto* S=Population[I].School.Get();
  if(!S){if(auto* H=Population[I].Habitat.Get())H->Destroy();Population.RemoveAtSwap(I);continue;}
  if(FVector::Dist2D(S->GetActorLocation(),Eye)>PopulationZoneSize*3.8f && !S->HasReservedCreatures())
  {
   const auto Removed=S->GetDepletedIndices();if(!Removed.IsEmpty())Depleted.FindOrAdd(Population[I].Key).Append(Removed);
   S->Destroy();if(auto* H=Population[I].Habitat.Get())H->Destroy();Population.RemoveAtSwap(I);
  }
 }
 int32 LocalCount=0;
 for(const auto& S:Schools)if(S.IsValid() && FVector::Dist2D(Eye,S->GetActorLocation())<PopulationZoneSize*2.5f)LocalCount+=S->LivingCount();
 if(Population.Num()>=Catalog->ActiveSchoolBudget || LocalCount>=Catalog->ActiveCreatureBudget)return;
 const FIntPoint Centre(FMath::FloorToInt(Eye.X/PopulationZoneSize),FMath::FloorToInt(Eye.Y/PopulationZoneSize));
 TArray<FIntVector> Candidates;
 for(int32 Y=-2;Y<=2;++Y)for(int32 X=-2;X<=2;++X)for(int32 Layer=0;Layer<2;++Layer)
 {
  const FIntVector Key(Centre.X+X,Centre.Y+Y,Layer);
  if(Population.ContainsByPredicate([&](const auto& Z){return Z.Key==Key;}))continue;
  if(const double* Retry=ZoneRetryAfter.Find(Key);Retry && W->GetTimeSeconds()<*Retry)continue;
  Candidates.Add(Key);
 }
 Candidates.Sort([&](const FIntVector& A,const FIntVector& B)
 {
  auto Score=[&](const FIntVector& K){return FVector::DistSquared2D(Eye,FVector((K.X+.5f)*PopulationZoneSize,(K.Y+.5f)*PopulationZoneSize,0))+K.Z*PopulationZoneSize*PopulationZoneSize*.3f;};
  return Score(A)<Score(B);
 });
 // At most one real school and 49 terrain samples per update. Failed land
 // candidates are cheap centre traces; there is no per-fish spawning tick.
 int32 Attempts=0;for(const auto& Key:Candidates){if(SpawnZone(Key,Eye))break;ZoneRetryAfter.Add(Key,W->GetTimeSeconds()+20);if(++Attempts>=5)break;}
 // Only nearby failed traces need retry bookkeeping. Capture depletion remains
 // session-persistent when a zone streams out and back in.
 for(auto It=ZoneRetryAfter.CreateIterator();It;++It)if(It.Value()<W->GetTimeSeconds())It.RemoveCurrent();
}
bool UMarineEcologySubsystem::SpawnZone(FIntVector Key,const FVector& Eye)
{
 auto* W=GetWorld();const float Size=Catalog->ZoneSize;
 const int32 Seed=int32(HashCombineFast(GetTypeHash(Key),205103u));FRandomStream Random(Seed);
 const FVector Centre((Key.X+.5f)*Size+Random.FRandRange(-.18f,.18f)*Size,(Key.Y+.5f)*Size+Random.FRandRange(-.18f,.18f)*Size,0);
 FWaterSurfaceSample Water;if(!W->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Centre,Water))return false;
 FHitResult Bottom;FCollisionQueryParams Query(SCENE_QUERY_STAT(OceanPopulation),false);
 const float Sea=Water.SurfacePoint.Z;
 if(!W->LineTraceSingleByObjectType(Bottom,FVector(Centre.X,Centre.Y,Sea-35),FVector(Centre.X,Centre.Y,Sea-20000),FCollisionObjectQueryParams(ECC_WorldStatic),Query))return false;
 const float Depth=(Sea-Bottom.ImpactPoint.Z)*.01f;if(Depth<1.5f || Depth>150)return false;
 // Preserve hand-authored focal reefs; the streamed ecology fills the world
 // beyond them instead of piling more fish into the old demonstration patch.
 for(const auto& S:Schools)if(S.IsValid() && !Population.ContainsByPredicate([&](const auto& Z){return Z.School==S;})
  && FVector::Dist2D(Centre,S->GetActorLocation())<S->HomeRadius+500)return false;
 // Broad ecological bands follow the terrain and stable world coordinates.
 // Reef shelves contain colonies; adjacent sand remains navigable open space.
 const bool Rocky=Bottom.ImpactNormal.Z<.94f || (Bottom.GetComponent() && Bottom.GetComponent()->Bounds.BoxExtent.Size2D()<2500)
  || FMath::PerlinNoise2D(FVector2D(Centre.X,Centre.Y)/18000.f)>.08f;
 float Total=0;TArray<float> Weights;
 for(const auto& P:Catalog->Profiles)
 {
  float Weight=P.Species && Depth>=P.SeabedDepthMetres.X && Depth<=P.SeabedDepthMetres.Y?P.Density:0;
  if(Key.Z==1 && P.bNearBottom)Weight=0;
  if(Key.Z==0 && !P.bNearBottom)Weight*=.38f;
  if(P.bReefAffinity)Weight*=Rocky?2.f:.23f;
  Weights.Add(Weight);Total+=Weight;
 }
 if(Total<=0)return false;
 float Roll=Random.FRand()*Total;int32 Index=INDEX_NONE;
 for(int32 I=0;I<Weights.Num();++I){Roll-=Weights[I];if(Weights[I]>0 && Roll<=0){Index=I;break;}}
 if(Index==INDEX_NONE)return false;const auto& Profile=Catalog->Profiles[Index];
 auto* Mesh=Profile.Species->Mesh.LoadSynchronous();if(!Mesh)return false;
 const float Z=Profile.bNearBottom?Bottom.ImpactPoint.Z+Profile.Clearance:
   FMath::Clamp(float(Eye.Z),float(Bottom.ImpactPoint.Z)+FMath::Max(180.f,Profile.Clearance),Sea-180);
 const FVector Origin(Centre.X,Centre.Y,Z);
 auto* School=W->SpawnActorDeferred<AMarineSchool>(AMarineSchool::StaticClass(),FTransform(Origin),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!School)return false;
 School->Species=Profile.Species;School->CreatureMesh=Mesh;School->Behaviour=Profile.Behaviour;School->Seed=Seed;School->bSideways=Profile.bSideways;
 School->HabitatID=FName(*FString::Printf(TEXT("OceanZone.%d.%d.%d"),Key.X,Key.Y,Key.Z));
 School->MeanSeaLevel=Sea;School->HomeRadius=Size*.39f;School->FloorExtent=Size*.57f;
 School->FloorResolution=7;School->FloorClearance=Profile.Clearance;School->SurfaceClearance=100;
 School->CruiseSpeed=Profile.Speed;School->SwimFrequency=Profile.Frequency;School->SwimAmplitude=Profile.Amplitude;
 School->Spacing=FMath::Clamp(float(Profile.Species->SizeCm.Y)*.9f,25.f,280.f);School->bCapturable=Profile.bCapturable;
 bool Valid=true;
 for(int32 Y=0;Y<7;++Y)for(int32 X=0;X<7;++X)
 {
  const FVector P=Origin+FVector((X/6.f*2-1)*School->FloorExtent,(Y/6.f*2-1)*School->FloorExtent,0);
  FHitResult Hit;
  if(!W->LineTraceSingleByObjectType(Hit,FVector(P.X,P.Y,Sea-35),FVector(P.X,P.Y,Sea-20000),FCollisionObjectQueryParams(ECC_WorldStatic),Query)){Valid=false;break;}
  School->FloorHeights.Add(Hit.ImpactPoint.Z-Origin.Z);
 }
 if(!Valid || School->FloorHeights.Num()!=49){School->Destroy();return false;}
 TArray<FOverlapResult> Objects;
 W->OverlapMultiByObjectType(Objects,Bottom.ImpactPoint+FVector(0,0,150),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeSphere(School->HomeRadius),Query);
 for(const auto& Object:Objects)if(const auto* C=Object.GetComponent())
 {
  const float Radius=FMath::Max(C->Bounds.BoxExtent.X,C->Bounds.BoxExtent.Y);
  if(Radius>30 && Radius<400 && School->Obstacles.Num()<24){const FVector P=C->Bounds.Origin-Origin;School->Obstacles.Add(FVector4(P,Radius));}
 }
 int32 Nearby=0;for(const auto& S:Schools)if(S.IsValid() && FVector::Dist2D(Eye,S->GetActorLocation())<Size*2.5f)Nearby+=S->LivingCount();
 for(int32 I=0;I<FMath::Min(Profile.GroupSize,Catalog->ActiveCreatureBudget-Nearby);++I)
 {
  const float A=Random.FRand()*2*PI,R=Random.FRandRange(.1f,.55f)*School->HomeRadius;
  FVector At(FMath::Cos(A)*R,FMath::Sin(A)*R,Random.FRandRange(-60,60));
  const float Floor=School->FloorAt(At)+Profile.Clearance;
  if(Floor>=Sea-Origin.Z-120)continue;
  At.Z=Profile.bNearBottom?Floor:FMath::Clamp(float(At.Z),Floor,Sea-float(Origin.Z)-120);
  School->Starts.Add(FTransform(FRotator(0,Random.FRandRange(-180,180),0),At,FVector(Random.FRandRange(.88f,1.08f))));
 }
 if(School->Starts.IsEmpty()){School->Destroy();return false;}
 // Bottom-layer patches share the school's lifetime and finite streaming
 // budget. Surface/mid-water schools never create a duplicate seabed patch.
 AOceanHabitat* Habitat=Key.Z==0?BuildZoneHabitat(School,Depth,Rocky,Random):nullptr;
 if(School->Starts.IsEmpty()){if(Habitat)Habitat->Destroy();School->Destroy();return false;}
 School->FinishSpawning(FTransform(Origin));
 if(const auto* Removed=Depleted.Find(Key))School->ApplyDepletedIndices(*Removed);
 Population.Add({Key,School,Habitat});return true;
}
AOceanHabitat* UMarineEcologySubsystem::BuildZoneHabitat(AMarineSchool* School,float Depth,bool Reef,FRandomStream& Random)
{
 if(!School || Catalog->ReefRocks.IsEmpty())return nullptr;
 auto* W=GetWorld();const FVector Origin=School->GetActorLocation();
 TArray<FOceanHabitatLayer> Layers;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(OceanSeabedColony),false,School);
 const auto Place=[&](UStaticMesh* Mesh,FVector2D XY,float Scale,bool Coral)
 {
  if(!Mesh)return false;
  FHitResult Hit;const FVector Ray(XY.X,XY.Y,School->MeanSeaLevel-35);
  if(!W->LineTraceSingleByObjectType(Hit,Ray,Ray-FVector(0,0,20000),FCollisionObjectQueryParams(ECC_WorldStatic),Query)
   || Hit.ImpactNormal.Z<(Coral?.94f:.82f))return false;
  const float WaterDepth=School->MeanSeaLevel-Hit.ImpactPoint.Z;
  const auto Bounds=Mesh->GetBounds();const float Height=Bounds.BoxExtent.Z*2*Scale;
  // solid colonies keep keel clearance: 2.5 m of water over them (a 63's 1.3 m draft and its pitch at speed), so a
  // shallow lake or bay never grows an invisible reef a boat runs onto
  if(WaterDepth<Height+250 || (Coral && (WaterDepth<220 || WaterDepth>2200)))return false;
  // Physical minimum sits slightly inside the real support plane, with a
  // restrained tilt. Imported asset origins are not assumed to be at the base.
  const FQuat Tilt=FQuat::FindBetweenNormals(FVector::UpVector,FMath::Lerp(FVector::UpVector,Hit.ImpactNormal,.35f).GetSafeNormal());
  const FQuat Rotation=Tilt*FQuat(FVector::UpVector,Random.FRandRange(-PI,PI));
  FVector At=Hit.ImpactPoint-Origin;
  At.Z-=(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale+(Coral?3.f:FMath::Min(Height*.12f,18.f));
  FOceanHabitatLayer* Layer=Layers.FindByPredicate([Mesh](const auto& L){return L.Mesh==Mesh;});
  if(!Layer){Layer=&Layers.AddDefaulted_GetRef();Layer->Mesh=Mesh;Layer->bSolid=true;}
  Layer->Instances.Add(FTransform(Rotation,At,FVector(Scale)));
  const FVector Centre=At+Rotation.RotateVector(Bounds.Origin*Scale);
  const float Radius=FMath::Max(Bounds.BoxExtent.X,Bounds.BoxExtent.Y)*Scale*.85f;
  School->Obstacles.Add(FVector4(Centre,Radius));return true;
 };
 const int32 Colonies=Reef && Depth<28?2:1;
 for(int32 Cluster=0;Cluster<Colonies;++Cluster)
 {
  const float Angle=Random.FRandRange(-PI,PI),Radius=Random.FRandRange(.4f,.73f)*School->HomeRadius;
  const FVector2D Centre(Origin.X+FMath::Cos(Angle)*Radius,Origin.Y+FMath::Sin(Angle)*Radius);
  const int32 Rocks=Reef?3:2;
  for(int32 I=0;I<Rocks;++I)
  {
   const float A=Random.FRandRange(-PI,PI),R=I?Random.FRandRange(135,270):0;
   Place(Catalog->ReefRocks[Random.RandRange(0,Catalog->ReefRocks.Num()-1)].LoadSynchronous(),
    Centre+FVector2D(FMath::Cos(A),FMath::Sin(A))*R,Random.FRandRange(.65f,1.15f),false);
  }
  if(Reef && Depth<22 && !Catalog->ReefCorals.IsEmpty())for(int32 I=0;I<5;++I)
  {
   const float A=I*2.399963f+Random.FRandRange(-.2f,.2f),R=Random.FRandRange(320,520);
   Place(Catalog->ReefCorals[Random.RandRange(0,Catalog->ReefCorals.Num()-1)].LoadSynchronous(),
    Centre+FVector2D(FMath::Cos(A),FMath::Sin(A))*R,Random.FRandRange(.48f,.9f),true);
  }
 }
 if(Layers.IsEmpty())return nullptr;
 // Reject initial animals inside the new solid colonies rather than allowing
 // their first visible frame to overlap rocks or coral branches.
 School->Starts.RemoveAll([&](const FTransform& T)
 {
  for(const auto& Obstacle:School->Obstacles)
   if(FVector::DistSquared(T.GetLocation(),FVector(Obstacle.X,Obstacle.Y,Obstacle.Z))<FMath::Square(Obstacle.W+School->Spacing*.3f))return true;
  return false;
 });
 if(School->Starts.IsEmpty())return nullptr;
 const FTransform At(Origin);
 auto* H=W->SpawnActorDeferred<AOceanHabitat>(AOceanHabitat::StaticClass(),At,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!H)return nullptr;H->bStreamed=true;H->HabitatLayers=MoveTemp(Layers);H->FinishSpawning(At);return H;
}
