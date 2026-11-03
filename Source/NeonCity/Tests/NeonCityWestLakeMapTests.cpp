#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Navigation/NeonCityWorldMapWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Navigation/NeonCityMapData.h"
#include "Player/NeonCityPlayerController.h"
#include "Property/HomeArrival.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

namespace NeonCityWestLakeMap
{
/** HUD still at the window's own size (-NeonCityArtStills): the map is UI, judged as the player sees it. */
static void Still(UWorld* W,const FString& Name)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))return;
 auto* Client=W?W->GetGameViewport():nullptr;auto Widget=Client?Client->GetGameViewportWidget():nullptr;
 TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
 if(!Widget || !FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(),Pixels,Size) || Pixels.IsEmpty())return;
 for(auto& Pixel:Pixels)Pixel.A=255;
 TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
 FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("UI");
 IFileManager::Get().MakeDirectory(*Folder,true);FFileHelper::SaveArrayToFile(PNG,*(Folder/Name));
}
/** Every 2 m along the route stays on passable map cells (the ends may snap off a doorway / deck). */
static int32 BlockedSamples(const UNeonCityMapData& Map,const TArray<FVector>& Route)
{
 const TArray<uint8>& G=Map.GetGrid();int32 Bad=0;
 for(int32 I=2;I<Route.Num()-1;++I)
 {
  const FVector A=Route[I-1],B=Route[I];const int32 N=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/200));
  for(int32 K=0;K<=N;++K)
  {
   const FVector P=FMath::Lerp(A,B,double(K)/N);
   const int32 X=FMath::RoundToInt((P.X-Map.GridOrigin.X)/Map.GridCell),Y=FMath::RoundToInt((P.Y-Map.GridOrigin.Y)/Map.GridCell);
   if(X<0 || Y<0 || X>=Map.GridWidth || Y>=Map.GridHeight || G[Y*Map.GridWidth+X]==0)++Bad;
  }
 }
 return Bad;
}

/** Normal PIE at the lakeside home: the map asset loads, a waypoint to Jingci temple gets a walking route over the
 * map grid (round the lake, not across it), the minimap and the M map draw (stills with -NeonCityArtStills), and
 * standing at the pin arrives and clears it. */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  const double Now=FPlatformTime::Seconds();
  if(Now-Started>120){Test->AddError(TEXT("Map check timed out"));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;
  APawn* P=PC?PC->GetPawn():nullptr;
  if(!P || W->GetTimeSeconds()<4)return false;
  UNeonCityNavigationComponent* Nav=PC->GetNavigationComponent();
  const UNeonCityMapData* Map=Nav?Nav->GetMapData():nullptr;
  if(Stage==0)
  {
   if(!Test->TestNotNull(TEXT("West Lake map asset with a decoded grid"),Map))return true;
   Test->TestTrue(TEXT("player starts inside the map"),Map->Contains(P->GetActorLocation()));
   const FNeonCityMapPlace* Jingci=Map->Places.FindByPredicate([](const auto& X){return X.PlaceID==TEXT("jingci");});
   if(!Test->TestNotNull(TEXT("Jingci temple place"),Jingci))return true;
   Goal=FVector(Jingci->Location,P->GetActorLocation().Z);
   Test->TestTrue(TEXT("waypoint accepted"),Nav->SetWaypoint(Goal,Jingci->Name,Jingci->PlaceID));
   Stage=1;At=Now;return false;
  }
  const auto& D=Nav->GetDisplay();
  if(Stage==1)
  {
   if(!D.UsingMap && Now-At<15)return false;
   Test->TestTrue(TEXT("walking route planned over the map grid"),D.UsingMap);
   double Length=0;for(int32 I=1;I<D.Route.Num();++I)Length+=FVector::Dist2D(D.Route[I-1],D.Route[I]);
   const double Straight=FVector::Dist2D(P->GetActorLocation(),Goal);
   Test->AddInfo(FString::Printf(TEXT("route %d points, %.0f m (straight %.0f m)"),D.Route.Num(),Length/100,Straight/100));
   Test->TestTrue(TEXT("route goes round the lake (longer than the straight line, not absurdly)"),Length>Straight*1.05 && Length<Straight*3.5);
   const int32 Bad=BlockedSamples(*Map,D.Route);
   Test->TestTrue(FString::Printf(TEXT("route stays on passable ground (%d blocked samples)"),Bad),Bad<=6);
   Still(W,TEXT("WL-Map-Minimap.png"));
   Nav->ToggleWorldMap();Test->TestTrue(TEXT("M map opens"),Nav->IsWorldMapOpen());
   Stage=2;At=Now;return false;
  }
  if(Stage==2)
  {
   if(Now-At<1.5)return false;
   Still(W,TEXT("WL-Map-Full.png"));
   // real pointer input through Slate (the map was hit-test invisible: wheel and drag never reached it)
   if(UNeonCityWorldMapWidget* Open=Nav->GetWorldMapWidget())
   {
    const FGeometry& G=Open->GetCachedGeometry();
    const FVector2D Screen=G.LocalToAbsolute(G.GetLocalSize()*FVector2D(.3,.5));
    const double Before=Open->GetZoom();const FVector2D C0=Open->GetViewCenter();
    FPointerEvent Wheel(0,Screen,Screen,TSet<FKey>(),EKeys::MouseWheelAxis,1.f,FModifierKeysState());
    FSlateApplication::Get().ProcessMouseWheelOrGestureEvent(Wheel,nullptr);
    Test->TestTrue(FString::Printf(TEXT("mouse wheel zooms the open map (%.2f -> %.2f)"),Before,Open->GetZoom()),Open->GetZoom()>Before*1.1);
    FPointerEvent Down(0,Screen,Screen,TSet<FKey>({EKeys::LeftMouseButton}),EKeys::LeftMouseButton,0,FModifierKeysState());
    FSlateApplication::Get().ProcessMouseButtonDownEvent(nullptr,Down);
    const FVector2D To=Screen+FVector2D(120,60);
    FPointerEvent Move(0,To,Screen,TSet<FKey>({EKeys::LeftMouseButton}),EKeys::Invalid,0,FModifierKeysState());
    FSlateApplication::Get().ProcessMouseMoveEvent(Move);
    FPointerEvent Up(0,To,To,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState());
    FSlateApplication::Get().ProcessMouseButtonUpEvent(Up);
    Test->TestTrue(TEXT("dragging pans the open map"),!Open->GetViewCenter().Equals(C0,1e-4));
   }
   Nav->ToggleWorldMap();Test->TestFalse(TEXT("M map closes"),Nav->IsWorldMapOpen());
   // walk the camera view along the route a little for a world-marker still
   P->SetActorLocation(D.Route[FMath::Min(3,D.Route.Num()-1)]+FVector(0,0,150),false,nullptr,ETeleportType::TeleportPhysics);
   Stage=3;At=Now;return false;
  }
  if(Stage==3)
  {
   if(Now-At<3)return false;
   Still(W,TEXT("WL-Map-WorldMarker.png"));
   FHitResult Hit;W->LineTraceSingleByChannel(Hit,Goal+FVector(500,0,8000),Goal+FVector(500,0,-8000),ECC_Visibility);
   P->SetActorLocation((Hit.bBlockingHit?Hit.ImpactPoint:Goal)+FVector(0,0,120),false,nullptr,ETeleportType::TeleportPhysics);
   Stage=4;At=Now;return false;
  }
  if(Stage==4)
  {
   if(Nav->HasWaypoint() && Now-At<5)return false;
   Test->TestFalse(TEXT("arriving at the pin clears it"),Nav->HasWaypoint());
   return true;
  }
  return true;
 }
private:
 FAutomationTestBase* Test;double Started=0,At=0;int32 Stage=0;FVector Goal=FVector::ZeroVector;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeMapPIE,"NeonCity.WestLake.MapPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeMapPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeMap::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}

/** Session start at home: the player is put at the home spawn and owned cars authored in the home's empty garage
 * bays come back there from wherever they were left; a car stored at another property, a wreck and an occupied bay
 * are left alone. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityHomeArrivalTest,"NeonCity.WestLake.HomeArrival",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityHomeArrivalTest::RunTest(const FString&)
{
 const FName Home(TEXT("Property.WestLake.Residence")),Other(TEXT("Property.Other"));
 auto* Defaults=NewObject<UNeonCitySaveGame>();auto* Model=NewObject<UNeonCitySaveGame>();
 const FVector BayA(4000,42000,50),BayB(4300,42000,50),BayC(4600,42000,50);
 auto Car=[](FGuid ID,FVector At){FNeonCityVehicleSaveData V;V.VehicleID=ID;V.Owned=true;V.Transform=FTransform(FRotator(0,40,0),At);return V;};
 const FGuid A=FGuid::NewGuid(),B=FGuid::NewGuid(),C=FGuid::NewGuid();
 Defaults->Vehicles={Car(A,BayA),Car(B,BayB),Car(C,BayC)};
 FNeonCityPropertySaveData P;P.PropertyID=Home;P.Owned=true;
 for(int32 I=0;I<3;++I){FNeonCityGarageSaveData S;S.SlotIndex=I;S.StoredLocation=(I==0?BayA:I==1?BayB:BayC);P.GarageSlots.Add(S);}
 Model->Properties={P};
 auto Driven=Car(A,FVector(90000,-20000,0));                               // left across town: comes home
 auto Stored=Car(B,FVector(50000,0,0));Stored.StoredPropertyID=Other;     // kept at another property: stays
 auto Wreck=Car(C,FVector(70000,0,0));Wreck.Condition=0;                   // wrecked: stays
 Model->Vehicles={Driven,Stored,Wreck};
 Model->Player.Transform=FTransform(FVector(1,2,3));
 const FTransform Spawn(FRotator(0,50,0),FVector(4670,43560,200));
 const int32 Parked=NeonCityHomeArrival::PrepareSessionStart(Home,Spawn,*Defaults,*Model);
 TestEqual(TEXT("one car parked"),Parked,1);
 TestTrue(TEXT("player at the home spawn"),Model->Player.Transform.Equals(Spawn));
 TestTrue(TEXT("driven car back in its bay"),Model->Vehicles[0].Transform.GetLocation().Equals(BayA,1));
 TestEqual(TEXT("bay registered"),Model->Properties[0].GarageSlots[0].VehicleID,A);
 TestTrue(TEXT("stored car untouched"),Model->Vehicles[1].Transform.GetLocation().Equals(FVector(50000,0,0),1));
 TestTrue(TEXT("wreck untouched"),Model->Vehicles[2].Transform.GetLocation().Equals(FVector(70000,0,0),1));
 // a second session start with the car already home changes nothing
 TestEqual(TEXT("idempotent"),NeonCityHomeArrival::PrepareSessionStart(Home,Spawn,*Defaults,*Model),0);
 // unowned home: nothing happens
 Model->Properties[0].Owned=false;Model->Player.Transform=FTransform(FVector(1,2,3));
 TestEqual(TEXT("not owned: no arrival"),NeonCityHomeArrival::PrepareSessionStart(Home,Spawn,*Defaults,*Model),0);
 TestTrue(TEXT("not owned: player stays"),Model->Player.Transform.GetLocation().Equals(FVector(1,2,3)));
 return true;
}
#endif
