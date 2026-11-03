#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#include "TextureCompiler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityOceanFoamAssetTest, "NeonCity.Tidewater.OceanFoamAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNeonCityOceanFoamAssetTest::RunTest(const FString&)
{
    auto* Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/NeonCity/Art/Tidewater/Ocean/T_NCTW_OceanFoam.T_NCTW_OceanFoam"));
    if (!TestNotNull(TEXT("Original foam native package"), Texture)) return false;
    // Editor audit only: finish the actual async compiler, including placeholder replacement.
    FTextureCompilingManager::Get().FinishCompilation({Texture});
    TestFalse(TEXT("Linear data, no gamma conversion"), Texture->SRGB);
    TestFalse(TEXT("Streak alpha retained"), Texture->CompressionNoAlpha);
    TestEqual(TEXT("All original mip levels retained"), int32(Texture->MipGenSettings), int32(TMGS_LeaveExistingMips));
    TestEqual(TEXT("Native runtime half-float format"), int32(Texture->GetPixelFormat()), int32(PF_FloatRGBA));
    TestEqual(TEXT("Native runtime mip count"), Texture->GetNumMips(), 11);
    TestEqual(TEXT("Source width"), Texture->Source.GetSizeX(), int64(1024));
    TestEqual(TEXT("Source height"), Texture->Source.GetSizeY(), int64(1024));
    TestEqual(TEXT("Source mip count"), Texture->Source.GetNumMips(), 11);
    TestEqual(TEXT("Source half-float format"), Texture->Source.GetFormat(), TSF_RGBA16F);
    TArray<uint8> DDS;
    if (!FFileHelper::LoadFileToArray(DDS, *(FPaths::ProjectDir() / TEXT("ArtSource/VerticalSlice/Tidewater/OceanFoam/T_NCTW_OceanFoam.dds"))))
    { AddError(TEXT("Original GPU DDS fixture missing")); return false; }
    int64 Offset = 148;
    for (int32 Mip = 0; Mip < 11; ++Mip)
    {
        const int64 Size = int64(1024 >> Mip) * (1024 >> Mip) * 8;
        TArray64<uint8> Bytes;
        if (!TestTrue(TEXT("Native source mip readable"), Texture->Source.GetMipData(Bytes, Mip))) return false;
        if (!TestEqual(TEXT("Native source mip bytes"), Bytes.Num(), Size) || DDS.Num() < Offset + Size) return false;
        TestEqual(FString::Printf(TEXT("Mip %d: every original GPU half-float channel bit"), Mip),
            FMemory::Memcmp(Bytes.GetData(), DDS.GetData() + Offset, Size), 0);
        Offset += Size;
    }
    TestEqual(TEXT("Entire original source chain compared"), Offset, int64(DDS.Num()));
    return !HasAnyErrors();
}
#endif
