# Complete vertical slice — open acceptance items

Phase 20.5c is IN PROGRESS. No Phase 21 expansion is authorized.
The authoritative workstream matrix is CompleteVerticalSlice.md. A passing
automation case does not imply that its whole player experience is complete.

- Art: principal vehicle/aircraft silhouettes and interiors still read as
  prototypes; road markings, pavement tiling, bright daylight exposure, coastal
  edges, clothing and world detail need further work. Rain acceptance is pending.
- Characters: shared aim/reload/hit/collapse poses are active. Long-gun-specific
  poses, precise fingers and an authored recovery transition are unfinished.
- Occupants: seven player transport classes have visible seated drivers and
  verified limb contacts. Fine finger overlap, wheel/lever animation, doors,
  entry/exit animation, NPC drivers and passengers remain unfinished.
- Vehicles: theft/ownership consequences, collection history, tuning,
  convertible animation and complete races are not implemented for this program.
- Marine/aircraft: speedboat throttle, HUD, spatial motor playback and basic
  wake/spray are integrated; audible quality, final water and the complete
  normal-game experiences still need work. Aircraft power/HUD input and persistence
  checks pass. The reviewed search-light geometry, actual emitter, visible night
  beam and city/beach/ocean input checks now pass their bounded verification;
  final uninterrupted aviation acceptance and rotor audible quality remain open.
  Yacht artwork is not a
  completed drivable yacht. Submarine/fishing/lifestyle loops are not complete.
- World/player loops: the Marcus minimap route, restart and full navigation
  regression pass. Generic opportunity choices/history and restart pass focused
  checks and all 123 registered regressions. Wider source-driven
  city activities still need concrete gameplay providers.
  Varied jobs, autonomous events/memory,
  expanded property/lifestyle and medical/weather consequences remain open.
- Audio/UI: existing functional widgets do not constitute the final coherent
  phone/HUD. Final audio and the in-game onboarding/controls pass are pending.
- QA: the 30–60 minute normal-game flow, long stress session, cross-session
  integration of new loops and final performance/visual acceptance remain open.

Historical failed runs are preserved. The environment checkpoint had an isolated
fixed-wing building-impact failure followed by an unchanged passing repeat.
The character checkpoint exposed an obsolete single-node NPC-animation test;
its graph/bone-motion replacement passed a focused repeat. Current full-suite
results are recorded per checkpoint instead of hiding those earlier failures.

No paid purchase, sign-in or other user action is currently blocking independent
development. Do not invent license or quality approval for unevaluated assets.
Fusion is installed but its commercial-use licence type is not yet confirmed.
The user has been asked to identify it; Fusion-dependent production work is
locally pending. Independent Blender, UE, resource review and code work continue.

Banking now has separate cash/bank, atomic transfers, bounded loans and persistent
history with actual phone/restart checks. BankingFullRegression01 passes all 126
registered cases with zero errors. Its marine route reports a non-Nanite virtual
shadow-map marking queue overflow; investigate geometry/shadow cost during the
art/performance pass rather than hiding it by reducing the quality baseline.
The purpose field does not implement vehicle/property financing purchases or a
business. Whole income diversity and final phone visual/audio design remain open.
