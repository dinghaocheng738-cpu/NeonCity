#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerController.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Economy/CurrencyComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Phone/PhoneComponent.h"
#include "Phone/PhoneSubsystem.h"
#include "Phone/MessageSystem.h"
#include "Missions/Prototype/NeonCityTransportTest.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace NeonCityDeliveryTests
{
class FVerify : public IAutomationLatentCommand
{
public:
	explicit FVerify(FAutomationTestBase* InTest):Test(InTest) {}
	bool Update() override
	{
		if (!Started) { Started=FPlatformTime::Seconds(); }
		if (FPlatformTime::Seconds()-Started>120) { Test->AddError(TEXT("Phone delivery PIE timed out")); return true; }
		UWorld* World=GEditor ? GEditor->PlayWorld : nullptr;
		auto* PC=World ? Cast<ANeonCityPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!PC || !PC->GetPawn() || !World->HasBegunPlay()) { return false; }
		auto* E=World->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
		auto* Inbox=World->GetGameInstance()->GetSubsystem<UMessageSystem>();
		auto* Phone=PC->GetPhoneComponent();
		float Now=World->GetTimeSeconds();
		auto Next=[&](int32 Value) { Stage=Value; Phase=Now; return false; };
		if (Stage==0)
		{
			Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
			if (!Player.IsValid() || !Player->GetCharacterMovement()->IsMovingOnGround()) { return false; }
			for (TActorIterator<ANeonCityTransportTest> It(World); It; ++It) { Contract=*It; }
			if (!Test->TestNotNull(TEXT("Authored delivery exists"),Contract.Get())) { return true; }
			Car=Contract->DeliveryVehicle;
			if (!Test->TestNotNull(TEXT("Assigned sedan exists"),Car.Get())) { return true; }
			Test->TestEqual(TEXT("Initial balance 1850"),E->GetBalance(),int64(1850));
			Test->TestEqual(TEXT("Currency adapter agrees"),Player->GetCurrencyComponent()->GetBalance(),int64(1850));
			Test->TestEqual(TEXT("Legacy getter delegates to wallet"),Player->GetPlayerDataComponent()->GetMoney(),int64(1850));
			Test->TestEqual(TEXT("One starting funds receipt"),E->GetTransactions().Num(),1);
			if (!Test->TestTrue(TEXT("NPC message received"),Inbox->GetMessages().Num()==1)) { return true; }
			Test->TestEqual(TEXT("Sender is Marcus"),Inbox->GetMessages().Last().Sender,FName(TEXT("Marcus")));
			Test->TestTrue(TEXT("Message content and time populated"),!Inbox->GetMessages().Last().Content.IsEmpty() && Inbox->GetMessages().Last().Timestamp.GetTicks()>0);
			Test->TestFalse(TEXT("Closed phone cannot accept accidentally"),Phone->AcceptLatestMessage());
			Car->SetActorLocation(Contract->GetActorLocation()+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);
			return Next(1);
		}
		if (Stage==1 && Now-Phase>1.5f)
		{
			Test->TestTrue(TEXT("Arrival before acceptance gives no reward"),Contract->GetState()==ENeonCityTransportState::Offered && E->GetBalance()==1850);
			Key(PC,EKeys::P,IE_Pressed); return Next(2);
		}
		if (Stage==2 && Now-Phase>0.3f)
		{
			Key(PC,EKeys::P,IE_Released);
			if (!Test->TestTrue(TEXT("P opens actual phone"),Phone->IsPhoneOpen())) { return true; }
			if (Capture()) { Shot(TEXT("Phone-Offer.png")); }
			return Next(3);
		}
		if (Stage==3 && Now-Phase>0.5f && !FScreenshotRequest::IsScreenshotRequested())
		{
			Key(PC,EKeys::Enter,IE_Pressed); return Next(4);
		}
		if (Stage==4 && Now-Phase>0.3f)
		{
			Key(PC,EKeys::Enter,IE_Released);
			if (!Test->TestTrue(TEXT("Enter accepts Marcus offer"),Contract->GetState()==ENeonCityTransportState::Accepted)) { return true; }
			Test->TestFalse(TEXT("Repeated acceptance rejected"),Phone->AcceptLatestMessage());
			return Next(5);
		}
		if (Stage==5 && Now-Phase>1.5f)
		{
			Test->TestEqual(TEXT("Unoccupied assigned car in zone does not pay"),E->GetBalance(),int64(1850));
			// Fixture places the car on the delivery approach; final approach uses real Chaos input.
			Car->SetActorLocationAndRotation(Contract->GetActorLocation()+FVector(1200,0,149),FRotator(0,180,0),false,nullptr,ETeleportType::TeleportPhysics);
			Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			return Next(6);
		}
		if (Stage==6 && Now-Phase>1.2f)
		{
			FVector Side=Car->GetActorLocation()+Car->GetActorRightVector()*-210;
			Side.Z=106;
			Player->SetActorLocation(Side,false,nullptr,ETeleportType::TeleportPhysics);
			Player->GetCharacterMovement()->StopMovementImmediately();
			PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,90)-Player->GetPawnViewLocation()).Rotation());
			return Next(7);
		}
		if (Stage==7 && Now-Phase>0.3f) { Key(PC,EKeys::F,IE_Pressed); return Next(8); }
		if (Stage==8 && Now-Phase>0.3f)
		{
			Key(PC,EKeys::F,IE_Released);
			if (!Test->TestTrue(TEXT("F possesses assigned car"),PC->GetPawn()==Car.Get())) { return true; }
			Test->TestTrue(TEXT("Phone survives possession"),Phone->IsPhoneOpen());
			DriveOrigin=Car->GetActorLocation();
			Key(PC,EKeys::W,IE_Pressed); return Next(9);
		}
		if (Stage==9)
		{
			float Offset=Car->GetActorLocation().X-Contract->GetActorLocation().X;
			if (Offset<450 && Car->GetVelocity().Size()>80)
			{
				SawSpeedGate=true;
				Test->TestTrue(TEXT("Fast arrival is not completion"),Contract->GetState()==ENeonCityTransportState::Accepted);
			}
			if (Offset>250)
			{
				if (Now-Phase>8) { Test->AddError(TEXT("Physical delivery approach did not reach zone")); return true; }
				return false;
			}
			Key(PC,EKeys::W,IE_Released); Key(PC,EKeys::SpaceBar,IE_Pressed);
			return Next(10);
		}
		if (Stage==10 && Now-Phase>2.5f)
		{
			Test->TestTrue(TEXT("Approach physically drives more than 8 metres"),FVector::Dist2D(DriveOrigin,Car->GetActorLocation())>800);
			Test->TestTrue(TEXT("Moving arrival guard exercised"),SawSpeedGate);
			Test->TestTrue(TEXT("Stopped delivery completed"),Contract->GetState()==ENeonCityTransportState::Completed);
			Test->TestEqual(TEXT("1850 plus 350 reward"),E->GetBalance(),int64(2200));
			Test->TestEqual(TEXT("Exactly one reward receipt"),E->GetTransactions().Num(),2);
			Test->TestFalse(TEXT("Same receipt cannot pay twice"),E->CreditOnce(350,Contract->ReceiptKey()));
			Test->TestEqual(TEXT("Duplicate leaves balance unchanged"),E->GetBalance(),int64(2200));
			Test->TestEqual(TEXT("Same character data reflects reward"),Player->GetPlayerDataComponent()->GetMoney(),int64(2200));
			Test->TestEqual(TEXT("Offer, acceptance and completion messages"),Inbox->GetMessages().Num(),3);
			if (Capture()) { Shot(TEXT("Phone-Reward.png")); }
			return Next(11);
		}
		if (Stage==11 && Now-Phase>0.5f && !FScreenshotRequest::IsScreenshotRequested())
		{
			Key(PC,EKeys::SpaceBar,IE_Released); Key(PC,EKeys::F,IE_Pressed);
			return Next(12);
		}
		if (Stage==12 && Now-Phase>0.3f)
		{
			Key(PC,EKeys::F,IE_Released);
			Test->TestTrue(TEXT("F restores original character"),PC->GetPawn()==Player.Get());
			Test->TestEqual(TEXT("Wallet survives exit"),Player->GetCurrencyComponent()->GetBalance(),int64(2200));
			Test->TestTrue(TEXT("Phone remains available on foot"),Phone->IsPhoneOpen());
			Key(PC,EKeys::P,IE_Pressed); return Next(13);
		}
		if (Stage==13 && Now-Phase>0.2f)
		{
			Key(PC,EKeys::P,IE_Released);
			Test->TestFalse(TEXT("P closes phone after possession changes"),Phone->IsPhoneOpen());
			Test->TestEqual(TEXT("Remaining in zone never repeats payout"),E->GetBalance(),int64(2200));
			const int32 MessageCount=Inbox->GetMessages().Num();
			const FTransform Transform(Contract->GetActorLocation());
			auto* Recreated=World->SpawnActorDeferred<ANeonCityTransportTest>(ANeonCityTransportTest::StaticClass(),Transform);
			Recreated->DeliveryVehicle=Car.Get(); Recreated->FinishSpawning(Transform);
			Test->TestTrue(TEXT("Recreated contract observes session receipt"),Recreated->GetState()==ENeonCityTransportState::Completed);
			Test->TestEqual(TEXT("Recreated contract does not resend offer"),Inbox->GetMessages().Num(),MessageCount);
			Test->TestEqual(TEXT("Recreated contract cannot repay"),E->GetBalance(),int64(2200));
			Recreated->Destroy();
			return true;
		}
		return false;
	}
private:
	static void Key(APlayerController* PC,FKey KeyValue,EInputEvent Event)
	{
		PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),KeyValue,Event,FPlatformTime::Cycles64()));
	}
	static bool Capture() { return FParse::Param(FCommandLine::Get(),TEXT("NeonCityPhoneCapture")); }
	static void Shot(const TCHAR* Name)
	{
		IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase7")),true);
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Phase7")/Name,true,false);
	}
	FAutomationTestBase* Test;
	int32 Stage=0; double Started=0; float Phase=0;
	bool SawSpeedGate=false;
	FVector DriveOrigin;
	TWeakObjectPtr<ANeonCityPlayerCharacter> Player;
	TWeakObjectPtr<ANeonCityTransportTest> Contract;
	TWeakObjectPtr<ANeonCityBaseVehicle> Car;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityPhoneDelivery,"NeonCity.Economy.PhoneDeliveryPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityPhoneDelivery::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityDeliveryTests::FVerify>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
