#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Player/NeonCityPlayerController.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Phone/PhoneComponent.h"
#include "Phone/PhoneSubsystem.h"
#include "Phone/MessageSystem.h"
#include "Missions/Prototype/NeonCityTransportTest.h"
#include "Property/NeonCityStarterApartment.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "Combat/HealthComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Vehicles/VehicleData.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/JsonSerializer.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"

namespace NeonCityDeliveryNavigation
{
void Key(APlayerController* PC,FKey K,EInputEvent E)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64())); }
bool SessionManifest(FAutomationTestBase* Test,const UNeonCitySaveGame* Model,FGuid VehicleID,const TCHAR* Session)
{
 const auto* Vehicle=Model->Vehicles.FindByPredicate([&](const auto& Item){return Item.VehicleID==VehicleID;});
 const auto* Opportunity=Model->Phone.Opportunities.FindByPredicate([](const auto& Item){return Item.OpportunityID==ANeonCityTransportTest::ReceiptKey();});
 if(!Test->TestTrue(TEXT("Disk record contains the completed assigned delivery"),Vehicle && Opportunity && Opportunity->State==2))return false;
 const auto Json=MakeShared<FJsonObject>();
 Json->SetNumberField(TEXT("processId"),FPlatformProcess::GetCurrentProcessId());
 Json->SetStringField(TEXT("saveId"),Model->SaveID.ToString());
 Json->SetStringField(TEXT("vehicleId"),VehicleID.ToString());
 Json->SetNumberField(TEXT("condition"),Vehicle->Condition);
 Json->SetBoolField(TEXT("owned"),Vehicle->Owned);
 Json->SetStringField(TEXT("transform"),Vehicle->Transform.ToHumanReadableString());
 Json->SetNumberField(TEXT("balance"),Model->Economy.Balance);
 Json->SetNumberField(TEXT("opportunityState"),Opportunity->State);
 Json->SetNumberField(TEXT("messageCount"),Model->Phone.Messages.Num());
 FString Text;FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Text));
 const FString Directory=FPaths::ProjectSavedDir()/TEXT("Navigation");IFileManager::Get().MakeDirectory(*Directory,true);
 return Test->TestTrue(TEXT("Cross-session evidence written"),FFileHelper::SaveStringToFile(Text,*(Directory/(FString(TEXT("Session"))+Session+TEXT(".json")))));
}
class FRoute : public IAutomationLatentCommand
{
public:
 explicit FRoute(FAutomationTestBase* T,bool SessionA=false):Test(T),CrossSession(SessionA) {}
 bool Next(int32 S) { Stage=S;At=W->GetTimeSeconds();return false; }
 bool WalkTo(const FVector& Goal)
 {
  const FVector Direction=(Goal-Player->GetActorLocation()).GetSafeNormal2D();
  if(FVector::Dist2D(Goal,Player->GetActorLocation())<28)return true;
  PC->SetControlRotation(FRotator(-8,Direction.Rotation().Yaw,0));
  Player->AddMovementInput(Direction,FMath::Clamp(float(FVector::Dist2D(Goal,Player->GetActorLocation())/180),.15f,1.f));return false;
 }
 bool Focus(AActor* Target,FVector Aim)
 {
  PC->SetControlRotation((Aim-Player->GetPawnViewLocation()).Rotation());
  return Player->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable()==Target;
 }
 void Shot(const TCHAR* Name) { Test->TestTrue(TEXT("Actual navigation HUD captured"),FNeonCityVisualEvidence::CaptureHUD(W,Name)); }
 FVector LookAhead() const
 {
  int32 Segment=0;double Best=TNumericLimits<double>::Max();FVector Closest=Path[0];
  for(int32 I=0;I<Path.Num()-1;++I)
  {
   const FVector A=Path[I],AB=Path[I+1]-A;
   const FVector2D FlatAB(AB),FlatAP(Car->GetActorLocation()-A);
   const double T=FlatAB.SizeSquared()>1?FMath::Clamp(FVector2D::DotProduct(FlatAP,FlatAB)/FlatAB.SizeSquared(),0.,1.):0;
   const FVector P=A+AB*T;const double D=FVector::DistSquared2D(P,Car->GetActorLocation());
   if(D<Best) { Best=D;Segment=I;Closest=P; }
  }
  double Remaining=650;
  for(int32 I=Segment;I<Path.Num()-1;++I)
  {
   const FVector End=Path[I+1];const double Length=FVector::Dist2D(Closest,End);
   if(Length>=Remaining)return Closest+(End-Closest)*(Remaining/FMath::Max(Length,1.));
   Remaining-=Length;Closest=End;
  }
  return Path.Last();
 }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>180) { Test->AddError(FString::Printf(TEXT("Normal Marcus route timed out stage %d at %s"),Stage,PC&&PC->GetPawn()?*PC->GetPawn()->GetActorLocation().ToString():TEXT("no pawn")));return true; }
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;
  if(!PC || W->GetTimeSeconds()<3)return false;
  if(Stage==0)
  {
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   for(TActorIterator<ANeonCityTransportTest> It(W);It;++It)Contract=*It;
   for(TActorIterator<ANeonCityStarterApartment> It(W);It;++It)Home=*It;
   if(!Test->TestTrue(TEXT("Ordinary South Beach home and delivery exist"),Contract && Home && Contract->DeliveryVehicle))return true;
   Car=Contract->DeliveryVehicle;Navigation=PC->GetNavigationComponent();
   if(!Test->TestNotNull(TEXT("Controller owns navigation across possession"),Navigation))return true;
   Economy=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   Test->TestTrue(TEXT("Normal spawn starts inside the owned apartment"),Home->IsInsideInterior_Implementation(Player));
   Test->TestEqual(TEXT("Normal new-game funds"),Economy->GetBalance(),int64(1850));
   Save->ConfigureSlot(CrossSession?TEXT("NeonCity_Test_MarcusNavigation_CrossSession"):TEXT("NeonCity_Test_MarcusNavigation"));Save->DeleteSave();
   CarOrigin=Car->GetActorLocation();Key(PC,EKeys::P,IE_Pressed);return Next(1);
  }
  const float E=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==1 && E>.4f)
  {
   Key(PC,EKeys::P,IE_Released);Test->TestTrue(TEXT("P reads Marcus offer"),PC->GetPhoneComponent()->IsPhoneOpen());Shot(TEXT("Marcus-Offer.png"));
   Key(PC,EKeys::Enter,IE_Pressed);return Next(2);
  }
  if(Stage==2 && E>.4f)
  {
   Key(PC,EKeys::Enter,IE_Released);Test->TestEqual(TEXT("Actual Enter accepts"),Contract->GetState(),ENeonCityTransportState::Accepted);
   Test->TestFalse(TEXT("Phone owns its screen region"),Navigation->IsPresented());
   Test->TestTrue(TEXT("Accepted task snapshot saves safely"),Save->SaveGame());
   Navigation->ClearDestination(Contract->ReceiptKey());
   Test->TestTrue(TEXT("Accepted task reload succeeds"),Save->LoadGame());return Next(3);
  }
  if(Stage==3 && E>.5f)
  {
   Test->TestEqual(TEXT("Load rebuilds stable pickup destination"),Navigation->GetDestination().DestinationID,FName(TEXT("SouthBeach.Marcus.AssignedSedan")));
   Test->TestTrue(TEXT("Live actor matched after load, no PIE-name persistence"),Navigation->GetDestination().MovingTarget==Car);
   Key(PC,EKeys::P,IE_Pressed);return Next(4);
  }
  if(Stage==4 && E>.3f) { Key(PC,EKeys::P,IE_Released);return Next(5); }
  if(Stage==5)
  {
   if(!WalkTo(Home->GetActorTransform().TransformPosition(FVector(0,230,99))))return false;
   if(!Focus(Home,Home->GetActorLocation()+FVector(0,0,130)))return false;
   Key(PC,EKeys::F,IE_Pressed);return Next(6);
  }
  if(Stage==6 && E>.4f)
  {
   Key(PC,EKeys::F,IE_Released);if(!Test->TestFalse(TEXT("F leaves the real home"),Home->IsInsideInterior_Implementation(Player)))return true;
   return Next(7);
  }
  if(Stage==7)
  {
   if(!WalkTo(Car->GetActorLocation()-Car->GetActorRightVector()*225))return false;
   if(!Focus(Car,Car->GetActorLocation()+FVector(0,0,65)))return false;
   Test->TestTrue(TEXT("Pickup minimap visible on foot"),Navigation->IsPresented());Shot(TEXT("Marcus-FindVehicle.png"));
   Key(PC,EKeys::F,IE_Pressed);return Next(8);
  }
  if(Stage==8 && E>.5f)
  {
   Key(PC,EKeys::F,IE_Released);if(!Test->TestTrue(TEXT("F enters assigned sedan in its original parking bay"),PC->GetPawn()==Car))return true;
   Test->TestTrue(TEXT("Car has not been staged near the goal"),FVector::Dist2D(Car->GetActorLocation(),CarOrigin)<40);
   Test->TestEqual(TEXT("Possession updates delivery destination"),Navigation->GetDestination().DestinationID,FName(TEXT("SouthBeach.Marcus.Delivery")));
   if(!Test->TestTrue(TEXT("Minimap uses authored road guide"),Navigation->GetDisplay().UsingRoad && Navigation->GetDisplay().Route.Num()>=3))return true;
   Path=Navigation->GetDisplay().Route;Shot(TEXT("Marcus-RoadRoute.png"));return Next(9);
  }
  if(Stage==9)
  {
   // Automated driver feeds the existing throttle/steer/brake ports. The real
   // Chaos car must cross the driveway and traverse the route; no actor staging,
   // velocity writes, disabled traffic, collision changes or time dilation.
   const FVector Local=Car->GetActorTransform().InverseTransformVectorNoScale(LookAhead()-Car->GetActorLocation());
   const double Angle=FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X));
   const float Distance=FVector::Dist2D(Car->GetActorLocation(),Contract->GetActorLocation());
   const float Speed=FVector::DotProduct(Car->GetVelocity(),Car->GetActorForwardVector());
   float Desired=FMath::Abs(Angle)>20?250:500;
   if(Distance<1200)Desired=FMath::Min(Desired,300.f);
   if(Distance<250)Desired=0;
   const float Steer=FMath::Clamp(float(FMath::RadiansToDegrees(FMath::Atan2(500*Local.Y,FMath::Max(1.,Local.SizeSquared2D())))/32),-1.f,1.f);
   auto* Drive=Car->GetDrivingComponent();Drive->SetSteerInput(Steer);
   Drive->SetDriveInput(FMath::Clamp((Desired-Speed)/180.f,0.f,1.f));
   Drive->SetBrakeInput(Desired==0?1.f:Speed>Desired+30?.4f:0.f);
   PC->SetControlRotation(FRotator(-12,Car->GetActorRotation().Yaw,0));
   if(Contract->GetState()==ENeonCityTransportState::Completed)
   {
    Drive->SetDriveInput(0);Drive->SetSteerInput(0);Drive->SetBrakeInput(1);
    Test->TestTrue(TEXT("Complete normal-map trip physically crosses over 70 metres"),FVector::Dist2D(CarOrigin,Car->GetActorLocation())>7000);
    Test->TestEqual(TEXT("Only existing Marcus reward credited"),Economy->GetBalance(),int64(2200));return Next(10);
   }
   if(E>90) { Test->AddError(FString::Printf(TEXT("Driving route failed: location %s speed %.1f health %.1f angle %.1f"),*Car->GetActorLocation().ToString(),Speed,Car->GetHealthComponent()->GetCurrentHealth(),Angle));return true; }
  }
  if(Stage==10 && E>.5f)
  {
   Test->TestFalse(TEXT("Completion clears active guidance"),Navigation->GetDisplay().Active);
   Test->TestTrue(TEXT("Notice uses actual reward"),Navigation->GetDisplay().Notice.ToString().Contains(TEXT("+$350")));
   Shot(TEXT("Marcus-Completed.png"));Key(PC,EKeys::F,IE_Pressed);return Next(11);
  }
  if(Stage==11 && E>.6f)
  {
   Key(PC,EKeys::F,IE_Released);if(!Test->TestTrue(TEXT("F exits at delivered car"),PC->GetPawn()==Player))return true;
   if(!Test->TestTrue(TEXT("Completed delivery saves"),Save->SaveGame()))return true;
   Test->TestTrue(TEXT("Completed delivery reloads"),Save->LoadGame());return Next(12);
  }
  if(Stage==12 && E>.6f)
  {
   Test->TestEqual(TEXT("Money survives reload"),Economy->GetBalance(),int64(2200));
   Test->TestEqual(TEXT("Completed state survives reload"),Contract->GetState(),ENeonCityTransportState::Completed);
   Test->TestFalse(TEXT("Completed save does not resurrect marker"),Navigation->GetDisplay().Active);
   Test->TestFalse(TEXT("Existing receipt still prevents repeat reward"),Economy->CreditOnce(350,Contract->ReceiptKey()));
   Test->TestTrue(TEXT("Player keeps ordinary walking after reload"),Player->GetCharacterMovement()->IsMovingOnGround());
   if(CrossSession)
   {
    TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());
    if(Test->TestTrue(TEXT("Completed route has a readable disk save"),Model.IsValid()))SessionManifest(Test,Model.Get(),Car->VehicleID,TEXT("A"));
   }
   else Save->DeleteSave();
   return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;UWorld* W=nullptr;ANeonCityPlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;
 ANeonCityStarterApartment* Home=nullptr;ANeonCityTransportTest* Contract=nullptr;ANeonCityBaseVehicle* Car=nullptr;
 UNeonCityNavigationComponent* Navigation=nullptr;UNeonCityEconomySubsystem* Economy=nullptr;UNeonCitySaveSubsystem* Save=nullptr;
 int32 Stage=0;float At=0;double Started=0;FVector CarOrigin;TArray<FVector> Path;
 bool CrossSession=false;
 FNeonCityVisualEvidence Evidence{TEXT("N-MarcusDelivery")};
};

class FRestart : public IAutomationLatentCommand
{
public:
 explicit FRestart(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>60) { Test->AddError(TEXT("Restarted Marcus route load timed out"));return true; }
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;
  if(!PC || W->GetTimeSeconds()<3)return false;
  auto* Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
  auto* Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
  if(!Loaded)
  {
   Save->ConfigureSlot(TEXT("NeonCity_Test_MarcusNavigation_CrossSession"));
   if(!Test->TestTrue(TEXT("Restart finds Session A on disk"),Save->DoesSaveExist()))return true;
   TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());
   if(!Test->TestTrue(TEXT("Disk save readable"),Model.IsValid()))return true;
   FString Previous;FFileHelper::LoadFileToString(Previous,*(FPaths::ProjectSavedDir()/TEXT("Navigation/SessionA.json")));
   TSharedPtr<FJsonObject> Json;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Previous),Json);
   double PID=0;FString SaveID;
   if(!Test->TestTrue(TEXT("Previous evidence belongs to another process and the same save"),Json.IsValid() && Json->TryGetNumberField(TEXT("processId"),PID) && PID>0 && PID!=FPlatformProcess::GetCurrentProcessId() && Json->TryGetStringField(TEXT("saveId"),SaveID) && SaveID==Model->SaveID.ToString()))return true;
   if(!Test->TestTrue(TEXT("Restart loads actual completed delivery"),Save->LoadGame()))return true;
   ANeonCityTransportTest* Contract=nullptr;for(TActorIterator<ANeonCityTransportTest> It(W);It;++It)Contract=*It;
   if(!Test->TestTrue(TEXT("Restart matches map delivery and car"),Contract && Contract->DeliveryVehicle))return true;
   auto* Car=Contract->DeliveryVehicle.Get();
   const auto* Record=Model->Vehicles.FindByPredicate([&](const auto& Item){return Item.VehicleID==Car->VehicleID;});
   if(!Test->TestNotNull(TEXT("Stable assigned vehicle ID matches disk"),Record))return true;
   Test->TestTrue(TEXT("Delivered vehicle remains at saved destination"),Car->GetActorTransform().Equals(Record->Transform,.1f));
   Test->TestTrue(TEXT("Condition restored"),FMath::IsNearlyEqual(Car->GetHealthComponent()->GetCurrentHealth(),Record->Condition*Car->GetVehicleProfile()->Durability,.01f));
   auto* Economy=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
   Test->TestEqual(TEXT("Restart retains earned cash"),Economy->GetBalance(),int64(2200));
   Test->TestEqual(TEXT("Restart retains completed contract"),Contract->GetState(),ENeonCityTransportState::Completed);
   Test->TestFalse(TEXT("Restart cannot pay duplicate reward"),Economy->CreditOnce(350,Contract->ReceiptKey()));
   Test->TestFalse(TEXT("Restart does not resurrect delivery destination"),PC->GetNavigationComponent()->GetDisplay().Active);
   SessionManifest(Test,Model.Get(),Car->VehicleID,TEXT("B"));
   Key(PC,EKeys::P,IE_Pressed);Loaded=true;At=W->GetTimeSeconds();return false;
  }
  if(W->GetTimeSeconds()-At<.5f)return false;
  Key(PC,EKeys::P,IE_Released);
  Test->TestTrue(TEXT("Completion message remains readable after restart"),PC->GetPhoneComponent()->GetDisplayedText().ToString().Contains(TEXT("Delivery complete")));
  Test->TestTrue(TEXT("Restart phone evidence captured"),FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Marcus-Restart.png")));
  Save->DeleteSave();return true;
 }
private:
 FAutomationTestBase* Test;double Started=0;float At=0;bool Loaded=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityMarcusNavigation,"NeonCity.Navigation.MarcusDeliveryPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityMarcusNavigation::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityDeliveryNavigation::FRoute>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FNeonCityNavigationRestart,"NeonCity.Navigation.CrossSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FNeonCityNavigationRestart::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 FString Mode;FParse::Value(FCommandLine::Get(),TEXT("NeonCityNavigationSession="),Mode);
 if(Mode==TEXT("A") || Mode==TEXT("B")) { Names.Add(Mode);Commands.Add(Mode); }
}
bool FNeonCityNavigationRestart::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 if(Parameters==TEXT("A"))FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityDeliveryNavigation::FRoute>(this,true));
 else FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityDeliveryNavigation::FRestart>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
