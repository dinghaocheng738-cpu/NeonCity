#include "ReferenceBoatLibrary.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"

bool UReferenceBoatLibrary::ConnectReferenceClearCoat(UMaterialExpression* Expression)
{
 const UMaterial* Material=Expression?Expression->GetTypedOuter<UMaterial>():nullptr;
 if(!Material || !Material->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Tidewater/Boat/"))) return false;
 return UMaterialEditingLibrary::ConnectMaterialProperty(Expression,TEXT("ClearCoat"),MP_CustomData0)
  && UMaterialEditingLibrary::ConnectMaterialProperty(Expression,TEXT("CoatRoughness"),MP_CustomData1);
}
