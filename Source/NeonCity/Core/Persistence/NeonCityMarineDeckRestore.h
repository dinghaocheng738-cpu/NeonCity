#pragma once
#include "CoreMinimal.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Physics/DynamicSurfaceInteractionInterface.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

/** Read-only pre-load validation against the exact saved boat's real native
 * deck collision. Marine transforms are applied later, so map the saved local
 * standing capsule onto this same boat's current physical geometry. Never move
 * the boat to validate a file, and never classify the whole hull as a floor. */
inline bool NeonCityValidateSavedMarineDeck(UWorld* W,const ACharacter* Player,const FNeonCityMarineSaveData& Saved,const FTransform& Standing)
{
 if(!W || !Player || !Saved.VehicleID.IsValid() || !Saved.Owned || !FMath::IsFinite(Saved.Condition) || Saved.Condition<=0 || !NeonCityPersistence::ValidTransform(Saved.Transform) || !NeonCityPersistence::ValidTransform(Standing))return false;
 ANeonCityMarineVehicleBase* Boat=nullptr;int32 Count=0;for(TActorIterator<ANeonCityMarineVehicleBase> It(W);It;++It)if(It->VehicleID==Saved.VehicleID){Boat=*It;++Count;}
 if(Count!=1 || !Boat || !Boat->Owned || !Boat->GetProfile() || !Boat->GetHealthComponent() || !(Boat->GetProfile()->bWalkableDeck || Boat->GetProfile()->bPreferDeckExit) || Saved.DataAssetID!=Boat->GetProfile()->VehicleType || Boat->GetHealthComponent()->IsDead() || Boat->GetVelocity().Size()>150)return false;
 const FRotator SavedRotation=Saved.Transform.Rotator();if(FMath::Abs(SavedRotation.Roll)>25 || FMath::Abs(SavedRotation.Pitch)>25 || FMath::Abs(Boat->GetActorRotation().Roll)>25 || FMath::Abs(Boat->GetActorRotation().Pitch)>25)return false;
 const float Half=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Player->GetCapsuleComponent()->GetScaledCapsuleRadius();const FVector World=Standing.GetLocation();
 float Water=0;FVector Current;const auto* Marine=W->GetSubsystem<UMarineWorldSubsystem>();if(!Marine || !Marine->SampleWater(World,Water,Current) || World.Z-Half<Water+10)return false;
 const FVector Local=Saved.Transform.InverseTransformPosition(World),Mapped=Boat->GetActorTransform().TransformPosition(Local);
 // A bounded deck point must be above the hull and within the actual finite
 // working vessel; these gates only narrow the physical collision test below.
 const float LongitudinalBound=Boat->GetProfile()->bPreferDeckExit?Boat->GetProfile()->HullExtent.X+50:450;
 if(FMath::Abs(Local.X)>LongitudinalBound || FMath::Abs(Local.Y)>200 || Local.Z<Half-10 || Local.Z>Half+250)return false;
 const FQuat Mapping=Boat->GetActorQuat()*Saved.Transform.GetRotation().Inverse();FCollisionQueryParams Q(SCENE_QUERY_STAT(PersistenceExactMarineDeck),false,Player);
 if(W->OverlapBlockingTestByChannel(Mapped,Mapping,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q))return false;
 FHitResult Floor;const FVector Down=Mapping.RotateVector(FVector::DownVector);if(!W->LineTraceSingleByChannel(Floor,Mapped,Mapped+Down*(Half+8),ECC_Pawn,Q) || Floor.GetActor()!=Boat)return false;
 const float Distance=Floor.Distance-Half;if(Distance<0 || Distance>8)return false;FDynamicStandingSurface Surface;
 if(!NeonCityDynamicSurfaces::Query(Floor,Player,Surface) || Surface.PointVelocity.ContainsNaN() || Surface.PointVelocity.Size()>150)return false;
 const FVector SavedNormal=Mapping.Inverse().RotateVector(Floor.ImpactNormal);if(SavedNormal.Z<Player->GetCharacterMovement()->GetWalkableFloorZ())return false;
 // The floor contract rejects gunwales, cabin roofs and hull side contacts;
 // sweep the full standing capsule down to prove usable physical support.
 FHitResult Sweep;return W->SweepSingleByChannel(Sweep,Mapped,Mapped+Down*10,Mapping,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q) && Sweep.GetActor()==Boat && Sweep.ImpactNormal.Z>=Player->GetCharacterMovement()->GetWalkableFloorZ();
}
