#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Engine/GameInstance.h"
namespace FishOwnershipBoundaries
{
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;if(!W || !W->HasBegunPlay() || W->GetTimeSeconds()<3)return false;
  auto* F=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();if(!Test->TestNotNull(TEXT("Shared inventory required"),F))return true;
  const auto Original=F->BuildPersistenceSnapshot();const UFishData* Profile=nullptr;
  for(const UFishData* P:F->GetSpecies())if(P && P->SuitableForAquarium && P->SaltwaterSpecies && P->SizeCm.X<=60){Profile=P;break;}
  if(!Test->TestNotNull(TEXT("Real eligible species profile required"),Profile))return true;
  Test->AddInfo(TEXT("Declared synthetic ownership boundary fixtures only. No native catch/gameplay/visual acceptance claimed. No save/reward written. Original in-memory inventory is restored before PIE ends."));
  auto Reset=[&](){F->ApplyPersistenceSnapshot(FNeonCityFishingSaveData());};
  auto Fixture=[&](){FNeonCityCaughtFish R;R.FishID=FGuid::NewGuid();R.SpeciesID=Profile->SpeciesID;R.WeightKg=.30f;R.SizeCm=27.5f;R.CaughtAt=FDateTime::UtcNow();R.FishingPointID=TEXT("Test.DeclaredBoundaryFixture");return R;};
  const FName Tank=TEXT("Island.Mansion.Aquarium.Main"),Grill=TEXT("Test.Boundary.Grill");
  Reset();auto Dead=Fixture();Dead.bAlive=false;F->StoreCatch(Dead);
  Test->TestFalse(TEXT("Dead fish cannot enter tank"),F->PutInAquarium(Dead.FishID,Tank,true));
  auto Partial=Fixture();Partial.bAlive=false;Partial.CookFraction=.2f;F->StoreCatch(Partial);
  Test->TestFalse(TEXT("Partially cooked fish cannot enter tank"),F->PutInAquarium(Partial.FishID,Tank,true));
  auto Large=Fixture();Large.SizeCm=61;F->StoreCatch(Large);
  Test->TestFalse(TEXT("Fish beyond physical tank size is refused without shrinking"),F->PutInAquarium(Large.FishID,Tank,true));
  auto Valid=Fixture();F->StoreCatch(Valid);
  Test->TestFalse(TEXT("Unsupported tank refuses transfer"),F->PutInAquarium(Valid.FishID,TEXT("Tank.Unknown"),true));
  Test->TestFalse(TEXT("Wrong salinity refuses transfer"),F->PutInAquarium(Valid.FishID,Tank,false));
  Test->TestEqual(TEXT("Rejected tank operations retain all four source identities"),F->GetFish().Num(),4);
  Reset();TArray<FGuid> InTank;
  for(int I=0;I<12;++I){auto R=Fixture();Test->TestTrue(TEXT("Capacity fixture stored"),F->StoreCatch(R));Test->TestTrue(TEXT("Tank accepts each of twelve eligible identities"),F->PutInAquarium(R.FishID,Tank,true));InTank.Add(R.FishID);}
  auto Thirteenth=Fixture();F->StoreCatch(Thirteenth);
  Test->TestFalse(TEXT("Thirteenth fish stays safely in pouch"),F->PutInAquarium(Thirteenth.FishID,Tank,true));
  Test->TestFalse(TEXT("Aquarium fish cannot be reserved by BBQ"),F->ReserveForGrill(InTank[0],Grill));
  for(int I=0;I<63;++I)Test->TestTrue(TEXT("Fill remaining pouch capacity explicitly"),F->StoreCatch(Fixture()));
  Test->TestFalse(TEXT("Full pouch refuses tank withdrawal without losing the original"),F->RemoveFromAquarium(InTank[0],Tank));
  Test->TestTrue(TEXT("Rejected withdrawal retains living tank identity"),F->FindFish(InTank[0]) && F->FindFish(InTank[0])->bAlive && F->FindFish(InTank[0])->Storage==ENeonCityFishStorage::Aquarium);
  Reset();auto Ingredient=Fixture();F->StoreCatch(Ingredient);
  for(int I=0;I<63;++I)F->StoreCatch(Fixture());
  Test->TestTrue(TEXT("Reserve actual fixture identity from full pouch"),F->ReserveForGrill(Ingredient.FishID,Grill));
  Test->TestTrue(TEXT("Persist nonzero native cooking state"),F->UpdateGrillReservation(Ingredient.FishID,Grill,ENeonCityGrillStage::CookingFirst,2,.2f));
  F->StoreCatch(Fixture());
  Test->TestFalse(TEXT("Full pouch cannot discard an unfinished grill reservation"),F->CancelGrillReservation(Ingredient.FishID,Grill));
  const auto Reserved=F->BuildPersistenceSnapshot();F->ApplyPersistenceSnapshot(Reserved);
  const auto* Restored=F->FindFish(Ingredient.FishID);
  Test->TestTrue(TEXT("Boundary snapshot retains same reserved identity and cooking progress"),Restored && Restored->Storage==ENeonCityFishStorage::Grill && Restored->ContainerID==Grill && Restored->CookFraction==.2f && !Restored->bAlive);
  FGuid Pouch;for(const auto& R:F->GetFish())if(R.Storage==ENeonCityFishStorage::Pouch){Pouch=R.FishID;break;}F->ReleaseFish(Pouch);
  Test->TestTrue(TEXT("Freeing one pouch place permits exact ingredient return"),F->CancelGrillReservation(Ingredient.FishID,Grill));
  Test->TestTrue(TEXT("Returned ingredient retains size/weight/partial cook without revival"),F->FindFish(Ingredient.FishID) && F->FindFish(Ingredient.FishID)->SizeCm==Ingredient.SizeCm && F->FindFish(Ingredient.FishID)->WeightKg==Ingredient.WeightKg && !F->FindFish(Ingredient.FishID)->bAlive && F->PouchCount()==64);
  Reset();Ingredient=Fixture();F->StoreCatch(Ingredient);F->ReserveForGrill(Ingredient.FishID,Grill);F->UpdateGrillReservation(Ingredient.FishID,Grill,ENeonCityGrillStage::Plating,1,1);
  auto FullMeals=F->BuildPersistenceSnapshot();
  for(int I=0;I<16;++I){FNeonCityFishMeal M;M.MealID=M.SourceFishID=FGuid::NewGuid();M.SpeciesID=Profile->SpeciesID;M.SourceWeightKg=.30f;M.SourceSizeCm=27.5f;FullMeals.Meals.Add(M);}F->ApplyPersistenceSnapshot(FullMeals);
  FGuid Result;Test->TestFalse(TEXT("Full meal inventory leaves plated source safely reserved"),F->CompleteGrillReservation(Ingredient.FishID,Grill,Result));
  Test->TestTrue(TEXT("Capacity refusal does not consume fish or produce receipt"),F->FindFish(Ingredient.FishID) && !Result.IsValid() && F->GetMeals().Num()==16);
  auto Space=F->BuildPersistenceSnapshot();Space.Meals.RemoveAt(0);F->ApplyPersistenceSnapshot(Space);
  Test->TestTrue(TEXT("Freed meal slot completes one exact source identity"),F->CompleteGrillReservation(Ingredient.FishID,Grill,Result));
  Test->TestTrue(TEXT("One meal replaces source with exact anatomy"),Result==Ingredient.FishID && !F->FindFish(Ingredient.FishID) && F->GetMeals().Num()==16 && F->GetMeals().Last().SourceSizeCm==Ingredient.SizeCm && F->GetMeals().Last().SourceWeightKg==Ingredient.WeightKg);
  Test->TestFalse(TEXT("Repeated completion cannot duplicate food"),F->CompleteGrillReservation(Ingredient.FishID,Grill,Result));
  Test->TestFalse(TEXT("Cooked source identity cannot be stored as a new catch"),F->StoreCatch(Ingredient));
  F->ApplyPersistenceSnapshot(Original);
  Test->TestEqual(TEXT("Restore original in-memory fish count"),F->GetFish().Num(),Original.Fish.Num());
  Test->TestEqual(TEXT("Restore original in-memory meal count"),F->GetMeals().Num(),Original.Meals.Num());return true;
 }
private:FAutomationTestBase* Test;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishOwnershipBoundaryTest,"NeonCity.Phase205c.Fishing.ContainerCapacityBoundariesPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFishOwnershipBoundaryTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FishOwnershipBoundaries::FCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
