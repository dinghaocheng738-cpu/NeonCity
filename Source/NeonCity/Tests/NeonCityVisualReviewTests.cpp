#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Water/SwimmingAnimInstance.h"
#include "Water/WaterInteractionComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimNode_LinkedAnimGraph.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "Animation/BlendSpace.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UnrealType.h"
#include "Components/TextRenderComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Materials/MaterialInterface.h"
#include "DynamicRHI.h"

namespace NeonCityVisualReview
{
FString OutputDir()
{
    FString Run; FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
    if(Run.IsEmpty()) Run=FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
    Run=FPaths::MakeValidFileName(Run);
    return FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run;
}
void Capture(UWorld* World,const FString& Directory,const FString& Name)
{
    auto* View=World->GetGameViewport(); if(!View || !View->Viewport) return;
    GEngine->AddOnScreenDebugMessage(20523,8,FColor::White,TEXT("NeonCity 20.5 | ")+FPaths::GetCleanFilename(Directory)+TEXT(" | ")+Name);
    View->Viewport->Draw(false); TArray<FColor> Pixels;
    if(!View->Viewport->ReadPixels(Pixels)) return;
    for(auto& Pixel:Pixels) Pixel.A=255;
    const FIntPoint Size=View->Viewport->GetSizeXY(); TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    IFileManager::Get().MakeDirectory(*Directory,true);
    FFileHelper::SaveArrayToFile(PNG,*(Directory/(Name+TEXT(".png"))));
}
FString Assets(UAnimInstance* Instance,int32 Depth=0)
{
    if(!Instance) return TEXT("NONE"); FString Out=Instance->GetClass()->GetPathName();
    auto* Interface=IAnimClassInterface::GetFromClass(Instance->GetClass());
    if(!Interface) return Out;
    for(FStructProperty* Property:Interface->GetAnimNodeProperties())
    {
        if(Property->Struct->IsChildOf(FAnimNode_SequencePlayerBase::StaticStruct()))
        {
            auto* Node=Property->ContainerPtrToValuePtr<FAnimNode_SequencePlayerBase>(Instance);
            if(Node->GetSequence() && Node->GetCachedBlendWeight()>.01f)
                Out+=FString::Printf(TEXT(" | %s weight=%.3f time=%.3f"),*Node->GetSequence()->GetName(),Node->GetCachedBlendWeight(),Node->GetCurrentAssetTime());
        }
        if(Depth<2 && Property->Struct->IsChildOf(FAnimNode_LinkedAnimGraph::StaticStruct()))
        {
            auto* Node=Property->ContainerPtrToValuePtr<FAnimNode_LinkedAnimGraph>(Instance);
            if(auto* Linked=Node->GetDynamicLinkTarget(Instance); Linked && Linked!=Instance) Out+=TEXT(" LINKED: ")+Assets(Linked,Depth+1);
        }
        if(Property->Struct->IsChildOf(FAnimNode_BlendSpacePlayerBase::StaticStruct()))
        {
            auto* Node=Property->ContainerPtrToValuePtr<FAnimNode_BlendSpacePlayerBase>(Instance);
            if(Node->GetBlendSpace() && Node->GetCachedBlendWeight()>.01f)
                Out+=FString::Printf(TEXT(" | %s weight=%.3f time=%.3f"),*Node->GetBlendSpace()->GetName(),Node->GetCachedBlendWeight(),Node->GetCurrentAssetTime());
        }
    }
    return Out;
}
class FReview : public IAutomationLatentCommand
{
public:
    explicit FReview(FAutomationTestBase* InTest):Test(InTest),Dir(OutputDir()) {}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds(); if(!Start) Start=Now;
        const float RealFrameMS=LastFrame>0?float((Now-LastFrame)*1000):0; LastFrame=Now;
        if(Now-Start>160) { Test->AddError(TEXT("Visual review timed out")); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;
        auto* PC=W?W->GetFirstPlayerController():nullptr;
        auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
        if(!P || W->GetTimeSeconds()<2) return false;
        if(Stage==0)
        {
            if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityOriginalArt")))
            {
                FString Json; TSharedPtr<FJsonObject> Root;
                if(!FFileHelper::LoadFileToString(Json,*(FPaths::ProjectDir()/TEXT("ArtSource/SouthBeachVisualOverrides.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root))
                { Test->AddError(TEXT("Original appearance manifest missing"));return true; }
                for(TActorIterator<AActor> It(W);It;++It)
                {
                    if(It->ActorHasTag(TEXT("NeonCity.Visual205"))) It->SetActorHiddenInGame(true);
                    for(const auto& Value:Root->GetArrayField(TEXT("retained_actors")))
                    {
                        const auto Record=Value->AsObject();FString ActorName;Record->GetStringField(TEXT("path")).Split(TEXT("PersistentLevel."),nullptr,&ActorName);
                        if(It->GetName()!=ActorName) continue;
                        if(auto* MeshActor=Cast<AStaticMeshActor>(*It))
                        {
                            auto* Mesh=MeshActor->GetStaticMeshComponent();Mesh->SetVisibility(Record->GetBoolField(TEXT("visible")));
                            const auto& Mats=Record->GetArrayField(TEXT("materials"));
                            for(int32 I=0;I<Mats.Num();++I) Mesh->SetMaterial(I,LoadObject<UMaterialInterface>(nullptr,*Mats[I]->AsString()));
                        }
                        else { TArray<UTextRenderComponent*> Texts;It->GetComponents(Texts);for(auto* Text:Texts) Text->SetVisibility(true); }
                    }
                }
                P->GetMesh()->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")));
            }
            if(auto* View=static_cast<FSceneViewport*>(W->GetGameViewport()->Viewport)) View->SetFixedViewportSize(1600,900);
            GEngine->bEnableOnScreenDebugMessages=true;
            GEngine->bEnableOnScreenDebugMessagesDisplay=true;
            if(FParse::Param(FCommandLine::Get(),TEXT("NeonCitySwimProbe")))
            {
                auto* Class=LoadClass<UAnimInstance>(nullptr,TEXT("/Game/NeonCity/Animations/Swimming/ABP_NC_Swimming_Probe.ABP_NC_Swimming_Probe_C"));
                if(!Class) { Test->AddError(TEXT("Missing swimming probe graph")); return true; }
                P->GetMesh()->SetAnimInstanceClass(Class);
                auto* Probe=W->SpawnActor<AStaticMeshActor>(FVector(-4600,-1000,40),FRotator::ZeroRotator);
                Probe->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
                Probe->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/SouthBeach/Probe/SM_SB205_Probe.SM_SB205_Probe")));
                Probe->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
            for(auto* Var:{TEXT("r.VSync"),TEXT("t.MaxFPS")}) if(auto* C=IConsoleManager::Get().FindConsoleVariable(Var)) C->Set(0,ECVF_SetByConsole);
            if(auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))) C->Set(100,ECVF_SetByConsole);
            for(const TCHAR* Name:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("r.RayTracing"),TEXT("r.EyeAdaptation.CachedLightingPreExposure")})
                if(auto* C=IConsoleManager::Get().FindConsoleVariable(Name)) Log+=FString::Printf(TEXT("%s=%s\n"),Name,*C->GetString());
            for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It) { Clock=*It; Clock->bCycleEnabled=false; Clock->SetTimeOfDay(10); }
            Camera=W->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->FieldOfView=70;
            View(0); PC->SetViewTarget(Camera); Phase=W->GetTimeSeconds(); Stage=1; return false;
        }
        if(Stage>=1 && Stage<=7)
        {
            const float Settle=Stage==4?20.f:(Stage==5?10.f:4.f);
            if(W->GetTimeSeconds()-Phase>Settle-2.5f && W->GetTimeSeconds()-Phase<Settle)
            {
                Frames.Add(RealFrameMS);
                const uint32 GPUCycles=RHIGetGPUFrameCycles();
                if(GPUCycles>0) GPUFrames.Add(FPlatformTime::ToMilliseconds(GPUCycles));
            }
            if(W->GetTimeSeconds()-Phase<Settle) return false;
            const FString Name=FString::Printf(TEXT("Street-%02d"),Stage);
            Capture(W,Dir,Name); Frames.Sort();
            float Sum=0; for(float Value:Frames) Sum+=Value;
            Log+=FString::Printf(TEXT("%s frames=%d mean_ms=%.3f p95_ms=%.3f resolution=%s screenpercentage=100\n"),*Name,Frames.Num(),Frames.IsEmpty()?0:Sum/Frames.Num(),Frames.IsEmpty()?0:Frames[FMath::Min(Frames.Num()-1,FMath::FloorToInt(Frames.Num()*.95f))],*W->GetGameViewport()->Viewport->GetSizeXY().ToString()); Frames.Reset();
            GPUFrames.Sort(); float GPUSum=0;for(float Value:GPUFrames) GPUSum+=Value;
            Log+=FString::Printf(TEXT("%s gpu_samples=%d gpu_mean_ms=%.3f gpu_p95_ms=%.3f\n"),*Name,GPUFrames.Num(),GPUFrames.IsEmpty()?0:GPUSum/GPUFrames.Num(),GPUFrames.IsEmpty()?0:GPUFrames[FMath::Min(GPUFrames.Num()-1,FMath::FloorToInt(GPUFrames.Num()*.95f))]); GPUFrames.Reset();
            ++Stage; Phase=W->GetTimeSeconds();
            if(Stage<=7) { View(Stage-1); if(Clock && (Stage==4 || Stage==5)) Clock->SetTimeOfDay(Stage==4?21:10); return false; }
            if(Clock) Clock->SetTimeOfDay(11);
            P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            P->SetActorLocation(FVector(0,-14500,-220),false,nullptr,ETeleportType::TeleportPhysics);
            P->GetCharacterMovement()->StopMovementImmediately(); P->GetCapsuleComponent()->UpdatePhysicsVolume(true);
            PC->SetControlRotation(FRotator(-12,0,0)); PC->SetViewTarget(P); return false;
        }
        if(Stage==8)
        {
            if(W->GetTimeSeconds()-Phase<2) return false;
            if(FParse::Param(FCommandLine::Get(),TEXT("NeonCitySwimProbe")))
            {
                const FVector Target=P->GetActorLocation();
                Camera->SetActorLocation(Target+FVector(-220,-220,-25));
                Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation()); PC->SetViewTarget(Camera);
            }
            if(Samples>=6) P->DoMove(0,1);
            if(Samples<12 && W->GetTimeSeconds()-SampleTime>.20f)
            {
                auto* Water=P->FindComponentByClass<UWaterInteractionComponent>();
                Log+=FString::Printf(TEXT("Swim sample=%d time=%.3f mode=%d water=%d head=%s mesh=%s skeleton=%s anim=%s\n"),Samples,W->GetTimeSeconds(),int32(P->GetCharacterMovement()->MovementMode),Water?int32(Water->GetWaterState()):-1,*P->GetMesh()->GetSocketLocation(TEXT("head")).ToString(),*GetNameSafe(P->GetMesh()->GetSkeletalMeshAsset()),*GetNameSafe(P->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()),*Assets(P->GetMesh()->GetAnimInstance()));
                Capture(W,Dir,FString::Printf(TEXT("Swim-%03d"),Samples++)); SampleTime=W->GetTimeSeconds();
            }
            if(Samples<12) return false;
            Test->TestTrue(TEXT("Diagnostic is actual native swimming"),P->GetCharacterMovement()->IsSwimming());
            FFileHelper::SaveStringToFile(Log,*(Dir/TEXT("review.log")));
            Test->AddInfo(TEXT("Current-run visual review: ")+Dir); return true;
        }
        return false;
    }
    void View(int32 Index)
    {
        const FVector Locations[]={FVector(-4300,-800,220),FVector(-3300,750,200),FVector(350,-750,240),FVector(-4300,-800,220),FVector(0,-9000,4800),FVector(-6700,1600,1750),FVector(-3450,-950,180)};
        const FVector Targets[]={FVector(-4400,2300,650),FVector(-4400,1550,350),FVector(1400,2400,650),FVector(-4400,2300,650),FVector(0,1500,400),FVector(-4400,2300,1200),FVector(-3200,-1190,95)};
        Camera->SetActorLocation(Locations[Index]); Camera->SetActorRotation((Targets[Index]-Locations[Index]).Rotation());
    }
    FAutomationTestBase* Test; FString Dir,Log; TArray<float> Frames,GPUFrames; ACameraActor* Camera=nullptr; ANeonCityDayNightCycle* Clock=nullptr;
    double Start=0,LastFrame=0; float Phase=0,SampleTime=0; int32 Stage=0,Samples=0;
};
class FStartReviewPIE : public IAutomationLatentCommand
{
public:
    bool Update() override
    {
        auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
        Settings->NewWindowWidth=1600; Settings->NewWindowHeight=900;
        FRequestPlaySessionParams Params; Params.EditorPlaySettings=Settings;
        Params.bAllowOnlineSubsystem=false; GEditor->RequestPlaySession(Params); return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityVisualReview,"NeonCity.Visual.ReferenceReviewPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityVisualReview::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtReview"))) { AddInfo(TEXT("Opt-in continuous captures require -NeonCityArtReview")); return true; }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityVisualReview::FStartReviewPIE>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityVisualReview::FReview>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
