#include "Adventure/IslandTennisRules.h"
bool FIslandTennisRules::CanServeFrom(const FVector& P)const
{
 const bool Ad=(Points[0]+Points[1])%2!=0;
 return !Rally && P.X<=-1220 && P.X>=-1650 && FMath::Abs(P.Y)<=HalfWidth && (Ad?P.Y>=10:P.Y<=-10);
}
bool FIslandTennisRules::Strike(int32 Side,bool Serve)
{
 if(Side<0 || Side>1 || (Rally && (Serve || LastHitter==Side || (Service && FirstLanding))) || (!Rally && !Serve))return false;
 LastHitter=Side;Bounces=0;Winner=-1;Rally=true;Service=Serve;FirstLanding=true;return true;
}
void FIslandTennisRules::Award(int32 Side)
{
 Winner=Side;Rally=false;Faults=0;++Points[Side];
 if(Points[Side]>=4 && Points[Side]-Points[1-Side]>=2)
 {++Games[Side];Points[0]=Points[1]=0;}
}
void FIslandTennisRules::Fault()
{
 Rally=false;Winner=-1;if(++Faults>=2)Award(1-LastHitter);
}
bool FIslandTennisRules::Bounce(const FVector& P)
{
 if(!Rally)return false;
 const bool Opposite=LastHitter==0?P.X>0:P.X<0;
 const bool In=FMath::Abs(P.X)<=HalfLength+3.35f && FMath::Abs(P.Y)<=HalfWidth+3.35f;
 if(FirstLanding)
 {
  // Alternating deuce/ad service box; ball touching a painted line is in.
  const bool Ad=(Points[0]+Points[1])%2!=0;
  const bool CrossBox=LastHitter==0?(Ad?P.Y<=3.35f:P.Y>=-3.35f):(Ad?P.Y>=-3.35f:P.Y<=3.35f);
  if(!In || !Opposite || (Service && (FMath::Abs(P.X)>ServiceLength+3.35f || !CrossBox)))
  {if(Service)Fault();else Award(1-LastHitter);return false;}
  FirstLanding=false;Service=false;Faults=0;Bounces=1;return true;
 }
 ++Bounces;if(Bounces>=2)Award(LastHitter);return Rally;
}
void FIslandTennisRules::Net(){if(!Rally)return;if(Service)Fault();else Award(1-LastHitter);}
void FIslandTennisRules::Reset(){*this=FIslandTennisRules();}
FString FIslandTennisRules::Score()const
{
 const TCHAR* Labels[]={TEXT("0"),TEXT("15"),TEXT("30"),TEXT("40")};FString Point;
 if(Points[0]>=3 && Points[1]>=3)
  Point=Points[0]==Points[1]?TEXT("Deuce"):Points[0]>Points[1]?TEXT("Advantage YOU"):TEXT("Advantage AI");
 else Point=FString::Printf(TEXT("%s : %s"),Labels[FMath::Min(Points[0],3)],Labels[FMath::Min(Points[1],3)]);
 return FString::Printf(TEXT("YOU %d — %d AI  |  %s"),Games[0],Games[1],*Point);
}
