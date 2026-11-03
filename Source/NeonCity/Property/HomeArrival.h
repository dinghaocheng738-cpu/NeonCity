#pragma once
#include "CoreMinimal.h"
class UNeonCitySaveGame;

namespace NeonCityHomeArrival
{
/** Session start: the automatic load as the world opens (not a manual quick-load). The player arrives at the home's
 * spawn, and every home garage bay that is empty gets back the owned car authored for it, wherever the last session
 * left that car - unless the player stored it in another property's garage, or it is wrecked or retired. Value-only:
 * edits the loaded model before it is validated and applied. Returns the number of cars parked. */
int32 PrepareSessionStart(FName HomeID,const FTransform& HomeSpawn,const UNeonCitySaveGame& Defaults,UNeonCitySaveGame& Model);
}
