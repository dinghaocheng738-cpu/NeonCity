#include "Adventure/IslandHeroDisplayLight.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

AIslandHeroDisplayLight::AIslandHeroDisplayLight()
{
 Fixture=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReviewedOpticalFixture"));SetRootComponent(Fixture);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/NeonCity/Art/VerticalSlice/AircraftLighting/SM_NC205C_AircraftSearchlight"));
 if(Mesh.Succeeded())Fixture->SetStaticMesh(Mesh.Object);Fixture->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GroundStand=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ManufacturedFloorStand"));GroundStand->SetupAttachment(Fixture);
 GroundStand->SetMobility(EComponentMobility::Movable);GroundStand->SetVisibility(false);
 GroundStand->SetCollisionEnabled(ECollisionEnabled::NoCollision);GroundStand->SetGenerateOverlapEvents(false);GroundStand->SetCanEverAffectNavigation(false);GroundStand->CanCharacterStepUpOn=ECB_No;
 const FString StandPath=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/McLaren720S321/BayStand411/SM_BaySpotStand");
 if(FPackageName::DoesPackageExist(StandPath))GroundStand->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*StandPath));
 Light=CreateDefaultSubobject<USpotLightComponent>(TEXT("BayWash"));Light->SetupAttachment(Fixture,TEXT("LightEmission"));
 Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetIntensity(500);Light->SetAttenuationRadius(1600);
 Light->SetInnerConeAngle(30);Light->SetOuterConeAngle(65);Light->SetSourceRadius(8);Light->SetUseTemperature(true);Light->SetTemperature(4300);
 MeteringBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("BayMeteringBounds"));MeteringBounds->SetupAttachment(Fixture);
 MeteringBounds->SetBoxExtent(FVector(1000,1000,650));MeteringBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 MeteringBounds->SetCollisionResponseToAllChannels(ECR_Ignore);MeteringBounds->SetGenerateOverlapEvents(false);
 Metering=CreateDefaultSubobject<UPostProcessComponent>(TEXT("BayMetering"));Metering->SetupAttachment(MeteringBounds);Metering->bUnbound=false;Metering->BlendRadius=350;Metering->Priority=10;
 // A lit bay should not inherit the dark-ocean exposure and clip its paint.
 // Extended-luminance mode interprets these as EV100. An EV0 floor lets the
 // dark ocean dominate metering beside a 500-lumen fixture, whitening paint
 // and seat upholstery. EV3 bounds this local lit bay without adding light.
 Metering->Settings.bOverride_AutoExposureMinBrightness=true;Metering->Settings.AutoExposureMinBrightness=3;
 Metering->Settings.bOverride_AutoExposureMaxBrightness=true;Metering->Settings.AutoExposureMaxBrightness=16;
}
void AIslandHeroDisplayLight::BeginPlay()
{
 Super::BeginPlay();
 // Earlier wall placement relied on broad gameplay contact boxes. A visible
 // complex mesh must actually support the optical head; otherwise install its
 // manufactured stand down to the native floor without moving the light/bay.
 FCollisionQueryParams MountQuery(SCENE_QUERY_STAT(HeroOpticalMount),true,this);FHitResult Wall,Floor;
 for(TActorIterator<APawn> It(GetWorld());It;++It)MountQuery.AddIgnoredActor(*It);
 const FVector Centre=Fixture->CalcBounds(Fixture->GetComponentTransform()).Origin;
 const bool WallHit=GetWorld()->LineTraceSingleByChannel(Wall,Centre,Centre-GetActorForwardVector()*45,ECC_Visibility,MountQuery);
 const auto* SupportMesh=WallHit?Cast<UStaticMeshComponent>(Wall.GetComponent()):nullptr;
 const bool VisibleWall=SupportMesh && SupportMesh->Mobility!=EComponentMobility::Movable && SupportMesh->GetStaticMesh() && SupportMesh->IsVisible() && !SupportMesh->bHiddenInGame
  && !SupportMesh->GetName().Contains(TEXT("Contact")) && FMath::Abs(Wall.ImpactNormal.Z)<.65f;
 if(!VisibleWall && GroundStand->GetStaticMesh() && GetWorld()->LineTraceSingleByChannel(Floor,Centre,Centre-FVector(0,0,250),ECC_Visibility,MountQuery) && Floor.ImpactNormal.Z>.7f
  && Floor.GetComponent() && Floor.GetComponent()->Mobility!=EComponentMobility::Movable && Floor.GetComponent()->IsVisible() && !Floor.GetComponent()->bHiddenInGame)
 {
  const float Top=Fixture->CalcBounds(Fixture->GetComponentTransform()).GetBox().Min.Z+1.f;
  const float Height=Top-Floor.ImpactPoint.Z;
  if(Height>10.f && Height<200.f)
  {
   GroundStand->SetWorldLocationAndRotation(FVector(Centre.X,Centre.Y,Floor.ImpactPoint.Z),FRotator::ZeroRotator);
   GroundStand->SetWorldScale3D(FVector(1,1,Height/100.f));GroundStand->SetVisibility(true);
   GroundStand->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);GroundStand->SetCollisionResponseToAllChannels(ECR_Block);
  }
 }
 if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())
 {
  // The supported fixtures are close to the parked cars. A modest night wash
  // preserves the paint and pavement detail instead of clipping both surfaces.
  FNeonCityLocalLightPolicy Policy;Policy.Intensity=500;Policy.DayMultiplier=0;Policy.ShadowDistance=2200;Policy.CullDistance=6500;Policy.Priority=1;
  Lighting->RegisterLight(Light,Policy);
  for(int32 I=0;I<Fixture->GetNumMaterials();++I)if(auto* Mat=Fixture->GetMaterial(I);Mat && Mat->GetName().Contains(TEXT("SearchlightLens")))
   if(auto* MID=Fixture->CreateAndSetMaterialInstanceDynamic(I))Lighting->RegisterNightMaterial(MID,TEXT("LightEnabled"));
 }
}
void AIslandHeroDisplayLight::EndPlay(EEndPlayReason::Type Reason)
{if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())Lighting->UnregisterLight(Light);Super::EndPlay(Reason);}
