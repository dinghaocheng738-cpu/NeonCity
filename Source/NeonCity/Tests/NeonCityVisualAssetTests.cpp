#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "PhysicsEngine/BodySetup.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "Water/SwimmingAnimInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisualAssetDimensions,"NeonCity.Visual.AssetDimensions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVisualAssetDimensions::RunTest(const FString&)
{
    struct FExpected { const TCHAR* Folder; const TCHAR* Name; FVector Size; };
    const FExpected Values[]={
        {TEXT("Buildings"),TEXT("CoralCourt"),FVector(1780,1840,1665)},
        {TEXT("Buildings"),TEXT("SeabreezeWorks"),FVector(1800,2250,1521.65)},
        {TEXT("Buildings"),TEXT("EntryPortal"),FVector(320,195,341)},
        {TEXT("Props"),TEXT("Bench"),FVector(205,65.99,93.25)},
        {TEXT("Props"),TEXT("StreetLamp"),FVector(42,161,555)},
        {TEXT("Streets"),TEXT("Paving4m"),FVector(399.4,299.4,3)}};
    for(const auto& V:Values)
    {
        const FString Path=FString::Printf(TEXT("/Game/NeonCity/Art/SouthBeach/%s/SM_SB205_%s.SM_SB205_%s"),V.Folder,V.Name,V.Name);
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);
        if(!TestNotNull(Path,Mesh)) continue;
        TestTrue(FString(V.Name)+TEXT(" metre/centimetre dimensions"),(Mesh->GetBounds().BoxExtent*2).Equals(V.Size,2));
        TestTrue(FString(V.Name)+TEXT(" no duplicate simple collision"),Mesh->GetBodySetup() && Mesh->GetBodySetup()->AggGeom.GetElementCount()==0);
        for(int32 I=0;I<Mesh->GetStaticMaterials().Num();++I) TestNotNull(TEXT("All material slots resolve"),Mesh->GetMaterial(I));
        if(FString(V.Folder)==TEXT("Props")) TestTrue(TEXT("Furniture has distance LODs"),Mesh->GetNumLODs()==3);
    }
    for(const TCHAR* Suffix:{TEXT("BC"),TEXT("R"),TEXT("N")})
    {
        auto* T=LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/SouthBeach/Textures/T_SB_Stucco_%s.T_SB_Stucco_%s"),Suffix,Suffix));
        if(TestNotNull(TEXT("PBR texture exists"),T)) TestEqual(TEXT("Colour space is explicit"),bool(T->SRGB),FString(Suffix)==TEXT("BC"));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwimmingAnimationAssets,"NeonCity.Visual.SwimmingAnimationAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSwimmingAnimationAssets::RunTest(const FString&)
{
    auto* Skeleton=LoadObject<USkeleton>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"));
    for(const TCHAR* Name:{TEXT("Tread"),TEXT("Forward"),TEXT("Underwater"),TEXT("Ascend"),TEXT("Descend")})
    {
        auto* Sequence=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Animations/Swimming/AN_NC_Swim_%s.AN_NC_Swim_%s"),Name,Name));
        if(!TestNotNull(Name,Sequence)) continue;
        TestTrue(TEXT("Uses actual player skeleton"),Sequence->GetSkeleton()==Skeleton);
        TestFalse(TEXT("No duplicate root displacement"),Sequence->bEnableRootMotion);
        TestTrue(TEXT("Genuine sampled 2.4 second clip"),Sequence->GetNumberOfSampledKeys()>=73 && FMath::IsNearlyEqual(Sequence->GetPlayLength(),2.4f,.01f));
        const auto* Model=Sequence->GetDataModel(); if(!TestNotNull(TEXT("Editable bone data"),Model)) continue;
        TArray<FName> Names; Model->GetBoneTrackNames(Names);
        for(const TCHAR* Bone:{TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("calf_l"),TEXT("calf_r"),TEXT("pelvis")})
        {
            if(!TestTrue(FString(TEXT("Animated bone "))+Bone,Names.Contains(Bone))) continue;
            const FTransform Start=Model->GetBoneTrackTransform(Bone,FFrameNumber(0));
            const FTransform End=Model->GetBoneTrackTransform(Bone,FFrameNumber(Model->GetNumberOfKeys()-1));
            TestTrue(FString(Name)+TEXT(" seamless bone endpoints ")+Bone,Start.Equals(End,.02));
            float Change=0;
            for(int32 Key=0;Key<Model->GetNumberOfKeys();Key+=6)
                Change=FMath::Max(Change,float(Start.GetRotation().AngularDistance(Model->GetBoneTrackTransform(Bone,FFrameNumber(Key)).GetRotation())));
            TestTrue(FString(Name)+TEXT(" coordinated motion ")+Bone,Change>.025f);
        }
        if(Names.Contains(TEXT("root")))
        {
            const auto Root=Model->GetBoneTrackTransform(TEXT("root"),FFrameNumber(0));
            for(int32 Key=0;Key<Model->GetNumberOfKeys();Key+=6)
                TestTrue(TEXT("Root remains stationary"),Root.Equals(Model->GetBoneTrackTransform(TEXT("root"),FFrameNumber(Key)),.01));
        }
    }
    auto* Class=LoadClass<UAnimInstance>(nullptr,TEXT("/Game/NeonCity/Animations/Swimming/ABP_NC_Swimming.ABP_NC_Swimming_C"));
    TestTrue(TEXT("Authored animation graph uses swimming presentation model"),Class && Class->IsChildOf(USwimmingAnimInstance::StaticClass()));
    return true;
}
#endif
