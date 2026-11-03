#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Property/PropertyComponent.h"
#include "Property/PropertySubsystem.h"
#include "World/NeonCityMainlandSite.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandPropertyPresentation920,
 "NeonCity.Phase205c.MainlandC920.PropertyPresentationAdoption",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandPropertyPresentation920::RunTest(const FString&)
{
 auto* Canonical=LoadObject<UNeonCityPropertyData>(nullptr,
  TEXT("/Game/NeonCity/Property/DA_StarterApartment.DA_StarterApartment"));
 auto* Authored=LoadObject<UNeonCityPropertyData>(nullptr,
  TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/DA_C920_StarterResidence.DA_C920_StarterResidence"));
 if(!TestNotNull(TEXT("Shared canonical starter data"),Canonical)
  || !TestNotNull(TEXT("Actual adopted C920 address data"),Authored)) return false;
 const FName ID(TEXT("NeonCity.Property.StarterApartment"));
 if(!TestEqual(TEXT("Canonical identity remains stable"),Canonical->PropertyID,ID)
  || !TestEqual(TEXT("New authored address retains that identity"),Authored->PropertyID,ID)) return false;

 auto* World=FAutomationEditorCommonUtils::CreateNewMap();
 if(!TestNotNull(TEXT("Isolated editor world"),World)) return false;
 auto* GI=NewObject<UGameInstance>();
 struct FRestoreWorldGameInstance
 {
  UWorld* World; UGameInstance* Original;
  ~FRestoreWorldGameInstance(){ World->SetGameInstance(Original); }
 } Restore{World,World->GetGameInstance()};
 World->SetGameInstance(GI);
 auto* Registry=NewObject<UPropertySubsystem>(GI);
 if(!TestTrue(TEXT("Register original catalogue object"),Registry->RegisterProperty(Canonical))) return false;
 FNeonCityPropertyRecord Original;
 Registry->GetProperty(ID,Original);
 const bool OriginalInitiallyOwned=Canonical->bInitiallyOwned;
 TestFalse(TEXT("Direct catalogue registration still rejects a second asset for the same ID"),Registry->RegisterProperty(Authored));

 auto* Owner=World->SpawnActor<AActor>();
 if(!TestNotNull(TEXT("Authored physical property fixture"),Owner)) return false;
 auto* Root=NewObject<USceneComponent>(Owner);
 Owner->SetRootComponent(Root);Owner->AddInstanceComponent(Root);Root->RegisterComponent();
 Owner->SetActorLocation(FVector(3300,-700,55));
 auto* Component=NewObject<UPropertyComponent>(Owner);
 Owner->AddInstanceComponent(Component);
 Component->Data=Authored;
 Component->HomeSpawnLocal=FTransform(FRotator(0,90,0),FVector(-350,0,99));
 // The real replacement bay, expressed relative to this isolated physical root.
 Component->GarageSlotsLocal={FVector(499.5,-130,0)};
 Component->RegisterComponent();

 FNeonCityPropertySaveData Saved;
 Saved.PropertyID=ID;Saved.Owned=false;
 Registry->ApplyPersistenceSnapshot({Saved});
 TestFalse(TEXT("A world without an adoption site rejects the duplicate asset"),Registry->RegisterLiveProperty(Component));
 FNeonCityPropertyRecord Record;
 Registry->GetProperty(ID,Record);
 TestTrue(TEXT("Rejected adoption preserves old presentation"),Record.Address.EqualTo(Original.Address) && Record.Location.Equals(Original.Location));
 auto* Site=World->SpawnActor<ANeonCityMainlandSite>();
 if(!TestNotNull(TEXT("Persistent mainland site fixture"),Site)) return false;
 TestFalse(TEXT("An untagged site does not authorize an address migration"),Registry->RegisterLiveProperty(Component));
 Site->Tags.Add(TEXT("NeonCity.MainlandC920"));
 if(!TestTrue(TEXT("Exact authored address admitted by a single tagged persistent site"),Registry->RegisterLiveProperty(Component))) return false;
 Registry->GetProperty(ID,Record);
 if(!TestEqual(TEXT("Existing slot schema retained"),Record.GarageSlots.Num(),1)) return false;
 TestEqual(TEXT("Same record identity"),Record.PropertyID,ID);
 TestFalse(TEXT("Explicitly unowned saved state is never re-owned by address adoption"),Record.Owned);
 TestTrue(TEXT("Unowned home identity remains empty"),Registry->GetHomePropertyID().IsNone());
 TestTrue(TEXT("Phone record receives authored name and address"),Record.DisplayName.EqualTo(Authored->DisplayName) && Record.Address.EqualTo(Authored->Address));
 TestTrue(TEXT("Property location receives authored presentation"),Record.Location.Equals(Authored->Location));
 const FVector Bay(3799.5,-830,55);
 TestTrue(TEXT("Garage uses the live physical bay"),Record.GarageSlots[0].Location.Equals(Bay,.01));

 // Synthetic value-only saved ownership/link, not evidence of driving or storage.
 const FGuid VehicleID=FGuid::NewGuid();
 Saved.Owned=true;
 FNeonCityGarageSaveData Slot;
 Slot.SlotIndex=0;Slot.VehicleID=VehicleID;Slot.StoredLocation=FVector(-91350,38000,1040);
 Saved.GarageSlots={Slot};
 Registry->ApplyPersistenceSnapshot({Saved});
 TestTrue(TEXT("Idempotent live adoption"),Registry->RegisterLiveProperty(Component));
 Registry->GetProperty(ID,Record);
 TestTrue(TEXT("Saved ownership is retained"),Record.Owned);
 TestEqual(TEXT("Saved garage link is retained"),Record.GarageSlots[0].VehicleID,VehicleID);
 TestEqual(TEXT("Slot index is retained"),Record.GarageSlots[0].SlotIndex,0);
 TestEqual(TEXT("Home property identity is retained"),Registry->GetHomePropertyID(),ID);
 FTransform Spawn;
 TestTrue(TEXT("Home spawn resolves through the admitted live component"),Registry->GetHomeSpawnTransform(Spawn));
 TestTrue(TEXT("Home spawn uses the physical replacement transform"),Spawn.Equals(Component->GetHomeSpawnTransform(),.01));
 TArray<FNeonCityPropertySaveData> LegacySnapshot{Saved};
 TArray<FString> Warnings;
 Registry->NormalizePersistenceSnapshot(LegacySnapshot,Warnings);
 TestEqual(TEXT("Supported legacy property and slot require no warnings"),Warnings.Num(),0);
 TestTrue(TEXT("Legacy disk location normalizes to the actual new bay"),LegacySnapshot[0].GarageSlots[0].StoredLocation.Equals(Bay,.01));
 TestEqual(TEXT("Normalization preserves the same linked car"),LegacySnapshot[0].GarageSlots[0].VehicleID,VehicleID);
 const auto Defaults=Registry->BuildPersistenceSnapshot(true);
 TestTrue(TEXT("Default snapshot also uses the admitted physical bay"),Defaults.Num()==1 && Defaults[0].GarageSlots.Num()==1 && Defaults[0].GarageSlots[0].StoredLocation.Equals(Bay,.01));

 auto* DuplicateOwner=World->SpawnActor<AActor>();
 if(!TestNotNull(TEXT("Conflicting live owner fixture"),DuplicateOwner)) return false;
 auto* Duplicate=NewObject<UPropertyComponent>(DuplicateOwner);
 DuplicateOwner->AddInstanceComponent(Duplicate);
 Duplicate->Data=Authored;Duplicate->GarageSlotsLocal=Component->GarageSlotsLocal;
 Duplicate->RegisterComponent();
 TestFalse(TEXT("Exact migration asset cannot replace another live owner"),Registry->RegisterLiveProperty(Duplicate));
 Registry->UnregisterLiveProperty(Duplicate);
 TestTrue(TEXT("Rejected owner's unregister leaves the admitted live home intact"),Registry->GetHomeSpawnTransform(Spawn) && Spawn.Equals(Component->GetHomeSpawnTransform(),.01));

 auto* Unapproved=DuplicateObject<UNeonCityPropertyData>(Authored,GI);
 Component->Data=Unapproved;
 TestFalse(TEXT("Same ID and values in an unapproved package cannot adopt"),Registry->RegisterLiveProperty(Component));
 Component->Data=Authored;
 auto* SecondSite=World->SpawnActor<ANeonCityMainlandSite>();
 if(!TestNotNull(TEXT("Ambiguous site fixture"),SecondSite)) return false;
 SecondSite->Tags.Add(TEXT("NeonCity.MainlandC920"));
 TestFalse(TEXT("Multiple persistent sites fail closed"),Registry->RegisterLiveProperty(Component));
 SecondSite->Destroy();
 TestTrue(TEXT("Single-site registration remains idempotent"),Registry->RegisterLiveProperty(Component));

 // A catalogue schema mismatch cannot be disguised as an address-only update.
 auto* SchemaRegistry=NewObject<UPropertySubsystem>(GI);
 auto* DifferentSchema=DuplicateObject<UNeonCityPropertyData>(Canonical,GI);
 DifferentSchema->Price+=1;
 TestTrue(TEXT("Independent fixture catalogue registers"),SchemaRegistry->RegisterProperty(DifferentSchema));
 TestFalse(TEXT("Migration cannot change the immutable property price"),SchemaRegistry->RegisterLiveProperty(Component));
 Registry->GetProperty(ID,Record);
 TestEqual(TEXT("Rejected operations retain linked car"),Record.GarageSlots[0].VehicleID,VehicleID);
 TestEqual(TEXT("Rejected operations retain one property record"),Registry->GetProperties().Num(),1);
 TestTrue(TEXT("Catalogue still holds the exact original shared object"),Registry->RegisterProperty(Canonical));
 TestFalse(TEXT("Authored presentation does not relax direct catalogue duplicates"),Registry->RegisterProperty(Authored));
 TestTrue(TEXT("Canonical authored text remains unchanged"),Canonical->DisplayName.EqualTo(Original.DisplayName) && Canonical->Address.EqualTo(Original.Address));
 TestTrue(TEXT("Canonical authored location remains unchanged"),Canonical->Location.Equals(Original.Location));
 TestEqual(TEXT("Canonical type remains unchanged"),Canonical->Type,Original.Type);
 TestEqual(TEXT("Canonical price remains unchanged"),Canonical->Price,Original.Price);
 TestEqual(TEXT("Canonical slot count remains unchanged"),Canonical->GarageSlotCount,Original.GarageSlots.Num());
 TestEqual(TEXT("Canonical default ownership remains unchanged"),Canonical->bInitiallyOwned,OriginalInitiallyOwned);
 return !HasAnyErrors();
}
#endif
