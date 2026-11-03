#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Slate/SceneViewport.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "DynamicRHI.h"
#include "RenderingThread.h"
#include "UnrealClient.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"

/** Same PIE world/game mode, in a dedicated opt-in evidence client instead of
 * the editor's smaller embedded viewport. No persistent editor preference edit. */
class FNeonCityStartVisualPIECommand : public IAutomationLatentCommand
{
public:
    bool Update() override
    {
        if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")) && !FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics")))
            return FStartPIECommand(false).Update();
        FRequestPlaySessionParams Params;
        auto* Settings=DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
        Settings->NewWindowWidth=1600;Settings->NewWindowHeight=900;Settings->CenterNewWindow=true;
        Settings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);Settings->SetPlayNumberOfClients(1);
        Settings->bLaunchSeparateServer=false;Settings->SetRunUnderOneProcess(true);
        Settings->LastExecutedPlayModeType=EPlayModeType::PlayMode_InEditorFloating;
        if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck")))
        {
            Settings->EnableGameSound=true;Settings->SoloAudioInFirstPIEClient=true;
        }
        // Supply an explicitly sized client like the official movie PIE executor.
        // Default PIE placement clamps its window to the host monitor work area;
        // a fixed render target alone then captures a smaller, scaled Slate HUD.
        TSharedRef<SWindow> Window=SNew(SWindow)
            .ClientSize(FVector2D(1600,900)).AutoCenter(EAutoCenter::None)
            .AdjustInitialSizeAndPositionForDPIScale(false).SaneWindowPlacement(false)
            .UseOSWindowBorder(false).CreateTitleBar(false)
            .LayoutBorder(FMargin(0)).UserResizeBorder(FMargin(0))
            .FocusWhenFirstShown(false).ActivationPolicy(EWindowActivationPolicy::Never)
            .SupportsMaximize(false).SupportsMinimize(false)
            .SizingRule(ESizingRule::FixedSize);
        Window->SetAcceptsInput(false);
        // Slate must show/layout the client to composite UMG. RenderOffscreen's
        // platform window remains virtual; no foreground native window is used.
        FSlateApplication::Get().AddWindow(Window,true);
        Params.EditorPlaySettings=Settings;Params.DestinationSlateViewport=nullptr;
        Params.CustomPIEWindow=Window;Params.bAllowOnlineSubsystem=false;
        GEditor->RequestPlaySession(Params);return true;
    }
};

/** Opt-in frame recording of real PIE. Never changes actor motion or simulation.
 * Images are sampled at 12Hz game time; timestamps retain actual wall time.
 * Performance comparison uses the separate review without continuous readback. */
class FNeonCityVisualEvidence
{
public:
    explicit FNeonCityVisualEvidence(const FString& InSequence,bool InSlateSource=false):Sequence(InSequence),bSlateSource(InSlateSource) {}
    bool HasWriteError()const{return bWriteError;}
    /** Bounded rendered evidence using the valid offscreen Slate surface and
     * fast JPEG encoding. Never changes render policy or simulation. */
    static bool CaptureRenderedJPEG(UWorld* World,const FString& Name)
    {
        if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))return false;
        auto* Client=World?World->GetGameViewport():nullptr;
        auto Widget=Client?Client->GetGameViewportWidget():nullptr;
        TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
        if(!Widget || !FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(),Pixels,Size)
            || Size.X!=1600 || Size.Y!=900 || Pixels.Num()!=1600*900)return false;
        int32 NonBlack=0;for(const auto& Pixel:Pixels)if(Pixel.R || Pixel.G || Pixel.B)++NonBlack;
        if(NonBlack<32)return false;
        for(auto& Pixel:Pixels)Pixel.A=255;
        TArray64<uint8> Encoded;if(!FImageUtils::CompressImage(Encoded,TEXT("jpg"),FImageView(Pixels.GetData(),1600,900),95))return false;
        FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("UI");
        IFileManager::Get().MakeDirectory(*Folder,true);
        return FFileHelper::SaveArrayToFile(Encoded,*(Folder/FPaths::MakeValidFileName(Name)));
    }
    static bool CaptureHUD(UWorld* World,const FString& Name)
    {
        // Continuous motion reviews can explicitly omit synchronous PNG/Slate
        // readback. Those stalls disturb a physics-driven moving deck; the
        // JPEG sequence still records unmodified PIE with real timestamps.
        if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtSkipHUD")))return true;
        if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion"))
            && !FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))return true;
        auto* Client=World?World->GetGameViewport():nullptr;auto Widget=Client?Client->GetGameViewportWidget():nullptr;
        if(Widget)
            if(auto Window=FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()))
                UE_LOG(LogTemp,Display,TEXT("HUD surface diagnostic: client %s, draw %s, local %s"),
                    *FVector2D(Window->GetClientSizeInScreen()).ToString(),*FVector2D(Widget->GetCachedGeometry().GetDrawSize()).ToString(),*FVector2D(Widget->GetCachedGeometry().GetLocalSize()).ToString());
        TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
        if(!Widget || !FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(),Pixels,Size) || Pixels.IsEmpty())return false;
        if(Size.X!=1600 || Size.Y!=900)
        {
            UE_LOG(LogTemp,Error,TEXT("HUD evidence resolution mismatch: %d x %d"),Size.X,Size.Y);
            return false;
        }
        for(auto& Pixel:Pixels)Pixel.A=255;
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
        FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("UI");
        IFileManager::Get().MakeDirectory(*Folder,true);
        UE_LOG(LogTemp,Display,TEXT("Actual HUD capture %s: %d x %d"),*Name,Size.X,Size.Y);
        return FFileHelper::SaveArrayToFile(PNG,*(Folder/FPaths::MakeValidFileName(Name)));
    }
    void Record(UWorld* World,APlayerController* PC,int32 Stage)
    {
        const bool Capture=FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion"));
        const bool Metrics=FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics"));
        if((!Capture && !Metrics) || !World || !PC) return;
        auto* Client=World->GetGameViewport(); if(!Client || !Client->Viewport) return;
        const float Now=World->GetTimeSeconds();
        if(Directory.IsEmpty())
        {
            FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
            Run=FPaths::MakeValidFileName(Run.IsEmpty()?FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")):Run);
            Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/(Capture?TEXT("Motion"):TEXT("Performance"))/Sequence;
            IFileManager::Get().MakeDirectory(*Directory,true);
            // Existing motion clients retain their fixed render target. The
            // standalone Slate source already has an exact sized window.
            if(!bSlateSource)
                static_cast<FSceneViewport*>(Client->Viewport)->SetFixedViewportSize(1600,900);
            for(const TCHAR* Name:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality")})
                if(auto* C=IConsoleManager::Get().FindConsoleVariable(Name)) C->Set(3,ECVF_SetByConsole);
            PC->ConsoleCommand(TEXT("r.ScreenPercentage 100"));
            if(!Client->IsStatEnabled(TEXT("Unit"))) PC->ConsoleCommand(TEXT("stat unit"));
            FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,wall_ms,gpu_ms,game_thread_ms,render_thread_ms\n"),*(Directory/TEXT("performance.csv")));
            FFileHelper::SaveStringToFile(TEXT("frame,game_seconds,wall_seconds,stage,pawn,movement_mode,x,y,z\n"),*(Directory/TEXT("frames.csv")));
            Start=PreviousFrame=FPlatformTime::Seconds(); Last=Now; return;
        }
        const double WallNow=FPlatformTime::Seconds(),FrameMs=(WallNow-PreviousFrame)*1000;PreviousFrame=WallNow;
        if(Now-Last<1.f/12.f) return;
        Last=Now;
        const auto* Unit=Client->GetStatUnitData();
        const FString Metric=FString::Printf(TEXT("%.4f,%d,%.4f,%.4f,%.4f,%.4f\n"),Now,Stage,FrameMs,FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),Unit?Unit->GameThreadTime:-1,Unit?Unit->RenderThreadTime:-1);
        FFileHelper::SaveStringToFile(Metric,*(Directory/TEXT("performance.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
        if(!Capture) return;
        TArray<FColor> Pixels;FIntPoint Size=Client->Viewport->GetSizeXY();
        if(bSlateSource)
        {
            // Offscreen standalone back buffers are owned by Slate. Use the
            // same real surface as the successful fixed reference capture.
            auto Widget=Client->GetGameViewportWidget();FIntVector ActualSize;
            if(!Widget || !FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(),Pixels,ActualSize))return;
            Size=FIntPoint(ActualSize.X,ActualSize.Y);
        }
        else
        {
            Client->Viewport->Draw(false);
            // FSceneViewport exposes different textures on the two threads.
            // Guard where the official FRenderTarget::ReadPixels reads it.
            ENQUEUE_RENDER_COMMAND(NeonCityEvidenceReadback)(
                [Viewport=Client->Viewport,Size,&Pixels](FRHICommandListImmediate& RHICmdList)
                {
                    const FTextureRHIRef& Texture=Viewport->GetShaderResourceTexture();
                    if(Texture.IsValid())RHICmdList.ReadSurfaceData(Texture,FIntRect(0,0,Size.X,Size.Y),Pixels,FReadSurfaceDataFlags());
                });
            FlushRenderingCommands();
        }
        if(Size!=FIntPoint(1600,900) || Pixels.Num()!=Size.X*Size.Y)return;
        int32 NonBlack=0;for(const auto& Pixel:Pixels)if(Pixel.R || Pixel.G || Pixel.B)++NonBlack;
        if(NonBlack<32)return;
        for(auto& Pixel:Pixels) Pixel.A=255;
        // JPEG avoids PNG's long synchronous deflate stalls affecting Chaos.
        // Fixed resolution and CSV timing remain explicit; stills use lossless PNG.
        TArray64<uint8> Encoded;
        if(!FImageUtils::CompressImage(Encoded,TEXT("jpg"),FImageView(Pixels.GetData(),Size.X,Size.Y),92)) return;
        const FString Name=FString::Printf(TEXT("%s-%s-%05d.jpg"),*Run,*Sequence,Frame);
        if(!FFileHelper::SaveArrayToFile(Encoded,*(Directory/Name))){bWriteError=true;return;}
        APawn* Pawn=PC->GetPawn(); const auto* C=Cast<ACharacter>(Pawn); const FVector P=Pawn?Pawn->GetActorLocation():FVector::ZeroVector;
        const FString Line=FString::Printf(TEXT("%d,%.4f,%.4f,%d,%s,%d,%.3f,%.3f,%.3f\n"),Frame,Now,FPlatformTime::Seconds()-Start,Stage,*GetNameSafe(Pawn),C?int32(C->GetCharacterMovement()->MovementMode):-1,P.X,P.Y,P.Z);
        if(!FFileHelper::SaveStringToFile(Line,*(Directory/TEXT("frames.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append)){bWriteError=true;return;}
        ++Frame;
    }
private:
    FString Sequence,Directory,Run; int32 Frame=0; float Last=0; double Start=0,PreviousFrame=0;
    bool bSlateSource=false,bWriteError=false;
};
#endif
