#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Property/PropertySubsystem.h"

TArray<ENeonCitySaveDomain> UNeonCitySaveSubsystem::GetPlannedDomains() const
{
	return {ENeonCitySaveDomain::PlayerData, ENeonCitySaveDomain::Vehicles,
		ENeonCitySaveDomain::Properties, ENeonCitySaveDomain::Missions};
}

TArray<FNeonCityOwnedVehicle> UNeonCitySaveSubsystem::BuildVehicleSnapshot()
{ return GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>()->BuildSaveSnapshot(); }

TArray<FNeonCityPropertyRecord> UNeonCitySaveSubsystem::BuildPropertySnapshot()
{ return GetGameInstance()->GetSubsystem<UPropertySubsystem>()->BuildSaveSnapshot(); }
