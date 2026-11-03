# Vehicle experience validation

Overall: PLAYER EXPERIENCE NOT COMPLETE; ART PASS NOT COMPLETE.
Road, marine and aircraft simulations remain the existing authoritative systems.

## Visible occupants checkpoint

Technical: TransportOccupants04 passed in real South Beach PIE with normal F
interaction. Sedan, Sports, SUV, Pickup, speedboat, helicopter and fixed wing
show one seated representation of the real driver. The repeated first vehicle
reuses its mesh. All four limb endpoints reach their authored targets without
stretching, and exit restores the original character and removes the visible proxy.

Visual: seven separate actual UE captures were inspected. This resolves empty
player cabins and the measured major reach/canopy issues. Fine fingers, moving
controls and the coherent hero-art pass remain open. The diagnostic camera is
used only to inspect poses; it is not claimed as a normal driving play session.

Performance: no additional Actor tick, vehicle scan, collision body or teleport
is introduced. Only the visible occupied skeletal component evaluates its graph.
Quantitative final performance and long-session leakage acceptance remain pending.

Evidence, build logs, retained unsuccessful iterations and full-suite status:
[TransportOccupantValidation.md](TransportOccupantValidation.md).

Next player loops remain separate gates: theft with witnesses and safe NPC exit;
boat acceleration/coast/reverse/swim/re-entry; helicopter power/hover/cruise/light;
fixed-wing normal-game access; and cross-system persistence/recovery. Existing
historic physics cases are regression protection, not substitutes for those loops.
