#include "Navigation/NeonCityRouteRibbon.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace NeonCityRouteRibbon
{
// Walking: a chevron every 4 m for 90 m; driving: every 9 m for 320 m. They flow toward the goal at 6 / 18 m/s.
constexpr float WalkStep=400,WalkReach=9000,WalkFlow=600,DriveStep=900,DriveReach=32000,DriveFlow=1800;
constexpr float ArmLength=150,ArmAngle=38,ArmWidth=24,Lift=5;
// The ground is found stepping along the route from the player's feet: each trace starts a little above the
// previous chevron, so tree canopies, eaves and bridges overhead are never mistaken for the path.
constexpr float StepUp=250,StepDown=1200;
}
using namespace NeonCityRouteRibbon;

ANeonCityRouteRibbon::ANeonCityRouteRibbon()
{
 PrimaryActorTick.bCanEverTick=false;SetCanBeDamaged(false);
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RouteRoot")));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowAsset(TEXT("/Game/NeonCity/Art/WeaponFX/MI_NC_RouteGlow.MI_NC_RouteGlow"));
 Cube=CubeAsset.Object;Glow=GlowAsset.Object;
}

UStaticMeshComponent* ANeonCityRouteRibbon::Arm(int32 Index)
{
 while(Arms.Num()<=Index)
 {
  auto* C=NewObject<UStaticMeshComponent>(this);C->SetStaticMesh(Cube);if(Glow)C->SetMaterial(0,Glow);
  C->SetMobility(EComponentMobility::Movable);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);
  C->SetCanEverAffectNavigation(false);C->SetGenerateOverlapEvents(false);C->bReceivesDecals=false;
  C->SetUsingAbsoluteLocation(true);C->SetUsingAbsoluteRotation(true);C->SetUsingAbsoluteScale(true);
  C->SetupAttachment(GetRootComponent());C->RegisterComponent();Arms.Add(C);
 }
 return Arms[Index];
}

bool ANeonCityRouteRibbon::GetArm(int32 Index,FTransform& Out) const
{
 if(Index<0 || Index>=Shown)return false;
 Out=Arms[Index]->GetComponentTransform();return true;
}

void ANeonCityRouteRibbon::Clear()
{
 for(int32 I=0;I<Shown;++I)Arms[I]->SetVisibility(false);
 Shown=0;
}

void ANeonCityRouteRibbon::Present(const TArray<FVector>& Route,const FVector& Origin,bool bDriving,float Time)
{
 int32 Count=0;
 if(Route.Num()>=2)
 {
  // where the player stands along the route
  int32 Seg=1;float Along=0,Best=MAX_flt;
  for(int32 I=1;I<Route.Num();++I)
  {
   const FVector A(Route[I-1].X,Route[I-1].Y,0),B(Route[I].X,Route[I].Y,0);
   const FVector P=FMath::ClosestPointOnSegment(FVector(Origin.X,Origin.Y,0),A,B);const float D=FVector::Dist(P,FVector(Origin.X,Origin.Y,0));
   if(D<Best){Best=D;Seg=I;Along=FVector::Dist(A,P);}
  }
  const float Step=bDriving?DriveStep:WalkStep,Reach=bDriving?DriveReach:WalkReach;
  // the first chevron sits a little ahead of the player, then they march on (the phase flows with time)
  float S=Step-FMath::Fmod(Time*(bDriving?DriveFlow:WalkFlow),Step)+(bDriving?600.f:250.f),Walked=-Along;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(RouteRibbon),false,this);
  if(APlayerController* PC=GetWorld()->GetFirstPlayerController())if(APawn* Pawn=PC->GetPawn())Q.AddIgnoredActor(Pawn);
  float GroundZ=Origin.Z-(bDriving?40.f:90.f);
  for(int32 I=Seg;I<Route.Num() && S<Reach;++I)
  {
   const FVector A(Route[I-1].X,Route[I-1].Y,0),B(Route[I].X,Route[I].Y,0);const float Len=FVector::Dist(A,B);
   if(Len<1){continue;}
   const FVector Dir=(B-A)/Len;
   while(S-Walked<=Len && S<Reach)
   {
    const FVector P=A+Dir*(S-Walked);
    FHitResult Hit;FVector Up=FVector::UpVector;
    if(GetWorld()->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,GroundZ+StepUp),FVector(P.X,P.Y,GroundZ-StepDown),ECC_Visibility,Q))
    {GroundZ=Hit.ImpactPoint.Z;Up=Hit.ImpactNormal;}
    const float Fade=FMath::Clamp((Reach-S)/(Reach*.25f),0.f,1.f);                   // shrink toward the far end
    const FQuat Ground=FQuat::FindBetweenNormals(FVector::UpVector,Up);
    for(float Side:{-1.f,1.f})
    {
     // one arm of the >: from the tip back and out to the side
     const FRotator Yaw=Dir.Rotation()+FRotator(0,180+Side*ArmAngle,0);
     const FVector Centre=FVector(P.X,P.Y,GroundZ+Lift)+Yaw.Vector()*ArmLength*.5f*Fade;
     UStaticMeshComponent* C=Arm(Count++);
     C->SetWorldTransform(FTransform(Ground*Yaw.Quaternion(),Centre,FVector(ArmLength*Fade/100,ArmWidth*FMath::Max(.4f,Fade)/100,.02f)));
     C->SetVisibility(true);
    }
    S+=Step;
   }
   Walked+=Len;
  }
 }
 for(int32 I=Count;I<Shown;++I)Arms[I]->SetVisibility(false);
 Shown=Count;
}
