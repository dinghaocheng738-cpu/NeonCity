#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/StaticMeshComponent.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/NeonCityPrototypeVehicle.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshSocket.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/FixedWing/NeonCityFixedWingAircraft.h"
#include "Combat/Weapons/NeonCityWeaponCatalog.h"
#include "Combat/Weapons/NeonCityPistol.h"

namespace
{
bool CheckArt(FAutomationTestBase& T,const TCHAR* Category,TArray<const TCHAR*> Names,float MinimumSize,float MaximumSize)
{
 for(const auto* Name:Names)
 {
  auto* M=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/FullArt/%s/SM_NC205B_%s.SM_NC205B_%s"),Category,Name,Name));
  if(!T.TestNotNull(Name,M)) continue;
  T.TestEqual(TEXT("Three distance LODs"),M->GetNumLODs(),3);
  T.TestTrue(TEXT("No added gameplay collision"),M->GetBodySetup() && M->GetBodySetup()->AggGeom.GetElementCount()==0);
  const double Size=(M->GetBounds().BoxExtent*2).GetMax();
  T.TestTrue(FString(Name)+TEXT(" centimetre-scale dimensions"),Size>=MinimumSize && Size<=MaximumSize);
  T.TestTrue(TEXT("Material slot budget"),M->GetStaticMaterials().Num()<=10);
  for(int32 I=0;I<M->GetStaticMaterials().Num();++I) T.TestNotNull(TEXT("All materials resolve"),M->GetMaterial(I));
 }
 return true;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullArtVehicle,"NeonCity.FullArt.VehicleAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFullArtVehicle::RunTest(const FString&)
{
 CheckArt(*this,TEXT("Vehicles"),{TEXT("Sedan"),TEXT("Sports"),TEXT("SUV"),TEXT("Pickup"),TEXT("Police")},450,510);
 CheckArt(*this,TEXT("Vehicles"),{TEXT("WheelTouring"),TEXT("WheelSport"),TEXT("WheelUtility")},62,67);
 auto* M=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/FullArt/Vehicles/SM_NC205B_Sedan.SM_NC205B_Sedan"));
 if(M) for(FName S:{TEXT("Headlight.Left"),TEXT("TailLight.Right"),TEXT("Door.Left")}) TestNotNull(TEXT("Light and door sockets"),M->FindSocket(S));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullArtMarine,"NeonCity.FullArt.MarineAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFullArtMarine::RunTest(const FString&) { return CheckArt(*this,TEXT("Marine"),{TEXT("Speedboat"),TEXT("Yacht")},490,1350); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullArtAircraft,"NeonCity.FullArt.AircraftAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFullArtAircraft::RunTest(const FString&)
{
 CheckArt(*this,TEXT("Aircraft"),{TEXT("Helicopter"),TEXT("FixedWing"),TEXT("HelicopterRotor")},600,900);
 auto* M=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/FullArt/Aircraft/SM_NC205B_Helicopter.SM_NC205B_Helicopter"));
 if(M) for(FName S:{TEXT("Nose"),TEXT("SideMount.Left"),TEXT("SideMount.Right"),TEXT("RotorVFX")}) TestNotNull(TEXT("Existing aircraft mount sockets"),M->FindSocket(S));
 if(M && M->FindSocket(TEXT("SideMount.Right"))) TestTrue(TEXT("Missile art socket matches released right mount"),M->FindSocket(TEXT("SideMount.Right"))->RelativeLocation.Equals(FVector(40,160,-20),.01));
 CheckArt(*this,TEXT("VehicleWeapons"),{TEXT("MountedGun"),TEXT("Cannon"),TEXT("RocketPod"),TEXT("MissileRail"),TEXT("GuidedMissile")},40,130);
 auto* Optics=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/VerticalSlice/AircraftLighting/SM_NC205C_AircraftSearchlight"));
 if(TestNotNull(TEXT("Reviewed aircraft optical head resolves"),Optics))
 {
  TestEqual(TEXT("Small light has three distance LODs"),Optics->GetNumLODs(),3);
  TestTrue(TEXT("Optical fitting has no added collision"),Optics->GetBodySetup() && Optics->GetBodySetup()->AggGeom.GetElementCount()==0);
  TestTrue(TEXT("Optical head fits existing bracket at human scale"),Optics->GetBounds().BoxExtent.GetMax()*2<28 && Optics->GetBounds().BoxExtent.X*2>19);
  const auto* Emitter=Optics->FindSocket(TEXT("LightEmission"));
  if(TestNotNull(TEXT("Real optical exit is authored"),Emitter))
   TestTrue(TEXT("Emitter is just beyond the front lens, not inside the hull"),Emitter->RelativeLocation.X>Optics->GetBounds().Origin.X+Optics->GetBounds().BoxExtent.X && Emitter->RelativeLocation.X<11);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullArtWeapon,"NeonCity.FullArt.WeaponAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFullArtWeapon::RunTest(const FString&)
{
 CheckArt(*this,TEXT("Weapons"),{TEXT("Pistol"),TEXT("SMG"),TEXT("Rifle"),TEXT("Shotgun")},30,115);
 const auto* Pistol=GetDefault<ANeonCityPistol>();
 TInlineComponentArray<UStaticMeshComponent*> PistolParts(Pistol);
 const UStaticMeshComponent* Visual=nullptr;
 for(const auto* Part:PistolParts) if(Part->GetFName()==TEXT("ArtVisual")) Visual=Part;
 const UStaticMesh* Model=Visual?Visual->GetStaticMesh().Get():nullptr;
 if(TestNotNull(TEXT("Pistol presentation retained"),Visual) && TestNotNull(TEXT("Reviewed pistol model resolves"),Model))
 {
  TestEqual(TEXT("Adapted CC0 pistol is the actual runtime mesh"),Model->GetFName(),FName(TEXT("SM_NC205C_Pistol")));
  TestTrue(TEXT("Pistol fits a human grip at a compact 21-23 cm length"),Model->GetBounds().BoxExtent.X*2>21 && Model->GetBounds().BoxExtent.X*2<23);
  TestEqual(TEXT("Reviewed pistol retains three LODs"),Model->GetNumLODs(),3);
  TestTrue(TEXT("Reviewed pistol stays within material budget"),Model->GetStaticMaterials().Num()<=6);
  TInlineComponentArray<USceneComponent*> SceneParts(Pistol);
  const USceneComponent* Muzzle=nullptr;
  for(const auto* Part:SceneParts) if(Part->GetFName()==TEXT("Muzzle")) Muzzle=Part;
  if(TestNotNull(TEXT("Logical muzzle retained"),Muzzle) && TestNotNull(TEXT("Authored muzzle retained"),Model->FindSocket(TEXT("Muzzle"))))
   TestTrue(TEXT("Default muzzle uses explicit local artwork coordinates"),Muzzle->GetRelativeLocation().Equals(Model->FindSocket(TEXT("Muzzle"))->RelativeLocation,.01));
 }
 const TCHAR* Names[]={TEXT("Pistol"),TEXT("SMG"),TEXT("Rifle"),TEXT("Shotgun")};
 for(int32 I=0;I<4;++I)
 {
  const auto Type=NeonCityWeapons::Resolve(NeonCityWeapons::Slot(I+1));
  TestNotNull(TEXT("Original stable weapon ID resolves"),Type.Get());
  const auto* D=NeonCityWeapons::Profile(NeonCityWeapons::Slot(I+1));
  TestTrue(TEXT("Existing balance data remains valid"),D && D->IsValidProfile());
  auto* M=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/FullArt/Weapons/SM_NC205B_%s.SM_NC205B_%s"),Names[I],Names[I]));
  if(M) { TestNotNull(TEXT("Grip reference"),M->FindSocket(TEXT("Grip"))); TestNotNull(TEXT("Muzzle reference"),M->FindSocket(TEXT("Muzzle"))); }
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullArtProps,"NeonCity.FullArt.EnvironmentAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFullArtProps::RunTest(const FString&)
{
 CheckArt(*this,TEXT("Props"),{TEXT("UtilityBox"),TEXT("Hydrant"),TEXT("ParkingMeter")},60,160);
 CheckArt(*this,TEXT("Beach"),{TEXT("BeachChair"),TEXT("Umbrella"),TEXT("LifeguardTower"),TEXT("Palm"),TEXT("Boardwalk3m")},100,600);
 for(const TCHAR* Side:{TEXT("West"),TEXT("East")})
 {
  auto* Shore=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/FullArt/Beach/SM_NC205B_Beach%s.SM_NC205B_Beach%s"),Side,Side));
  if(TestNotNull(TEXT("Shore skirt resolves"),Shore))
  {
   TestTrue(TEXT("Static shore floor matches its slope"),Shore->GetBodySetup() && Shore->GetBodySetup()->CollisionTraceFlag==CTF_UseComplexAsSimple);
   TestTrue(TEXT("Imported shore begins at the promenade on the south side"),FMath::IsNearlyEqual(Shore->GetBounds().GetBox().Max.Y,-1450.,1.));
   TestTrue(TEXT("Submerged slope extends below the returning swimming capsule"),Shore->GetBounds().GetBox().Min.Z< -390.);
  }
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullArtContracts,"NeonCity.FullArt.RuntimeVisualContracts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFullArtContracts::RunTest(const FString&)
{
 const AActor* Actors[]={GetDefault<ANeonCityStarterSedan>(),GetDefault<ANeonCityHelicopterPrototype>(),GetDefault<ANeonCityFixedWingAircraft>()};
 for(const auto* Actor:Actors)
 {
  TestNotNull(TEXT("Gameplay root retained"),Actor->GetRootComponent());
  int32 ArtCount=0;TInlineComponentArray<UStaticMeshComponent*> Parts(Actor);
  for(const auto* Part:Parts) if(Part->ComponentHasTag(TEXT("NeonCity.Visual205B")))
  {
   ++ArtCount;TestTrue(TEXT("Art resolves in actual native class"),Part->GetStaticMesh()!=nullptr);
   TestTrue(TEXT("Art contributes no second body"),Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
   TestFalse(TEXT("Art is not a dynamic floor"),Part->CanCharacterStepUpOn==ECB_Yes);
  }
  TestTrue(TEXT("Gameplay class actually uses new artwork"),ArtCount>0);
 }
 return true;
}
#endif
