#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CoastalAmbientArtLibrary.generated.h"
class UAnimBlueprint;
class UAnimSequence;
class UStaticMesh;
class USkeletalMesh;

UCLASS()
class NEONCITYARTEDITOR_API UCoastalAmbientArtLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 /** Append licensed upper-body activities and the production seat loop only
  * to the C921 copied NPC graph. Existing locomotion/combat nodes remain. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Coastal Ambient")
 static bool AddCoastalAmbientLayers(UAnimBlueprint* Blueprint,UAnimSequence* Idle,UAnimSequence* Look,UAnimSequence* Talk,UAnimSequence* Sitting);
 /** Read-only native triangles and actual CPU-skinned seat pose; transient
  * preview actor only, no persistent actor/package mutation or asset save. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Coastal Ambient")
 static FString MeasureCoastalCafeAssets(UStaticMesh* Chair,UStaticMesh* Cup,USkeletalMesh* Skin,UAnimSequence* Sitting);
 /** Only the private Cafe52 clone receives optional real-floor foot IK. The
  * existing C921 animation blueprint and normal input remain untouched. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Coastal Ambient")
 static bool AddGroundedCoastalCafeFeet(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Coastal Ambient")
 static FString GetGroundedCoastalCafeFeetGraphAudit(UAnimBlueprint* Blueprint);
};
