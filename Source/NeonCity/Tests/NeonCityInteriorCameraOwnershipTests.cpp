#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorCameraOwnership,"NeonCity.Camera.InteriorOverlapOwnership",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInteriorCameraOwnership::RunTest(const FString&)
{
 UWorld* W=FAutomationEditorCommonUtils::CreateNewMap();auto* PlayerClass=LoadClass<ANeonCityPlayerCharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));if(!TestNotNull(TEXT("Existing concrete player Blueprint"),PlayerClass))return false;auto* P=W->SpawnActor<ANeonCityPlayerCharacter>(PlayerClass);if(!TestNotNull(TEXT("Concrete player fixture"),P))return false;auto* A=W->SpawnActor<AActor>();auto* B=W->SpawnActor<AActor>();auto* C=P->GetCameraExtensionComponent();if(!TestNotNull(TEXT("Existing camera extension"),C))return false;
 auto* Boom=P->GetCameraBoom();Boom->TargetArmLength=400;P->GetMesh()->SetOwnerNoSee(false);
 C->ApplyInteriorCamera(A,P,180,1);C->ApplyInteriorCamera(B,P,160,1);
 TestTrue(TEXT("Leaving first overlapping room keeps the second policy"),C->ReleaseInteriorCamera(A));
 C->ApplyInteriorCamera(B,P,160,1);TestEqual(TEXT("Second room remains close"),Boom->TargetArmLength,160.f);
 TestTrue(TEXT("Final room restores shared baseline"),C->ReleaseInteriorCamera(B));TestEqual(TEXT("Outdoor arm never captures inner-room length"),Boom->TargetArmLength,400.f);TestFalse(TEXT("Outdoor owner visibility restored"),bool(P->GetMesh()->bOwnerNoSee));
 // Reverse exit order must retain the original400cm, too.
 C->ApplyInteriorCamera(A,P,180,1);C->ApplyInteriorCamera(B,P,160,1);C->ReleaseInteriorCamera(B);C->ReleaseInteriorCamera(A);TestEqual(TEXT("Reverse overlap exit restores outdoor arm"),Boom->TargetArmLength,400.f);
 P->GetMesh()->SetOwnerNoSee(true);C->ApplyInteriorCamera(A,P,180,1);C->ReleaseInteriorCamera(A);TestTrue(TEXT("Pre-existing owner visibility is preserved"),bool(P->GetMesh()->bOwnerNoSee));
 P->Destroy();A->Destroy();B->Destroy();return true;
}
#endif
