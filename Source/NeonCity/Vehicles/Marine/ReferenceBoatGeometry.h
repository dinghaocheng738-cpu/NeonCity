/*
MIT License

Copyright (c) 2026 DRG Software Solutions LLC

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
#pragma once
#include "CoreMinimal.h"
// Generated from the locked reference; SI samples, UE-centimetre collision and attachment data.
namespace NeonCityReferenceBoat {
struct FSample { FVector Position; double Area; };
inline const FSample HullSamples[]={
 {FVector(0.245002301704,-0.228877089051,-3.13690932883),0.526},
 {FVector(-0.245002301704,-0.228877089051,-3.13690932883),0.526},
 {FVector(0.648262654493,-0.101386996929,-3.13828156178),0.5264},
 {FVector(-0.648262654493,-0.101386996929,-3.13828156178),0.5264},
 {FVector(0.260628748303,-0.294781947564,-2.26604578331),0.5484},
 {FVector(-0.260628748303,-0.294781947564,-2.26604578331),0.5484},
 {FVector(0.68952894689,-0.129328988601,-2.26688604589),0.5484},
 {FVector(-0.68952894689,-0.129328988601,-2.26688604589),0.5484},
 {FVector(0.27147626262,-0.34492340872,-1.39402714323),0.5832},
 {FVector(-0.27147626262,-0.34492340872,-1.39402714323),0.5832},
 {FVector(0.717838855773,-0.153154814399,-1.39550689496),0.5824},
 {FVector(-0.717838855773,-0.153154814399,-1.39550689496),0.5824},
 {FVector(0.273626016063,-0.37006691895,-0.514767883967),0.588},
 {FVector(-0.273626016063,-0.37006691895,-0.514767883967),0.588},
 {FVector(0.723524630058,-0.168700583126,-0.515614509734),0.5868},
 {FVector(-0.723524630058,-0.168700583126,-0.515614509734),0.5868},
 {FVector(0.255170498288,-0.366035779092,0.356526922178),0.5468},
 {FVector(-0.255170498288,-0.366035779092,0.356526922178),0.5468},
 {FVector(0.674805337135,-0.162695296216,0.356300247901),0.546},
 {FVector(-0.674805337135,-0.162695296216,0.356300247901),0.546},
 {FVector(0.200690355531,-0.319125037612,1.21177265657),0.4144},
 {FVector(-0.200690355531,-0.319125037612,1.21177265657),0.4144},
 {FVector(0.530922058519,-0.131452985749,1.21142342038),0.414},
 {FVector(-0.530922058519,-0.131452985749,1.21142342038),0.414},
 {FVector(0.115325854867,-0.218291740657,2.04191075158),0.2228},
 {FVector(-0.115325854867,-0.218291740657,2.04191075158),0.2228},
 {FVector(0.304804855457,-0.0820137039207,2.04158887279),0.222},
 {FVector(-0.304804855457,-0.0820137039207,2.04158887279),0.222},
 {FVector(0.0370029881285,-0.113374736581,2.80083211603),0.046},
 {FVector(-0.0370029881285,-0.113374736581,2.80083211603),0.046},
 {FVector(0.0961461625665,-0.0378403817846,2.81263889569),0.0472},
 {FVector(-0.0961461625665,-0.0378403817846,2.81263889569),0.0472},
};
struct FCollider { const TCHAR* Tag; FVector Center; FVector Half; bool Walkable; };
inline const FCollider Colliders[]={
 {TEXT("deck"),FVector(-142.5,0,30),FVector(240.5,125.891250665,5),true},
 {TEXT("bulwark"),FVector(-310,-127.161120963,70.75),FVector(80,12.965339774,35.75),false},
 {TEXT("bulwark"),FVector(-310,127.161120963,70.75),FVector(80,12.965339774,35.75),false},
 {TEXT("bulwark"),FVector(-150,-134.143618702,72.6611202686),FVector(80,10.7710511915,37.6611202686),false},
 {TEXT("bulwark"),FVector(-150,134.143618702,72.6611202686),FVector(80,10.7710511915,37.6611202686),false},
 {TEXT("bulwark"),FVector(37.5,-121.236458247,80.8657286764),FVector(107.5,23.6782116459,45.8657286764),false},
 {TEXT("bulwark"),FVector(37.5,121.236458247,80.8657286764),FVector(107.5,23.6782116459,45.8657286764),false},
 {TEXT("transom"),FVector(-386.5,0,70.75),FVector(3.5,128,35.75),false},
 {TEXT("houseWall"),FVector(55,-122.742649203,169.414435),FVector(90,2.25,61.585565),false},
 {TEXT("houseWall"),FVector(55,122.742649203,169.414435),FVector(90,2.25,61.585565),false},
 {TEXT("console"),FVector(121.5,0,87.5),FVector(23.5,110,52.5),false},
 {TEXT("helmSeat"),FVector(60.5,55,70),FVector(22.5,23,35),false},
 {TEXT("bench"),FVector(45,-86,58.5),FVector(37.5,21,23.5),true},
 {TEXT("roof"),FVector(19.5,0,236),FVector(114.5,125,5),true},
 {TEXT("trap"),FVector(-325,-66,56.5),FVector(47.5,27.5,18.5),true},
 {TEXT("trap"),FVector(-323,-66,97),FVector(47.5,27.5,18.5),true},
 {TEXT("trap"),FVector(-220,-64,56.5),FVector(47.5,27.5,18.5),true},
 {TEXT("trap"),FVector(-325,68,56.5),FVector(47.5,27.5,18.5),true},
 {TEXT("hauler"),FVector(-53.5,100,94.5),FVector(16.5,20,59.5),false},
 {TEXT("baitBarrel"),FVector(-80,-86,75),FVector(26,26,40),false},
 {TEXT("foredeck"),FVector(192.5,0,84.2372788862),FVector(47.5,124.545773548,49.2372788862),true},
 {TEXT("foredeck"),FVector(285,0,89.8226241323),FVector(45,99.495584882,54.8226241323),true},
 {TEXT("foredeck"),FVector(372.5,0,96.0262670877),FVector(42.5,59.5079412723,61.0262670877),true},
};
struct FPart { const TCHAR* Name; FVector Pivot; };
inline const FPart Parts[]={
 {TEXT("hull"),FVector(0,-0,0)},
 {TEXT("gelcoat"),FVector(0,-0,0)},
 {TEXT("wood"),FVector(0,-0,0)},
 {TEXT("fittings"),FVector(0,-0,0)},
 {TEXT("trap"),FVector(0,-0,0)},
 {TEXT("glow"),FVector(0,-0,0)},
 {TEXT("glass"),FVector(0,-0,0)},
 {TEXT("wheel"),FVector(93.8979446888,55.0000011921,129.263663292)},
 {TEXT("throttle"),FVector(124.000000954,93.0000007153,149.000000954)},
 {TEXT("radar"),FVector(80.0000011921,-0,271.000003815)},
 {TEXT("propeller"),FVector(-329.999995232,-0,-52.999997139)},
 {TEXT("rudder"),FVector(-357.999992371,-0,-51.9999980927)},
};
inline const FVector HelmEye=FVector(30,55,185);
inline const FVector CenterOfMassSource=FVector(0,0.3,-0.737678839671);
inline const FVector InertiaSource=FVector(14563.538496,15705.354384,4330.27213056);
inline const double Mass=3204;
}
