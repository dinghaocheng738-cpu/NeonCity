#include "Police/PoliceObservation.h"
#include "Water/WaterInteractionComponent.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
bool NeonCityObservation::Visible(UWorld* W,AActor* Observer,FVector Eye,FVector Forward,float Range,float Angle,AActor* Target)
{
 if(!IsValid(Target) || Target->IsHidden() || Target->GetWorld()!=W || Eye.ContainsNaN() || !FMath::IsFinite(Range) || !FMath::IsFinite(Angle)) return false;
 if(UWaterInteractionComponent::IsSubmergedActor(Target)) return false;
 const FVector Point=Target->GetActorLocation()+FVector(0,0,45),Delta=Point-Eye;
 if(Delta.SizeSquared()>FMath::Square(FMath::Clamp(Range,100.f,5000.f)) || FVector::DotProduct(Delta.GetSafeNormal(),Forward.GetSafeNormal())<FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Angle,10.f,120.f)))) return false;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(PoliceObservation),false,Observer); Q.AddIgnoredActor(Target);
 return !W->LineTraceTestByChannel(Eye,Point,ECC_Visibility,Q);
}
FSuspectDescriptor NeonCityObservation::Capture(AActor* Candidate,EPoliceEvidenceSource Source)
{
 FSuspectDescriptor D; if(!IsValid(Candidate)) return D;
 D.LastKnownLocation=Candidate->GetActorLocation(); D.LastSeenDirection=Candidate->GetVelocity().GetSafeNormal2D();
 if(D.LastSeenDirection.IsNearlyZero()) D.LastSeenDirection=Candidate->GetActorForwardVector();
 D.LastSeenTime=D.DescriptionTimestamp=Candidate->GetWorld()->GetTimeSeconds(); D.Confidence=1; D.Source=Source;
 if(const auto* V=Cast<ANeonCityBaseVehicle>(Candidate))
 {
  D.InVehicle=true; D.Vehicle.VehicleID=V->VehicleID;
  if(const auto* Data=V->GetVehicleProfile()) D.Vehicle.VehicleType=Data->VehicleType;
  const FName Appearances[]={TEXT("Blue.Sedan"),TEXT("Coral.Sports"),TEXT("Ivory.SUV"),TEXT("Teal.Pickup")};
  D.Vehicle.AppearanceTag=Appearances[FMath::Clamp(int32(D.Vehicle.VehicleType),0,3)];
 }
 else if(Cast<ANeonCityPlayerCharacter>(Candidate)) { D.CharacterAppearance=TEXT("NeonCity.Player.Default"); D.ClothingDescription=TEXT("Prototype.Outfit"); }
 return D;
}
bool NeonCityObservation::Matches(AActor* Candidate,const FSuspectDescriptor& Known)
{
 if(!Known.IsValid()) return false;
 if(auto* V=Cast<ANeonCityBaseVehicle>(Candidate)) return Known.Vehicle.IsKnown() && V->VehicleID==Known.Vehicle.VehicleID;
 return Cast<ANeonCityPlayerCharacter>(Candidate) && !Known.CharacterAppearance.IsNone();
}
bool NeonCityObservation::SafeDeployment(UWorld* W,FVector P,float MinimumDistance)
{
 if(P.ContainsNaN()) return false;
 const auto* PC=W?W->GetFirstPlayerController():nullptr; const auto* Camera=PC?PC->PlayerCameraManager.Get():nullptr;
 if(!Camera) return false;
 const FVector Eye=Camera->GetCameraLocation(),Delta=P-Eye;
 if(Delta.SizeSquared()<FMath::Square(MinimumDistance)) return false;
 if(FVector::DotProduct(Delta.GetSafeNormal(),Camera->GetCameraRotation().Vector())>.2f)
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(PoliceSpawnVisibility),false,PC->GetPawn());
  if(!W->LineTraceTestByChannel(Eye,P+FVector(0,0,80),ECC_Visibility,Q)) return false;
 }
 return true;
}
