#include "Property/PropertySubsystem.h"
#include "Property/PropertyComponent.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleInteraction.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/HealthComponent.h"
#include "Engine/GameInstance.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/NeonCityMainlandSite.h"
void UPropertySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection); Collection.InitializeDependency<UVehicleOwnershipSubsystem>();
 if(auto* Data=LoadObject<UNeonCityPropertyData>(nullptr,TEXT("/Game/NeonCity/Property/DA_StarterApartment.DA_StarterApartment"))) RegisterProperty(Data);
}
void UPropertySubsystem::Deinitialize()
{ LiveProperties.Reset(); Catalogue.Reset(); Records.Reset(); HomePropertyID=NAME_None; OnPropertyChanged.Clear(); Super::Deinitialize(); }
bool UPropertySubsystem::RegisterProperty(UNeonCityPropertyData* Data)
{
 if(!IsValid(Data) || !Data->IsValidData()) return false;
 if(const auto* Existing=Catalogue.Find(Data->PropertyID)) return *Existing==Data;
 if(Records.Num()>=128) return false;
 FNeonCityPropertyRecord Record; Record.PropertyID=Data->PropertyID; Record.DisplayName=Data->DisplayName;
 Record.Type=Data->Type; Record.Price=Data->Price; Record.Location=Data->Location; Record.Address=Data->Address; Record.Owned=Data->bInitiallyOwned;
 Record.DailyIncome=Data->DailyIncome;
 for(int32 I=0;I<Data->GarageSlotCount;++I) { FNeonCityGarageSlot Slot; Slot.SlotIndex=I; Record.GarageSlots.Add(Slot); }
 Catalogue.Add(Data->PropertyID,Data); Records.Add(Data->PropertyID,Record);
 if(Record.Owned && IsBetterHome(Data->PropertyID,HomePropertyID)) HomePropertyID=Data->PropertyID;
 OnPropertyChanged.Broadcast(Data->PropertyID); return true;
}
bool UPropertySubsystem::IsBetterHome(FName Candidate,FName Current) const
{
 if(Current.IsNone())return true;
 const auto* A=Catalogue.Find(Candidate);const auto* B=Catalogue.Find(Current);
 const bool PA=A && *A && (*A)->bPreferredHome,PB=B && *B && (*B)->bPreferredHome;
 if(PA!=PB)return PA;
 return Candidate.LexicalLess(Current);
}
bool UPropertySubsystem::RegisterLiveProperty(UPropertyComponent* Component)
{
 if(!IsValid(Component) || !Component->GetOwner() || !Component->GetOwner()->HasAuthority() || !Component->GetWorld()
  || Component->GetWorld()->GetGameInstance()!=GetGameInstance() || !Component->Data || !Component->Data->IsValidData()
  || Component->GarageSlotsLocal.Num()!=Component->Data->GarageSlotCount || Component->HomeSpawnLocal.ContainsNaN()) return false;
 for(const FVector& Slot:Component->GarageSlotsLocal) if(Slot.ContainsNaN()) return false;
 const FName ID=Component->GetPropertyID();
 if(const auto* Existing=LiveProperties.Find(ID); Existing && Existing->IsValid() && Existing->Get()!=Component) return false;
 bool AdoptPresentation=false;
 if(!RegisterProperty(Component->Data))
 {
  const auto* Canonical=Catalogue.Find(ID);
  if(!Canonical || !CanAdoptMainlandPresentation(Component,Canonical->Get())) return false;
  AdoptPresentation=true;
 }
 LiveProperties.Add(ID,Component);
 auto& Record=Records.FindChecked(ID);
 if(AdoptPresentation)
 {
  Record.DisplayName=Component->Data->DisplayName;
  Record.Address=Component->Data->Address;
  Record.Location=Component->Data->Location;
 }
 for(auto& Slot:Record.GarageSlots) Slot.Location=Component->GetGarageLocation(Slot.SlotIndex);
 OnPropertyChanged.Broadcast(ID); return true;
}
bool UPropertySubsystem::CanAdoptMainlandPresentation(const UPropertyComponent* Component,const UNeonCityPropertyData* Canonical) const
{
 if(!IsValid(Component) || !IsValid(Canonical) || !Canonical->IsValidData()) return false;
 const auto* Data=Component->Data.Get();
 const UWorld* World=Component->GetWorld();
 if(!IsValid(Data) || !World || !Component->GetOwner() || Component->GetOwner()->GetLevel()!=World->PersistentLevel
  || Data->PropertyID!=Canonical->PropertyID || Data->Type!=Canonical->Type || Data->Price!=Canonical->Price
  || Data->bInitiallyOwned!=Canonical->bInitiallyOwned || Data->GarageSlotCount!=Canonical->GarageSlotCount) return false;
 const auto* Record=Records.Find(Data->PropertyID);
 if(!Record || Record->GarageSlots.Num()!=Canonical->GarageSlotCount) return false;

 // Exact owned packages opt in only these two moved mainland addresses. Other
 // duplicate IDs, including streamed island properties, keep the strict policy.
 const TCHAR* ExpectedPath=nullptr;
 if(Data->PropertyID==FName(TEXT("NeonCity.Property.StarterApartment")))
  ExpectedPath=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/DA_C920_StarterResidence.DA_C920_StarterResidence");
 else if(Data->PropertyID==FName(TEXT("Property.Mainland.RivieraEstate916")))
  ExpectedPath=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/DA_C920_CourtyardResidence.DA_C920_CourtyardResidence");
 if(!ExpectedPath || !Data->GetPathName().Equals(ExpectedPath,ESearchCase::CaseSensitive)) return false;

 const ANeonCityMainlandSite* Site=nullptr;
 for(TActorIterator<ANeonCityMainlandSite> It(World);It;++It)
  if(It->GetLevel()==World->PersistentLevel)
  {
   if(Site) return false;
   Site=*It;
  }
 return Site && Site->ActorHasTag(TEXT("NeonCity.MainlandC920"));
}
void UPropertySubsystem::UnregisterLiveProperty(UPropertyComponent* Component)
{
 if(Component) if(auto* Found=LiveProperties.Find(Component->GetPropertyID()); Found && Found->Get()==Component) LiveProperties.Remove(Component->GetPropertyID());
}
bool UPropertySubsystem::IsOwned(FName ID) const { const auto* Record=Records.Find(ID); return Record && Record->Owned; }
TArray<FNeonCityPropertyRecord> UPropertySubsystem::GetProperties() const
{
 TArray<FNeonCityPropertyRecord> Result; Records.GenerateValueArray(Result);
 Result.Sort([](const auto& A,const auto& B){ return A.PropertyID.LexicalLess(B.PropertyID); }); return Result;
}
bool UPropertySubsystem::GetProperty(FName ID,FNeonCityPropertyRecord& Record) const
{ if(const auto* Found=Records.Find(ID)) { Record=*Found; return true; } Record={}; return false; }
bool UPropertySubsystem::GetHomeSpawnTransform(FTransform& Transform) const
{
 const auto* Component=LiveProperties.Find(HomePropertyID);
 if(!IsOwned(HomePropertyID) || !Component || !Component->IsValid()) return false;
 Transform=Component->Get()->GetHomeSpawnTransform(); return true;
}
bool UPropertySubsystem::IsGarageVehicleEligible(const FNeonCityGarageSlot& Slot,const ANeonCityBaseVehicle* Vehicle) const
{
 return IsValid(Vehicle) && Vehicle->HasAuthority() && Vehicle->GetWorld()==GetWorld() && !Vehicle->GetController()
  && !Vehicle->GetVehicleInteraction()->GetDriver() && !Vehicle->GetHealthComponent()->IsDead()
  && FVector::Dist2D(Vehicle->GetActorLocation(),Slot.Location)<=250
  && FMath::Abs(Vehicle->GetActorLocation().Z-Slot.Location.Z)<150;
}
bool UPropertySubsystem::IsParked(const FNeonCityGarageSlot& Slot,const ANeonCityBaseVehicle* Vehicle) const
{ return IsGarageVehicleEligible(Slot,Vehicle) && Vehicle->GetVelocity().Size()<50; }
bool UPropertySubsystem::CanRetainGarageVehicle(const FNeonCityGarageSlot& Slot,const ANeonCityBaseVehicle* Vehicle) const
{
 if(IsParked(Slot,Vehicle))return true;
 if(!IsGarageVehicleEligible(Slot,Vehicle) || Vehicle->GetVelocity().Size2D()>=50
  || Vehicle->GetActorUpVector().Z<.7)return false;
 // Registration still requires full 3D rest. A previously registered car may
 // rebound vertically after restore even though it has not driven out of its
 // bay. Accept that motion only within the actual authored tire/suspension
 // reach; a falling car above the floor or an unsupported chassis cannot keep
 // the link merely because its XY speed is small. No timer/grace period is used.
 const auto* Movement=Vehicle->GetChaosMovement();
 const auto* Mesh=Vehicle->GetMesh();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(GarageSuspensionSupport),false,Vehicle);
 for(TActorIterator<APawn> It(GetWorld());It;++It)Query.AddIgnoredActor(*It);
 const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
 int32 Supported=0;
 for(const auto& Setup:Movement->WheelSetups)
 {
  if(!Setup.WheelClass || !Mesh->DoesSocketExist(Setup.BoneName))continue;
  const auto* Wheel=Setup.WheelClass->GetDefaultObject<UChaosVehicleWheel>();
  if(!Wheel || !FMath::IsFinite(Wheel->WheelRadius) || Wheel->WheelRadius<=0)continue;
  const FVector Centre=Mesh->GetSocketLocation(Setup.BoneName)
   +Mesh->GetComponentTransform().TransformVectorNoScale(Setup.AdditionalOffset);
  const float Raise=FMath::Max(0.f,Wheel->SuspensionMaxRaise);
  const float Reach=Wheel->WheelRadius+FMath::Max(0.f,Wheel->SuspensionMaxDrop)+5.f;
  FHitResult Hit;
  if(GetWorld()->LineTraceSingleByObjectType(Hit,Centre+FVector(0,0,Raise),Centre-FVector(0,0,Reach),Objects,Query)
   && Hit.ImpactNormal.Z>=.7 && Hit.GetComponent() && !Hit.GetComponent()->IsSimulatingPhysics()
   && Hit.GetComponent()->GetCollisionResponseToChannel(ECC_Vehicle)==ECR_Block && ++Supported>=2)return true;
 }
 return false;
}
bool UPropertySubsystem::StoreVehicle(FName ID,int32 Index,ANeonCityBaseVehicle* Vehicle)
{
 // Reconcile before taking map references: listeners may register other catalogue entries.
 ReconcileGarage(ID);
 auto* Record=Records.Find(ID); const auto* Live=LiveProperties.Find(ID);
 auto* Vehicles=GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();
 if(!Record || !Record->Owned || !Live || !Live->IsValid() || !Record->GarageSlots.IsValidIndex(Index)
  || !IsParked(Record->GarageSlots[Index],Vehicle) || !Vehicles || !Vehicles->IsOwned(Vehicle->VehicleID)
  || Vehicles->FindLiveVehicle(Vehicle->VehicleID)!=Vehicle) return false;
 auto& Slot=Record->GarageSlots[Index];
 if(Slot.VehicleID.IsValid()) return Slot.VehicleID==Vehicle->VehicleID;
 for(const auto& Pair:Records) for(const auto& Other:Pair.Value.GarageSlots) if(Other.VehicleID==Vehicle->VehicleID) return false;
 Slot.VehicleID=Vehicle->VehicleID; OnPropertyChanged.Broadcast(ID); return true;
}
bool UPropertySubsystem::ReleaseVehicle(FName ID,int32 Index)
{
 auto* Record=Records.Find(ID);
 if(!Record || !Record->Owned || !Record->GarageSlots.IsValidIndex(Index) || !Record->GarageSlots[Index].VehicleID.IsValid()) return false;
 Record->GarageSlots[Index].VehicleID.Invalidate(); OnPropertyChanged.Broadcast(ID); return true;
}
ANeonCityBaseVehicle* UPropertySubsystem::FindParkedVehicle(FName ID,int32 Index) const
{
 const auto* Record=Records.Find(ID); const auto* Live=LiveProperties.Find(ID);
 if(!Record || !Record->Owned || !Live || !Live->IsValid() || !Record->GarageSlots.IsValidIndex(Index)) return nullptr;
 auto* Vehicles=GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>(); if(!Vehicles) return nullptr;
 const auto& Slot=Record->GarageSlots[Index];
 if(Slot.VehicleID.IsValid() && Vehicles->IsOwned(Slot.VehicleID))
  if(auto* Linked=Vehicles->FindLiveVehicle(Slot.VehicleID);CanRetainGarageVehicle(Slot,Linked))return Linked;
 for(const auto& Owned:Vehicles->BuildSaveSnapshot())
  if(auto* Vehicle=Vehicles->FindLiveVehicle(Owned.VehicleID); IsParked(Record->GarageSlots[Index],Vehicle)) return Vehicle;
 return nullptr;
}
void UPropertySubsystem::ReconcileGarage(FName ID)
{
 auto* Record=Records.Find(ID); if(!Record) return;
 auto* Vehicles=GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>(); if(!Vehicles) return;
 bool Changed=false;
 for(auto& Slot:Record->GarageSlots)
  if(Slot.VehicleID.IsValid())
  {
   auto* Vehicle=Vehicles->FindLiveVehicle(Slot.VehicleID);
   // Unloaded actors keep their value record. A live car leaving or being occupied releases the slot.
   if(!Vehicles->IsOwned(Slot.VehicleID) || (Vehicle && !CanRetainGarageVehicle(Slot,Vehicle))) { Slot.VehicleID.Invalidate(); Changed=true; }
  }
 if(Changed) OnPropertyChanged.Broadcast(ID);
}
TArray<FNeonCityPropertyRecord> UPropertySubsystem::BuildSaveSnapshot()
{
 TArray<FName> IDs; Records.GetKeys(IDs); for(FName ID:IDs) ReconcileGarage(ID); return GetProperties();
}
bool UPropertySubsystem::Purchase(FName ID,FText& Reason)
{
 auto* Record=Records.Find(ID);
 if(!Record){Reason=NSLOCTEXT("NeonCity","PropertyUnknown","This property is not for sale.");return false;}
 if(Record->Owned){Reason=NSLOCTEXT("NeonCity","PropertyOwned","You already own it.");return false;}
 auto* Economy=GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
 if(!Economy || Economy->IsAccountOperationInProgress()){Reason=NSLOCTEXT("NeonCity","PropertyBusy","Your accounts are busy. Try again.");return false;}
 const int64 Cash=Economy->GetBalance(),Bank=Economy->GetBankBalance(),Price=Record->Price;
 if(Cash+Bank<Price)
 {Reason=FText::Format(NSLOCTEXT("NeonCity","PropertyFunds","You need ${0} (cash and bank: ${1})."),FText::AsNumber(Price),FText::AsNumber(Cash+Bank));return false;}
 // the shortfall in cash comes out of the bank first, then the whole price is paid in cash
 if(Price>Cash && !Economy->Withdraw(Price-Cash)){Reason=NSLOCTEXT("NeonCity","PropertyWithdraw","The bank could not release the money.");return false;}
 if(!Economy->RemoveMoney(Price,FName(*FString::Printf(TEXT("Property.Purchase.%s"),*ID.ToString())))){Reason=NSLOCTEXT("NeonCity","PropertyPay","The payment failed.");return false;}
 Record->Owned=true;
 if(Record->DailyIncome>0)
  for(TActorIterator<ANeonCityDayNightCycle> It(GetGameInstance()->GetWorld());It;++It){Record->LastIncomeDay=It->GetGameDay();break;}
 if(IsBetterHome(ID,HomePropertyID) && !Catalogue.FindChecked(ID)->IsBusiness() && HomePropertyID.IsNone())HomePropertyID=ID;
 OnPropertyChanged.Broadcast(ID);return true;
}
int64 UPropertySubsystem::SettleBusinessIncome(int32 GameDay)
{
 auto* Economy=GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();if(!Economy)return 0;
 int64 Paid=0;
 for(auto& Pair:Records)
 {
  auto& R=Pair.Value;
  if(!R.Owned || R.DailyIncome<=0 || R.LastIncomeDay<0 || GameDay<=R.LastIncomeDay)continue;
  const int32 Days=FMath::Min(7,GameDay-R.LastIncomeDay);
  // one receipt per business per day: a reload or a repeated broadcast never pays twice
  const FName Receipt(*FString::Printf(TEXT("Business.%s.Day%d"),*Pair.Key.ToString(),GameDay));
  if(Economy->CreditOnce(R.DailyIncome*Days,Receipt)){Paid+=R.DailyIncome*Days;}
  R.LastIncomeDay=GameDay;OnPropertyChanged.Broadcast(Pair.Key);
 }
 return Paid;
}
