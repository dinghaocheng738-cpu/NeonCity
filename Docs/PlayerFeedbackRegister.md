# Phase 20.5c — player feedback acceptance register

Authoritative request: [original PF-001–PF-035 register](Requirements/Phase20_5c_PlayerFeedbackRegister.txt).
This supersedes conflicting priorities in the earlier quality reassessment.
Baseline: `91ec1ae39cdac3c4d620c900b68467098e4a2a9d`.
**COMPLETE VERTICAL SLICE = FAIL. USER FINAL PLAYTEST REQUIRED. No Phase 21.**

Existing tests, imported assets and prior screenshots are implementation evidence,
not acceptance of these player reports. No row is waived. A new automated PIE
route is identified as automated; it is never labeled a human/manual playtest.
The original physics, movement, dynamic water and licensed assets remain in place.

First dependency: reproduce and repair PF-007/009 in the normal South Beach world,
then PF-006/010. Further isolated-island detailing is deferred until connection.
Remaining groups: coherent marine movement/swimming; roads; designed interiors;
helicopter combat/light; damage/police/navigation/traffic; continuous hero views.

In the table, **Pending** means not newly verified under this register. Historical
technical passes do not change that. PARTIAL is not accepted completion; it needs
the missing work and ultimately user approval. Process requirements are checked
against actual changes and intake records, rather than a fictitious PIE action.

| PF-ID / concern | Reproduced? | Root cause | Fix / remaining work | Automated regression | Manual PIE evidence | Visual evidence | Status |
|---|---|---|---|---|---|---|---|
| PF-001 Swimming | Pending | Investigate normal animation layers/transitions | Verify full wade/swim/dive/exit loop | Pending new loop; old swim tests retained | Pending | Pending continuous route | PARTIAL |
| PF-002 Ocean motion | Normal-world shore/boat/arrival frames reviewed; active provider audited | Flat authored water remains active; native FFT is not connected to the visible game surface | Connect the existing shared field to real multiscale ocean rendering | Native FFT tests alone insufficient | Pending | Shore and arrival captures retain mirror-flat sea defect | FAIL |
| PF-003 Hull feels waves | Active provider returns a constant surface height/normal | Existing hull queries have no live wave field to sample | Connect the shared wave field; heave/pitch/roll from water, not camera substitute | Pending heading/speed/hull cases | Pending | Pending physical hull motion | FAIL |
| PF-004 Sea states | Pending | Audit current state providers | Calm/Light/Moderate and vessel differentiation | Pending | Pending | Pending Calm/Moderate | PARTIAL |
| PF-005 Speedboat throttle | Pending | Investigate normal helm input/feedback | Full idle/accelerate/coast/reverse/stop loop | Prior driving cases retained; new route pending | Pending | Pending HUD/wake/spray | PARTIAL |
| PF-006 Fishing boat location | Reproduced in native normal-world audit | MarineTestArea used the mainland prototype berth | Canonical island berth now used; stable ID retained; conservative old-default migration | Normal entry verifies berth/migration; IslandBerth03 focused loop PASS | Pending | IslandFishingBoatHelm.png; proper dry-pier boarding still open | PARTIAL |
| PF-007 Island invisible | Reproduced: normal map had zero islands, one level | Existing Landscape/village lived only in a separate map; no streaming connection | Authored UE island sublevel connected; translated materials; actual home-to-shore route | Before01 FAIL retained; ConnectedShore02 PASS | Pending; automation is not manual | PF007ConnectedShore02 / PF-007-IslandFromShore.png, reviewed | PARTIAL |
| PF-008 Worth visiting | Pending in connected world | Existing detail is only certified separately | Coherent silhouette/coast/path/landmark/exploration | Prior island tests not acceptance | Pending | Pending shore/approach/landing | PARTIAL |
| PF-009 Distance/streaming | Reproduced: island was disconnected; reload also exposed invalid village material parents | No mainland streaming entry; repeated BeginPlay recreated dynamic children from dynamic parents | Native bounded stream, safe destination reload, reuse valid material instances; aerial transitions still open | CoastalVoyage03, full 158-case checkpoint and final 5-case material recovery PASS; Marine A/B restart PASS | Unverified: editor exited before GUI capture; user feedback requested | Actual home-to-island approach and night arrival reviewed | PARTIAL |
| PF-010 Island dock | Real pier/boat/lights observed in connected world | Existing high pier has no native traversal adapter for its virtual ladder | Berth/lights/collision connected; safe climb/return and dry boarding remain open | Focused water/F boarding PASS; not full pier traversal | Pending | IslandFishingBoatHelm.png and IslandArrivalAtNight.png; night readability insufficient | PARTIAL |
| PF-011 Road geometry | Pending | Investigate current meshes/transitions | Curves, edges, curbs, parking and entrances | Pending collision/drive checks | Pending | Pending road review | PARTIAL |
| PF-012 Road surfaces | Pending | Investigate actual repetition/detail | Asphalt/repairs/markings/drainage with coherent placement | Pending asset/collision budget | Pending | Pending near/driver views | PARTIAL |
| PF-013 Road ends | Pending | Investigate visible prototype boundary | Credible connection or closure within existing slice | Pending drive/walk route | Pending | Pending endpoint views | FAIL |
| PF-014 Empty interiors | Reproduced in normal apartment startup image | Existing room has no furnished living layout | Designed interior still required | No fix yet | Pending | PF007NormalWorldBefore01 / PF-007-NormalStartup.png | FAIL |
| PF-015 Interior research | Official libraries, individual pages and current licences checked | Source/quality review required per asset | See InteriorAssetResearch.md; three reviewed chair/table/cabinet sources downloaded, ornate sofa rejected | File signatures/checksums and Blender dimensions/UV/material audit PASS; no UE import | Not applicable to research alone | Native combined-room evaluation pending | PARTIAL |
| PF-016 Interior design | Pending | Audit function/circulation/composition | Plan room uses and furniture circulation before placement | Pending access/collision checks | Pending | Pending furnished layout | PARTIAL |
| PF-017 Interior detail | Pending | Audit currently used rooms | Trim/storage/functional detail with LOD/instance budgets | Pending | Pending | Pending ordinary room views | PARTIAL |
| PF-018 Interior light | Pending | Audit window depth/artificial light | Day/sunset/night lighting in real rooms | Pending light budgets | Pending | Pending street-to-room views | FAIL |
| PF-019 Hero views | Pending | Connected experience still incomplete | Eight requested real-game views and continuous routes | Automation not visual acceptance | Pending | All eight current views required | FAIL |
| PF-020 Web asset first | Process audit pending | Existing licensed library does not cover every need | Search/review/reuse before new modeling | Manifest validation required | Not applicable to intake alone | Native quality evaluation required | PARTIAL |
| PF-021 License gate | Process audit pending | Review each newly adopted asset | Explicit license/commercial/credit/path record before download | Existing provenance checks retained | Not applicable to licensing alone | Source evidence required | PARTIAL |
| PF-022 Unified style | Pending | Audit visible mix at gameplay scale | Scale/material/color/detail/collision/LOD normalization | Pending native asset checks | Pending | Pending combined scene review | PARTIAL |
| PF-023 Helicopter pilot | Pending normal game | Prior occupant fixture is not acceptance | Verify visible seated pilot and safe exit without duplicate | Prior occupant checks retained | Pending | Pending entry/cockpit/exit | PARTIAL |
| PF-024 Helicopter missile | Pending | Investigate player loadout/input/targeting | Reuse Phase 19 selection/lock/track/impact/explosion | Pending normal-game route | Pending | Pending complete firing sequence | FAIL |
| PF-025 Helicopter aim | Pending | Investigate real HUD bindings | Reticles, lock state, ammo for each mount | Pending | Pending | Pending active HUD | FAIL |
| PF-026 Helicopter gun feedback | Pending | Investigate native effects/targets/audio | Muzzle/tracer/impact/ammo/damage feedback | Pending | Pending | Pending firing/impact | PARTIAL |
| PF-027 Helicopter speed | Pending | Investigate pilot input/physical response | Hover/acceleration/cruise/deceleration/hover | Prior flight tests retained | Pending | Pending continuous flight | PARTIAL |
| PF-028 Searchlight | User explicitly reports FAIL; reproduction pending | Prior fixture does not establish normal L-key operation | Verify and repair normal South Beach night player light | Pending normal input/surface checks | Pending | Pending road/building/car/beach/ocean | FAIL |
| PF-029 Pedestrian hits | Pending | Investigate damage/animation/effect composition | Visible hit/injury/downed/dead and restrained impact/blood | Pending | Pending | Pending normal combat | PARTIAL |
| PF-030 Police loses weapon | Pending | Investigate state restoration/layer/ownership | Weapon × police × damage × wanted regression | Prior tests retained; combined reproduction pending | Pending | Pending recovery loop | PARTIAL |
| PF-031 Marcus destination | Pending | Investigate active task navigation in normal HUD | Marker/minimap/direction/distance and route | Prior navigation tests retained | Pending | Pending accepted-task drive | PARTIAL |
| PF-032 Street hijacking | Pending | Audit traffic possession/driver exit/crime links | Immediate drive plus witness/wanted/ownership effects | Pending | Pending | Pending street encounter | FAIL |
| PF-033 Model quality | Pending across all named categories | Existing packages do not prove quality | Evaluate buildings/interiors/roads/all transport/weapons/coast/props | Technical checks necessary, insufficient | Pending | Per-category native evidence required | PARTIAL |
| PF-034 Experience first | Applied as acceptance policy | Prior component/test completion was insufficient | Controls/animation/VFX/audio/UI/failure/recovery/save/manual loop | Retain, do not substitute for playtest | Pending | Pending continuous complete flow | PARTIAL |
| PF-035 Slice before expansion | Scope enforced | No Phase 21 authorization | Existing South Beach/island/ocean only; user final playtest | Review changes for scope | User final playtest pending | Not a separate screenshot gate | PARTIAL |

## Evidence log

- Initial source audit: default map is South Beach; `NeonCityWorldManager` spawns
  `MarineTestArea` there. That actor creates both mainland boats and bounded water.
  `ImportReferenceIsland.py` explicitly created a separate island map, and current
  island route tests open that map directly. No fix or normal-world PASS is claimed
  from those source observations. See PlayerFeedbackCoastalValidation.md for subsequent reproduction, native fixes, retained failures and current evidence.
