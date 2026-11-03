#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandEstate916
{
void Key(APlayerController*,FKey,bool);
// One disclosed dry-deck starting fixture. Entry, swimming and return use the
// real W mapping and existing water/CharacterMovement, with intact collision.
class FPool:public IAutomationLatentCommand
{
public:explicit FPool(FAutomationTestBase* In):Test(In){}
 bool Update()override
 {
  W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<6)return false;
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>55)return End(TEXT("Native estate pool entry/exit timeout"));
  if(!P)
  {
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return End(TEXT("Normal player absent"));Original=P->GetActorTransform();
   for(TActorIterator<ANeonCityWaterVolume> I(W);I;++I)
    if(I->bContainedPoolWater && FVector::Dist2D(I->GetActorLocation(),FVector(-92000,31300,0))<100)Pool=*I;
   if(!Pool)return End(TEXT("Installed finite estate pool absent"));
   FWaterSurfaceSample Sample;auto* Registry=W->GetSubsystem<UMarineWorldSubsystem>();
   const FVector Centre=Pool->GetActorLocation();
   if(!Registry || !Registry->QueryWater(Centre,Sample) || !Sample.ContainedWater || !Sample.InsideVolume || !Sample.Flow.IsNearlyZero())return End(TEXT("Estate pool did not use finite current-free native water"));
   if(Pool->QueryWaterSurface_Implementation(Centre-FVector(0,0,Pool->Extent.Z+2),Sample) || Pool->QueryWaterSurface_Implementation(Centre+FVector(Pool->Extent.X+2,0,0),Sample))return End(TEXT("Pool water spills outside its physical basin"));
   Deck=FVector(-92000,32100,973+P->GetSimpleCollisionHalfHeight()+3);
   P->SetActorLocation(Deck,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);At=W->GetTimeSeconds();return false;
  }
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==0){if(Age<1.5)return false;if(!P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Pool entry deck has no supported walking"));Stage=1;At=W->GetTimeSeconds();return false;}
  const FVector Goal=Stage==1?FVector(-92000,31500,973):Deck;
  const FVector D=Goal-P->GetActorLocation();
  if(P->GetCharacterMovement()->IsSwimming())
  {
   if(!Swam){Swam=true;SwimStart=P->GetActorLocation();}
   SwimDistance=FMath::Max(SwimDistance,FVector::Dist2D(SwimStart,P->GetActorLocation()));
  }
  if(D.Size2D()<30)
  {
   Key(PC,EKeys::W,false);Held=false;
   if(Stage==1){if(!Swam || SwimDistance<120 || !P->GetCharacterMovement()->IsSwimming())return End(TEXT("Actual entry did not retain native swimming over120cm at the pool goal"));Capture(TEXT("Estate916-PoolSwimming.jpg"));Stage=2;At=W->GetTimeSeconds();return false;}
   if(!P->GetCharacterMovement()->IsMovingOnGround() || FMath::Abs(P->GetActorLocation().Z-Deck.Z)>20)return End(TEXT("Actual pool exit did not restore dry supported walking"));
   Capture(TEXT("Estate916-PoolExit.jpg"));Test->AddInfo(FString::Printf(TEXT("Native W entry/swim/exit passed; swim %.1fcm, no relocation after entry"),SwimDistance));return End(TEXT(""));
  }
  if(Age>20){Capture(TEXT("Estate916-PoolBlocked.jpg"));return End(FString::Printf(TEXT("Actual pool movement blocked stage%d at%s mode%d"),Stage,*P->GetActorLocation().ToString(),int32(P->GetCharacterMovement()->MovementMode)));}
  PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));if(!Held){Key(PC,EKeys::W,true);Held=true;}return false;
 }
private:
 void Capture(const TCHAR* Name)
 {
#if WITH_EDITOR
  if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))FNeonCityVisualEvidence::CaptureRenderedJPEG(W,Name);
#endif
 }
 bool End(const FString& Error){Key(PC,EKeys::W,false);if(!Error.IsEmpty())Test->AddError(Error);if(P){P->GetCharacterMovement()->StopMovementImmediately();P->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);}return true;}
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;ANeonCityWaterVolume* Pool=nullptr;
 FTransform Original;FVector Deck,SwimStart;double Started=0;float At=0,SwimDistance=0;int32 Stage=0;bool Held=false,Swam=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstatePool916,"NeonCity.Phase205c.Mainland916.Standalone.EstatePool",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstatePool916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FPool(this));return true;}
#endif
