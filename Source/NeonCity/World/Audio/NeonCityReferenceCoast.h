#pragma once
#include "CoreMinimal.h"

// Analytic coastDistance/Noise2D adapted from pinned Tidewater (MIT).
// Copyright (c) 2026 DRG Software Solutions LLC. Full notice:
// Docs/ThirdParty/Tidewater-LICENSE.txt. Source coordinates are metres, Y up.
namespace NeonCityReferenceCoast
{
 inline double Smooth(double A,double B,double X)
 {
  const double T=FMath::Clamp((X-A)/(B-A),0.,1.);return T*T*(3-2*T);
 }
 class FNoise
 {
 public:
  FNoise()
  {
   uint32 State=7;
   auto Random=[&]()
   {
    State+=0x6D2B79F5u;uint32 T=State;
    T=(T^(T>>15))*(T|1u);T^=T+((T^(T>>7))*(T|61u));
    return double(T^(T>>14))/4294967296.;
   };
   uint8 P[256];for(int32 I=0;I<256;++I)P[I]=uint8(I);
   for(int32 I=255;I>0;--I){const int32 J=FMath::FloorToInt(Random()*(I+1));Swap(P[I],P[J]);}
   for(int32 I=0;I<512;++I)Perm[I]=P[I&255];
   for(int32 I=0;I<256;++I)
   {
    const double A=Random()*UE_DOUBLE_PI*2;
    // JS stores its gradients in Float32Array before evaluating in doubles.
    Grad[I*2]=float(FMath::Cos(A));Grad[I*2+1]=float(FMath::Sin(A));
   }
  }
  double Noise(double X,double Y) const
  {
   const int32 XI=FMath::FloorToInt(X),YI=FMath::FloorToInt(Y);
   const double XF=X-XI,YF=Y-YI;const int32 PX=XI&255,PY=YI&255;
   const int32 H00=Perm[PX+Perm[PY]],H10=Perm[PX+1+Perm[PY]];
   const int32 H01=Perm[PX+Perm[PY+1]],H11=Perm[PX+1+Perm[PY+1]];
   const double D00=Grad[H00*2]*XF+Grad[H00*2+1]*YF;
   const double D10=Grad[H10*2]*(XF-1)+Grad[H10*2+1]*YF;
   const double D01=Grad[H01*2]*XF+Grad[H01*2+1]*(YF-1);
   const double D11=Grad[H11*2]*(XF-1)+Grad[H11*2+1]*(YF-1);
   const double U=XF*XF*XF*(XF*(XF*6-15)+10),V=YF*YF*YF*(YF*(YF*6-15)+10);
   const double A=D00+(D10-D00)*U,B=D01+(D11-D01)*U;
   return (A+(B-A)*V)*1.414;
  }
  double FBM(double X,double Y,int32 Octaves) const
  {
   double Sum=0,Amp=1,Freq=1,Norm=0;
   for(int32 I=0;I<Octaves;++I)
   {Sum+=Noise(X*Freq+I*17.13,Y*Freq-I*9.71)*Amp;Norm+=Amp;Amp*=.5;Freq*=2;}
   return Sum/Norm;
  }
 private:
  uint8 Perm[512]{};float Grad[512]{};
 };
 inline bool Query(double X,double Z,double& OutDistance,double& OutBeachZone)
 {
  OutDistance=OutBeachZone=0;
  if(!FMath::IsFinite(X) || !FMath::IsFinite(Z) || FMath::Abs(X)>1e6 || FMath::Abs(Z)>1e6)return false;
  static const FNoise N;
  const double WX=X+N.FBM(X/300,Z/300,3)*60,WZ=Z+N.FBM(X/300+7.1,Z/300-3.3,3)*60;
  const double F170=N.FBM(WX/170,WZ/170,4),Und=N.FBM(X/260,Z/90,2);
  const double BZ=(1-Smooth(120,205,FMath::Abs(X-10)))*(1-Smooth(40,140,Z+40))*Smooth(-260,-120,Z);
  auto Ellipse=[&](double CX,double CZ,double RX,double RZ)
  {const double DX=(X-CX)/RX,DZ=(Z-CZ)/RZ;return (FMath::Sqrt(DX*DX+DZ*DZ)-1)*FMath::Min(RX,RZ);};
  auto Min=[](double A,double B,double K)
  {const double H=FMath::Clamp(.5+.5*(B-A)/K,0.,1.);return B+(A-B)*H-K*H*(1-H);};
  double D=Min(Ellipse(0,-442,565,400),Min(Ellipse(-272,-25,92,205),Ellipse(288,-12,108,228),40),75);
  D+=F170*34*(1-.9*BZ);
  if(BZ<1)D+=N.FBM(X/38,Z/38,3)*7*(1-BZ);
  D+=Und*5*BZ;OutDistance=D;OutBeachZone=BZ;return true;
 }
}
