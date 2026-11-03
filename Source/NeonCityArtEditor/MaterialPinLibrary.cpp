#include "MaterialPinLibrary.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"

bool UMaterialPinLibrary::ConnectClearCoat(UMaterial* Material,UMaterialExpression* Expression,int32 Slot)
{
 if(!Material || !Expression || Slot<0 || Slot>1)return false;
 return UMaterialEditingLibrary::ConnectMaterialProperty(Expression,FString(),Slot==0?MP_CustomData0:MP_CustomData1);
}
