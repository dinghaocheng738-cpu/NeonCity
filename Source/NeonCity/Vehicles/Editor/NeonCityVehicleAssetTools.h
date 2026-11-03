#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NeonCityVehicleAssetTools.generated.h"
class UIslandSupercarPresentationData;

/** Editor asset-generation utility; it has no runtime vehicle behavior. */
UCLASS(meta=(DisplayName="NeonCity Vehicle Asset Tools (Editor Only)"))
class NEONCITY_API UNeonCityVehicleAssetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Duplicate the official proxy and update only the project-owned sedan chassis. Game builds return false. */
	UFUNCTION(BlueprintCallable, Category="NeonCity|Editor|Vehicles",
		meta=(DevelopmentOnly, DisplayName="Build Starter Sedan Physics (Editor Only)"))
	static bool BuildStarterSedanPhysics();
	UFUNCTION(BlueprintCallable, Category="NeonCity|Editor|Vehicles", meta=(DevelopmentOnly))
	static bool BuildIslandOffroadPhysics(bool Pickup);
	UFUNCTION(BlueprintCallable, Category="NeonCity|Editor|Vehicles", meta=(DevelopmentOnly))
	static bool BuildIslandSupercarPhysics(bool Roadster);
 UFUNCTION(BlueprintCallable, Category="NeonCity|Editor|Vehicles", meta=(DevelopmentOnly))
 static bool BuildNacrePhysics();
 UFUNCTION(BlueprintCallable, Category="NeonCity|Editor|Vehicles", meta=(DevelopmentOnly))
 static bool BuildReplicaChassis(UIslandSupercarPresentationData* Presentation);
 UFUNCTION(BlueprintCallable, Category="NeonCity|Editor|Vehicles", meta=(DevelopmentOnly))
 static bool RebuildAuthoredVehicle(AActor* Actor);
};
