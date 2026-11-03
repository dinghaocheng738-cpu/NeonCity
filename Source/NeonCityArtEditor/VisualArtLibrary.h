#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VisualArtLibrary.generated.h"
class UAnimBlueprint;
class UAnimSequence;
class UStaticMesh;
class USkeletalMesh;
/** Supported editor authoring, never runtime asset mutation. */
UCLASS()
class NEONCITYARTEDITOR_API UVisualArtLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static bool AddDivingGearTransition(UAnimBlueprint* Blueprint,UAnimSequence* Clip);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static bool AddDivingPresentation(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static bool AddOceanLocomotionR2(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static bool CorrectOceanLocomotionR3(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static bool BuildDiveHandsGraph(UAnimBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Art")
    static bool ConfigureHeroGarmentLODSettings(USkeletalMesh* Mesh);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Art")
    static bool BuildSwimmingGraph(UAnimBlueprint* Blueprint, bool bProbeOnly=false);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Art")
    static bool BuildCharacterPresentationGraph(UAnimBlueprint* Blueprint, bool bNPC=false);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Art")
    static bool BuildTransportOccupantGraph(UAnimBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Art")
    static bool AddWeaponSupportContact(UAnimBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Art")
    static bool AddFishingHandContact(UAnimBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean")
    static bool AddFishingMotionR2(UAnimBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Art")
    static bool SeparateFishingBodyWeight(UAnimBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean")
    static bool AddFishingWristContact(UAnimBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Art")
    static bool AddCookingBodyContact(UAnimBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Art")
    static bool AddFlashlightCarryPose(UAnimBlueprint* Blueprint,UAnimSequence* CarryPose);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Art")
    static TArray<FVector> GetStaticMeshVertices(UStaticMesh* Mesh);
    UFUNCTION(BlueprintCallable,Category="NeonCity|Art")
    static FString DescribeAnimationGraph(UAnimBlueprint* Blueprint);
};
