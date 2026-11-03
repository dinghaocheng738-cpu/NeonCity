#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "UObject/UObjectGlobals.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityPlayerDataInvariantsTest,
	"NeonCity.Player.DataInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNeonCityPlayerDataInvariantsTest::RunTest(const FString& Parameters)
{
	UNeonCityPlayerDataComponent* Data = NewObject<UNeonCityPlayerDataComponent>();
	if (!TestNotNull(TEXT("Player data can be created without a world"), Data))
	{
		return false;
	}

	TestEqual(TEXT("Default Health"), Data->GetHealth(), 100.0f);
	TestEqual(TEXT("Default MaxHealth"), Data->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Default Stamina"), Data->GetStamina(), 100.0f);
	TestEqual(TEXT("Default Money"), Data->GetMoney(), int64(0));
	TestFalse(TEXT("Data does not tick"), Data->PrimaryComponentTick.bCanEverTick);

	Data->SetHealth(200.0f);
	TestEqual(TEXT("Health cannot exceed its maximum"), Data->GetHealth(), 100.0f);
	Data->SetHealth(-10.0f);
	TestEqual(TEXT("Negative Health is clamped"), Data->GetHealth(), 0.0f);
	Data->SetHealth(75.0f);
	Data->SetMaxHealth(40.0f);
	TestEqual(TEXT("Lowering maximum also clamps Health"), Data->GetHealth(), 40.0f);
	TestEqual(TEXT("Maximum is updated"), Data->GetMaxHealth(), 40.0f);
	Data->SetMaxHealth(200.0f);
	TestEqual(TEXT("Raising maximum does not heal"), Data->GetHealth(), 40.0f);
	Data->SetHealth(150.0f);
	TestEqual(TEXT("Health can use the increased maximum"), Data->GetHealth(), 150.0f);
	Data->SetMaxHealth(-1.0f);
	TestEqual(TEXT("Negative MaxHealth is clamped"), Data->GetMaxHealth(), 0.0f);
	TestEqual(TEXT("Zero maximum clears Health"), Data->GetHealth(), 0.0f);

	Data->SetStamina(-20.0f);
	TestEqual(TEXT("Negative Stamina is clamped"), Data->GetStamina(), 0.0f);
	Data->SetStamina(250.0f);
	TestEqual(TEXT("Stamina has no implicit maximum"), Data->GetStamina(), 250.0f);
	Data->SetMoney(MAX_int64);
	TestEqual(TEXT("Money preserves a valid value above 32-bit range"), Data->GetMoney(), int64(MAX_int64));
	Data->SetMoney(-1);
	TestEqual(TEXT("Negative Money is clamped"), Data->GetMoney(), int64(0));

	Data->SetMaxHealth(100.0f);
	Data->SetHealth(70.0f);
	Data->SetStamina(40.0f);
	const float InvalidValues[] = {
		std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity()
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(InvalidValues); ++Index)
	{
		Data->SetHealth(InvalidValues[Index]);
		Data->SetMaxHealth(InvalidValues[Index]);
		Data->SetStamina(InvalidValues[Index]);
		TestEqual(FString::Printf(TEXT("Nonfinite input %d preserves Health"), Index), Data->GetHealth(), 70.0f);
		TestEqual(FString::Printf(TEXT("Nonfinite input %d preserves MaxHealth"), Index), Data->GetMaxHealth(), 100.0f);
		TestEqual(FString::Printf(TEXT("Nonfinite input %d preserves Stamina"), Index), Data->GetStamina(), 40.0f);
	}

	return true;
}

#endif
