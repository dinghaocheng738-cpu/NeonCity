# Phase 20.5b measured source inventory

Generated from the two original Blender export ledgers. Counts are evaluated
LOD0 triangles, not an aesthetic grade. No external model downloads.
UE destination base: `/Game/NeonCity/Art/FullArt/`. Three LODs: 100/50/18%.

| Asset | Category | Triangles | Slots | Metres (X × Y × Z) |
|---|---|---:|---:|---|
| Sedan | Vehicles | 14890 | 9 | 4.735 × 2.110 × 1.262 |
| Sports | Vehicles | 15258 | 9 | 4.735 × 2.230 × 1.263 |
| SUV | Vehicles | 17106 | 8 | 4.735 × 2.110 × 1.333 |
| Pickup | Vehicles | 20346 | 9 | 4.735 × 2.110 × 1.262 |
| Police | Vehicles | 21682 | 9 | 4.789 × 2.110 × 1.585 |
| WheelTouring | Vehicles | 13992 | 3 | 0.642 × 0.642 × 0.268 |
| WheelSport | Vehicles | 14256 | 3 | 0.642 × 0.642 × 0.268 |
| WheelUtility | Vehicles | 13992 | 3 | 0.642 × 0.642 × 0.268 |
| PoliceBeacon | Vehicles | 2444 | 4 | 0.310 × 1.330 × 0.116 |
| Speedboat | Marine | 6313 | 9 | 5.047 × 2.052 × 1.498 |
| Yacht | Marine | 5559 | 9 | 12.651 × 3.752 × 3.970 |
| Helicopter | Aircraft | 6436 | 8 | 6.574 × 3.205 × 2.501 |
| HelicopterRotor | Aircraft | 140 | 2 | 8.100 × 8.100 × 0.090 |
| TailRotor | Aircraft | 468 | 2 | 1.050 × 1.050 × 0.120 |
| FixedWing | Aircraft | 5102 | 9 | 6.400 × 8.360 × 2.280 |
| Pistol | Weapons | 2264 | 4 | 0.365 × 0.055 × 0.234 |
| SMG | Weapons | 8232 | 3 | 0.836 × 0.068 × 0.289 |
| Rifle | Weapons | 8232 | 4 | 1.016 × 0.068 × 0.292 |
| Shotgun | Weapons | 8088 | 3 | 1.076 × 0.068 × 0.234 |
| UtilityBox | Props | 1692 | 3 | 0.660 × 0.434 × 1.160 |
| Hydrant | Props | 716 | 3 | 0.370 × 0.546 × 0.850 |
| ParkingMeter | Props | 880 | 4 | 0.240 × 0.169 × 1.368 |
| Manhole | Streets | 1504 | 2 | 0.680 × 0.680 × 0.036 |
| Drain | Streets | 2256 | 2 | 0.620 × 0.350 × 0.057 |
| BeachChair | Beach | 244 | 3 | 1.445 × 0.676 × 0.803 |
| Umbrella | Beach | 656 | 4 | 2.960 × 2.960 × 2.660 |
| LifeguardTower | Beach | 6108 | 5 | 2.720 × 4.625 × 3.895 |
| Palm | Beach | 15948 | 2 | 4.819 × 4.852 × 5.502 |
| Boardwalk3m | Beach | 3008 | 1 | 3.000 × 3.020 × 0.070 |
| CoralServices | Buildings | 6168 | 5 | 16.860 × 8.983 × 14.385 |
| WorksServices | Buildings | 6168 | 5 | 16.860 × 10.983 × 14.785 |
| BeachWest | Beach | 7040 | 1 | 109.000 × 28.500 × 4.158 |
| FoamWest | Beach | 160 | 1 | 109.000 × 1.276 × 0.000 |
| BeachEast | Beach | 704 | 1 | 9.000 × 28.500 × 4.153 |
| FoamEast | Beach | 16 | 1 | 9.000 × 1.310 × 0.000 |
| RoadRepair | Streets | 756 | 1 | 4.056 × 1.030 × 0.018 |
| MountedGun | VehicleWeapons | 560 | 2 | 0.570 × 0.180 × 0.290 |
| Cannon | VehicleWeapons | 560 | 2 | 0.570 × 0.180 × 0.290 |
| RocketPod | VehicleWeapons | 1380 | 3 | 0.626 × 0.460 × 0.460 |
| MissileRail | VehicleWeapons | 564 | 2 | 0.780 × 0.190 × 0.138 |
| GuidedMissile | VehicleWeapons | 336 | 2 | 1.213 × 0.440 × 0.440 |

41 unique meshes; 242,224 triangles summed once per unique LOD0.
This sum is not a frame triangle count; instance counts and selected LODs differ.

All transport/prop assets have no added collision. BeachWest/BeachEast alone
use static complex floor collision. Yacht shell has no boarding/collision.
Materials share the existing original 512-pixel masonry normal only where
appropriate; smooth transport finishes use geometric normals. Sand/foam add
bounded procedural variation. No new 4K texture set is introduced.
