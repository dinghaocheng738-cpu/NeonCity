#include "Water/CoastalWaveData.h"
#include "NeonCityOceanFFT.h"
UCoastalWaveData::UCoastalWaveData()=default;
bool UCoastalWaveData::IsValidField()const
{
 if(CalmProfile.Num()!=224 || LightProfile.Num()!=224 || ModerateProfile.Num()!=224
  || GridSize<2 || GridSize>513 || DepthCm.Num()!=GridSize*GridSize || !DepthTexture
  || !FMath::IsFinite(SeaLevel) || GridMinimum.ContainsNaN() || GridMaximum.ContainsNaN()
  || GridMaximum.X<=GridMinimum.X || GridMaximum.Y<=GridMinimum.Y)return false;
 for(float D:DepthCm)if(!FMath::IsFinite(D) || D<0 || D>20000)return false;
 for(uint8 State=0;State<3;++State)
 {
  FNeonCityOceanParameters P;FMemory::Memcpy(&P,GetProfile(State).GetData(),sizeof(P));
  for(int32 I=0;I<4;++I)if(P.Sizes[I].ContainsNaN() || P.Cuts[I].ContainsNaN()
   || P.Sizes[I].X<=0 || P.Cuts[I].X<0 || P.Cuts[I].Y<=P.Cuts[I].X)return false;
  for(int32 I=0;I<2;++I)if(P.SysA[I].ContainsNaN() || P.SysB[I].ContainsNaN()
   || P.SysA[I].X<0 || P.SysB[I].X<0 || P.SysB[I].Y<=0)return false;
  for(float V:{P.Choppiness,P.FoamBias,P.FoamGain,P.FoamDecay,P.FoamAdd,P.Time,P.Depth})if(!FMath::IsFinite(V))return false;
  if(P.Depth<=0 || P.Choppiness<0 || P.Choppiness>2 || P.FoamDecay<0)return false;
 }
 return true;
}
float UCoastalWaveData::GetDepth(FVector2D P,FVector2D* Gradient) const
{
 if(Gradient)*Gradient=FVector2D::ZeroVector;
 if(P.ContainsNaN() || GridSize<2 || DepthCm.Num()!=GridSize*GridSize)return 20000;
 const FVector2D Step=(GridMaximum-GridMinimum)/(GridSize-1);
 const FVector2D G=(P-GridMinimum)/Step;
 const double GX=FMath::Clamp(G.X,0.,double(GridSize-1)),GY=FMath::Clamp(G.Y,0.,double(GridSize-1));
 const int32 X=FMath::Min(FMath::FloorToInt(GX),GridSize-2),Y=FMath::Min(FMath::FloorToInt(GY),GridSize-2);
 const double U=GX-X,V=GY-Y;
 const float A=DepthCm[Y*GridSize+X],B=DepthCm[Y*GridSize+X+1],C=DepthCm[(Y+1)*GridSize+X],D=DepthCm[(Y+1)*GridSize+X+1];
 if(Gradient)*Gradient=FVector2D(FMath::Lerp(B-A,D-C,V)/Step.X,FMath::Lerp(C-A,D-B,U)/Step.Y);
 return FMath::Lerp(FMath::Lerp(A,B,U),FMath::Lerp(C,D,U),V);
}
