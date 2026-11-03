#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Vehicles/MainlandVehicleRecovery.h"
#include "Vehicles/VehicleData.h"
#include "World/NeonCityMainlandSite.h"
#include "Property/PropertySubsystem.h"
#include "Property/PropertyComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

namespace DefaultParking921
{
struct FFixture
{
 TStrongObjectPtr<UGameInstance> GI{NewObject<UGameInstance>(GEngine)};
 UWorld* World=nullptr;
 FFixture(){GI->InitializeStandalone(FName(*FString::Printf(TEXT("C921DefaultParking_%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits))));World=GI->GetWorld();}
 ~FFixture(){GI->Shutdown();if(World){GEngine->DestroyWorldContext(World);World->DestroyWorld(false);}}
};
AActor* Block(UWorld& World,UStaticMesh* Cube,FVector Point,FVector Size,ECollisionChannel Object=ECC_WorldStatic)
{
 auto* A=World.SpawnActor<AActor>();if(!A)return nullptr;
 auto* M=NewObject<UStaticMeshComponent>(A);A->SetRootComponent(M);A->AddInstanceComponent(M);
 M->SetStaticMesh(Cube);M->SetCollisionProfileName(TEXT("BlockAll"));M->SetCollisionObjectType(Object);
 M->SetWorldScale3D(Size/100);M->SetWorldLocation(Point);M->RegisterComponent();return A;
}
const TCHAR* IDs[]={TEXT("0889167F430338D9839BAEB8F5500BB3"),TEXT("6A26E40E49A5ABC2B2DD5E98B399755E"),
 TEXT("66CBA46E4EFEE00CBF50C299AA9B4A0B"),TEXT("FE1019924247593F4BFD20A30519AE6C"),
 TEXT("BDBA472399505C57AD706630958711BA"),TEXT("8D521A134E5857A2BA5DEF01745C71C8"),TEXT("65A15939583852E088263B9A8A474E12")};
const TCHAR* Profiles[]={TEXT("DA_HeroKestrel"),TEXT("DA_HeroKestrel"),TEXT("DA_HeroNacre"),TEXT("DA_HeroNacre"),
 TEXT("DA_HeroKestrel"),TEXT("DA_HeroNacre"),TEXT("DA_HeroKestrel")};
FNeonCityVehicleSaveData Saved(int32 Index)
{
 FNeonCityVehicleSaveData D;FGuid::ParseExact(IDs[Index],EGuidFormats::Digits,D.VehicleID);
 D.DataAssetID=Profiles[Index];D.VehicleType=uint8(ENeonCityVehicleType::SportsCar);
 D.Transform=FTransform(FRotator(0,-90,0),FVector(12460.+340.*Index,4150.,90.));
 D.Condition=.46f;D.RoofProgress=.62f;D.RoofTargetOpen=true;D.IslandFleetMigrationVersion=2;return D;
}
FNeonCityVehicleSaveData Authored(int32 Index)
{
 auto D=Saved(Index);D.Transform=FTransform(FRotator(0,-90,0),FVector(12340.+360.*Index,4150.,65.));return D;
}
bool SameNonPose(const FNeonCityVehicleSaveData& A,const FNeonCityVehicleSaveData& B)
{auto Copy=A;Copy.Transform=B.Transform;return FNeonCityVehicleSaveData::StaticStruct()->CompareScriptStruct(&Copy,&B,0);}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefaultParkingMigration921,"NeonCity.Phase205c.MainlandC921.DefaultParkingMigration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefaultParkingMigration921::RunTest(const FString&)
{
 using namespace DefaultParking921;using namespace NeonCityVehiclePersistence;
 FFixture F;if(!TestNotNull(TEXT("Isolated native physics/registry world"),F.World))return false;
 auto* Site=F.World->SpawnActor<ANeonCityMainlandSite>();auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 if(!TestNotNull(TEXT("Explicit mainland adoption site"),Site) || !TestNotNull(TEXT("Native collision fixture mesh"),Cube))return false;
 Site->bRecoverUnsupportedMainlandVehicles=true;Site->Tags.Add(TEXT("NeonCity.MainlandC920"));
 auto* Floor=Block(*F.World,Cube,FVector(13500,4150,30),FVector(4000,1200,20));
 if(!TestNotNull(TEXT("Real complete native parking support"),Floor))return false;
 for(int32 I=0;I<7;++I)for(bool Owned:{false,true})
 {
  const auto Before=Saved(I);auto D=Before;D.Owned=Owned;const auto Original=D;const auto Target=Authored(I);FString Diagnostic;
  TestEqual(TEXT("Only the six actually shifted released defaults follow their native bays"),MigrateDefaultMainlandParking(F.World,D,&Target,&Diagnostic),I!=6);
  TestTrue(TEXT("Whole native pose, including measured chassis height, comes from the sealed authored default"),D.Transform.Equals(I!=6?Target.Transform:Original.Transform,.001));
  TestTrue(TEXT("Every health/roof/ownership/identity/slot/schema value is preserved"),SameNonPose(D,Original));
  TestFalse(TEXT("A current authored pose is idempotent"),MigrateDefaultMainlandParking(F.World,D,&Target));
 }
 auto Target=Authored(0);const auto Original=Saved(0);
 const auto Reject=[&](const TCHAR* Label,FNeonCityVehicleSaveData D,const FNeonCityVehicleSaveData* Default)
 {
  const auto Before=D;TestFalse(Label,MigrateDefaultMainlandParking(F.World,D,Default));
  TestTrue(TEXT("Rejected default correction preserves the entire original record"),FNeonCityVehicleSaveData::StaticStruct()->CompareScriptStruct(&D,&Before,0));
 };
 auto D=Original;D.Transform.AddToTranslation(FVector(2.01,0,0));Reject(TEXT("Even a small player movement beyond 2cm is preserved"),D,&Target);
 D=Original;D.Transform.SetRotation(FRotator(0,-89.49,0).Quaternion());Reject(TEXT("A custom parking angle is preserved"),D,&Target);
 D=Original;D.Transform.SetLocation(NeonCityCoastalLayout::FromLegacyIsland(FVector(60000,-153000,750)));Reject(TEXT("A mainland car driven onto the island stays there"),D,&Target);
 for(const TCHAR* Text:{TEXT("5FF7803645833C8D593E77A50B006245"),TEXT("7F242340469FEA4D1A7D21A5D5DED892"),TEXT("EA3CB5934EC94A1987F7A19A34C2A1D8")})
 {D=Original;FGuid::ParseExact(Text,EGuidFormats::Digits,D.VehicleID);Reject(TEXT("Every canonical island identity is excluded even at a matching mainland coordinate"),D,&Target);}
 D=Original;D.VehicleID=FGuid::NewGuid();Reject(TEXT("An unlisted identity is excluded"),D,&Target);
 D=Original;D.DataAssetID=TEXT("DA_HeroAster");Reject(TEXT("Profile normalization must produce the actual current selected model first"),D,&Target);
 D=Original;D.LegacyFleetStored=true;Reject(TEXT("Owned-storage collectibles are never deployed or repositioned"),D,&Target);
 D=Original;D.Condition=0;Reject(TEXT("A destroyed default wreck is preserved"),D,&Target);
 Reject(TEXT("Missing live sealed authored record fails closed"),Original,nullptr);
 auto Wrong=Target;Wrong.VehicleID=FGuid::NewGuid();Reject(TEXT("Another car's default cannot move this identity"),Original,&Wrong);
 Wrong=Target;Wrong.Transform.AddToTranslation(FVector(4,0,0));Reject(TEXT("A different native layout is not this released correction"),Original,&Wrong);
 Wrong=Target;Wrong.Transform.SetLocation(Original.Transform.GetLocation());Reject(TEXT("A C920 world does not opt into C921 placements"),Original,&Wrong);
 Site->bRecoverUnsupportedMainlandVehicles=false;Reject(TEXT("Explicit site opt-out remains authoritative"),Original,&Target);Site->bRecoverUnsupportedMainlandVehicles=true;

 // The real property registry supplies the authored new slot; no synthetic
 // optional slot pointer can bypass the production matching-link guard.
 auto* Properties=F.GI->GetSubsystem<UPropertySubsystem>();auto* Owner=F.World->SpawnActor<AActor>();
 if(!TestNotNull(TEXT("Production property registry"),Properties) || !TestNotNull(TEXT("Native property owner"),Owner))return false;
 auto* Root=NewObject<USceneComponent>(Owner);Owner->SetRootComponent(Root);Owner->AddInstanceComponent(Root);Root->RegisterComponent();
 auto* Property=NewObject<UPropertyComponent>(Owner);Owner->AddInstanceComponent(Property);
 auto* Data=NewObject<UNeonCityPropertyData>(F.GI.Get());Data->PropertyID=TEXT("Property.Mainland.RivieraEstate916");Data->DisplayName=FText::FromString(TEXT("C921 native parking policy fixture"));Data->GarageSlotCount=8;
 Property->Data=Data;for(int32 I=0;I<8;++I)Property->GarageSlotsLocal.Add(FVector(12340.+360.*I,4150.,40.));Property->RegisterComponent();
 if(!TestTrue(TEXT("Actual eight-slot native property is registered"),Properties->RegisterLiveProperty(Property)))return false;
 auto Linked=Original;Linked.Owned=true;Linked.StoredPropertyID=Data->PropertyID;Linked.GarageSlot=0;const auto LinkedBefore=Linked;
 TestTrue(TEXT("An exactly untouched old bay parked car follows its matching native property slot"),MigrateDefaultMainlandParking(F.World,Linked,&Target));
 TestTrue(TEXT("Linked relocation preserves ownership, exact property/slot link, roof and condition"),SameNonPose(Linked,LinkedBefore) && Linked.Transform.Equals(Target.Transform,.001));
 D=LinkedBefore;D.GarageSlot=1;Reject(TEXT("A different original slot is retained"),D,&Target);
 D=LinkedBefore;D.Owned=false;Reject(TEXT("An unowned car cannot adopt a saved property association"),D,&Target);
 D=LinkedBefore;D.StoredPropertyID=TEXT("Property.Island.Mansion");Reject(TEXT("Another property link is preserved"),D,&Target);
 Property->GarageSlotsLocal[0].X+=10;Properties->RegisterLiveProperty(Property);Reject(TEXT("A mismatched actual native property slot cannot relocate a linked car"),LinkedBefore,&Target);
 Property->GarageSlotsLocal[0].X-=10;Properties->RegisterLiveProperty(Property);

 auto CustomChoice=Target;CustomChoice.VehicleID=FGuid::NewGuid();CustomChoice.Owned=true;
 const TArray<FNeonCityVehicleSaveData> SavedChoices={Original,CustomChoice};FString OccupiedDiagnostic;D=Original;
 TestFalse(TEXT("A later-applied saved custom car blocks the untouched default correction"),MigrateDefaultMainlandParking(F.World,D,&Target,&OccupiedDiagnostic,SavedChoices));
 TestTrue(TEXT("Saved destination occupancy is diagnosed without changing either saved pose"),!OccupiedDiagnostic.IsEmpty() && D.Transform.Equals(Original.Transform)
  && SavedChoices[1].Transform.Equals(CustomChoice.Transform));
 TArray<FNeonCityVehicleSaveData> AllOldDefaults;for(int32 I=0;I<7;++I)AllOldDefaults.Add(Saved(I));D=Original;
 TestTrue(TEXT("Adjacent released defaults do not falsely block the legitimate correction"),MigrateDefaultMainlandParking(F.World,D,&Target,nullptr,AllOldDefaults));

 auto* Wall=Block(*F.World,Cube,Target.Transform.GetLocation()+FVector(0,0,45),FVector(40,200,80));
 if(!TestNotNull(TEXT("Actual blocking destination wall"),Wall))return false;
 FString Diagnostic;D=LinkedBefore;
 TestFalse(TEXT("A real blocked new bay fails closed"),MigrateDefaultMainlandParking(F.World,D,&Target,&Diagnostic));
 TestTrue(TEXT("Blocked linked correction records a diagnostic and retains the exact original link/pose"),!Diagnostic.IsEmpty() && D.Transform.Equals(LinkedBefore.Transform) && SameNonPose(D,LinkedBefore));
 Wall->SetActorEnableCollision(false);
 auto* Dynamic=Block(*F.World,Cube,Target.Transform.GetLocation()+FVector(0,0,25),FVector(40,80,40),ECC_Vehicle);
 if(!TestNotNull(TEXT("Real current dynamic vehicle-channel collider"),Dynamic))return false;
 Reject(TEXT("Another current collider at the destination prevents correction"),Original,&Target);Dynamic->SetActorEnableCollision(false);
 Floor->SetActorEnableCollision(false);D=Original;Diagnostic.Reset();
 TestFalse(TEXT("No static destination floor prevents correction"),MigrateDefaultMainlandParking(F.World,D,&Target,&Diagnostic));
 TestTrue(TEXT("Missing support is explicitly diagnosed"),!Diagnostic.IsEmpty());
 AddInfo(TEXT("Value-only old-save correction against real isolated native support/collision/property registry. Seven audited IDs, six shifted centres; custom poses, all island identities, mismatched/blocked linked bays fail closed. No map load, save read/write, player slot, physics tick or actor pose migration."));
 return !HasAnyErrors();
}
#endif
