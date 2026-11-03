#include "Navigation/NeonCityMapData.h"
#include "Algo/Reverse.h"

bool UNeonCityMapData::Contains(const FVector& World) const
{
 return World.X>=WorldMin.X && World.X<=WorldMax.X && World.Y>=WorldMin.Y && World.Y<=WorldMax.Y;
}
FVector2D UNeonCityMapData::ToUV(const FVector& World) const
{
 const FVector2D Size=WorldMax-WorldMin;
 return FVector2D((WorldMax.X-World.X)/FMath::Max(1.,Size.X),(WorldMax.Y-World.Y)/FMath::Max(1.,Size.Y));
}
FVector UNeonCityMapData::ToWorld(const FVector2D& UV,double Z) const
{
 const FVector2D Size=WorldMax-WorldMin;
 return FVector(WorldMax.X-UV.X*Size.X,WorldMax.Y-UV.Y*Size.Y,Z);
}
const TArray<uint8>& UNeonCityMapData::GetGrid() const
{
 const int64 Cells=int64(GridWidth)*GridHeight;
 if(Decoded.Num()==Cells || Cells<=0 || Cells>4000000)return Decoded;
 Decoded.Reset();Decoded.Reserve(Cells);
 for(int32 I=0;I+1<GridRuns.Num() && Decoded.Num()<Cells;I+=2)
 {
  const uint8 Value=uint8(FMath::Clamp(GridRuns[I],0,4));
  const int32 Count=FMath::Clamp<int64>(GridRuns[I+1],0,Cells-Decoded.Num());
  for(int32 K=0;K<Count;++K)Decoded.Add(Value);
 }
 if(Decoded.Num()!=Cells)Decoded.Reset();                         // malformed: no grid rather than a shifted one
 return Decoded;
}

namespace
{
struct FOpen{float F;int32 Cell;bool operator<(const FOpen& O)const{return F<O.F;}};
}
bool FNeonCityMapRouter::FindRoute(const UNeonCityMapData& Map,const FVector& Origin,const FVector& Goal,bool Driving,TArray<FVector>& Route,int32 MaxExpand)
{
 Route.Reset();
 const TArray<uint8>& G=Map.GetGrid();const int32 W=Map.GridWidth,H=Map.GridHeight;
 if(G.Num()!=W*H || W<2 || H<2)return false;
 const double C=Map.GridCell;
 auto CellOf=[&](const FVector& P){return FIntPoint(FMath::RoundToInt((P.X-Map.GridOrigin.X)/C),FMath::RoundToInt((P.Y-Map.GridOrigin.Y)/C));};
 auto In=[&](FIntPoint P){return P.X>=0 && P.Y>=0 && P.X<W && P.Y<H;};
 // cost per metre by surface; walking crosses lawns and woods, driving keeps to roads and paths
 const float WalkCost[5]={0,1.f,1.f,1.6f,2.6f},DriveCost[5]={0,1.f,1.8f,5.f,12.f};
 const float* Cost=Driving?DriveCost:WalkCost;
 // start / goal: the nearest passable cell within 30 m (the player may stand on a deck or in a doorway)
 auto Snap=[&](FIntPoint P)->int32
 {
  for(int32 R=0;R<=15;++R)for(int32 DY=-R;DY<=R;++DY)for(int32 DX=-R;DX<=R;++DX)
  {
   if(FMath::Max(FMath::Abs(DX),FMath::Abs(DY))!=R)continue;
   const FIntPoint Q(P.X+DX,P.Y+DY);if(In(Q) && G[Q.Y*W+Q.X]!=0)return Q.Y*W+Q.X;
  }
  return INDEX_NONE;
 };
 const int32 Start=Snap(CellOf(Origin)),End=Snap(CellOf(Goal));
 if(Start==INDEX_NONE || End==INDEX_NONE)return false;
 const int32 EX=End%W,EY=End/W;
 auto Heuristic=[&](int32 Cell){const int32 DX=FMath::Abs(Cell%W-EX),DY=FMath::Abs(Cell/W-EY);return float(FMath::Max(DX,DY)+.41421f*FMath::Min(DX,DY));};
 TArray<float> Gs;Gs.Init(TNumericLimits<float>::Max(),W*H);
 TArray<int32> From;From.Init(INDEX_NONE,W*H);
 TArray<FOpen> Open;Open.Reserve(65536);
 Gs[Start]=0;Open.HeapPush({Heuristic(Start),Start});
 static const int32 DX[8]={1,-1,0,0,1,1,-1,-1},DY[8]={0,0,1,-1,1,-1,1,-1};
 int32 Expanded=0;bool Found=false;
 while(Open.Num())
 {
  FOpen Top;Open.HeapPop(Top,EAllowShrinking::No);
  const int32 Cell=Top.Cell;
  if(Top.F-Heuristic(Cell)>Gs[Cell]+.001f)continue;                // stale entry
  if(Cell==End){Found=true;break;}
  if(++Expanded>MaxExpand)break;
  const int32 X=Cell%W,Y=Cell/W;const float Here=Cost[G[Cell]];
  for(int32 K=0;K<8;++K)
  {
   const int32 NX=X+DX[K],NY=Y+DY[K];if(NX<0 || NY<0 || NX>=W || NY>=H)continue;
   const int32 N=NY*W+NX;const uint8 V=G[N];if(V==0)continue;
   if(K>=4 && (G[Y*W+NX]==0 || G[NY*W+X]==0))continue;          // no corner cutting past a blocked cell
   const float Step=(K>=4?1.41421f:1.f)*.5f*(Here+Cost[V]);
   const float Ng=Gs[Cell]+Step;
   if(Ng<Gs[N]){Gs[N]=Ng;From[N]=Cell;Open.HeapPush({Ng+Heuristic(N),N});}
  }
 }
 if(!Found)return false;
 TArray<int32> Cells;for(int32 Cell=End;Cell!=INDEX_NONE;Cell=From[Cell])Cells.Add(Cell);
 Algo::Reverse(Cells);
 // string pulling: keep a cell only where the straight line from the last kept one would cross a costlier or
 // blocked surface (the route stays on the road / path it follows)
 auto LineKeepsTo=[&](int32 A,int32 B)
 {
  const int32 AX=A%W,AY=A/W,BX=B%W,BY=B/W;const int32 Steps=FMath::Max(FMath::Abs(BX-AX),FMath::Abs(BY-AY));
  const uint8 Worst=FMath::Max(G[A],G[B]);
  for(int32 S=1;S<Steps;++S)
  {
   const int32 X=FMath::RoundToInt(AX+(BX-AX)*double(S)/Steps),Y=FMath::RoundToInt(AY+(BY-AY)*double(S)/Steps);
   const uint8 V=G[Y*W+X];if(V==0 || V>Worst)return false;
  }
  return true;
 };
 TArray<int32> Kept={Cells[0]};
 for(int32 I=1;I<Cells.Num();++I)
  if(!LineKeepsTo(Kept.Last(),Cells[I]))Kept.Add(Cells[I-1]);
 if(Kept.Last()!=Cells.Last())Kept.Add(Cells.Last());
 Route.Add(Origin);
 for(int32 Cell:Kept)Route.Add(FVector(Map.GridOrigin.X+(Cell%W)*C,Map.GridOrigin.Y+(Cell/W)*C,Origin.Z));
 Route.Add(FVector(Goal.X,Goal.Y,Origin.Z));
 return true;
}
