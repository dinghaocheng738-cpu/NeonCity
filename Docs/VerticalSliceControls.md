# Current playable controls

This records the implemented bindings during Phase 20.5c. It is not the final
onboarding/UI acceptance, and does not describe planned controls as available.

| Context | Current controls |
|---|---|
| On foot | Existing Third Person movement/look; Space jumps; F interacts with the focused nearby object |
| Phone | P opens/closes; Enter accepts the current available opportunity; V opens the vehicle catalogue; H opens properties; Left/Right browse entries |
| Character weapons | 1–4 select owned pistol/SMG/rifle/shotgun; the current slot toggles stow; right mouse aims, left mouse fires, R reloads |
| Road car | W/S drive/reverse, A/D steer, Space brakes, F exits when the existing safe-exit checks permit |
| Speedboat | W/S progressively builds forward/reverse thrust, A/D rudder, release to coast, Space assist brake, F enters/exits when safe; speed/throttle HUD explains a refused moving exit |
| Helicopter | W/S forward/back, A/D yaw, Q/E strafe, Space/C ascend/descend; Shift raises power, Ctrl reduces power and slows to hover; mouse adjusts view; F enters/exits when safe |
| Fixed wing | W/S throttle, A/D roll, Q/E yaw, Up/Down or mouse Y pitch, Space ground brake, F enters/exits when safe |
| Equipped transport weapons | V arms/safes, Tab cycles mounts, LMB fires, RMB requests a timed target lock, R reloads; helicopter/fixed-wing instruments show actual weapon/ammo/lock state |

Visible seated drivers require no extra control. Returning from a vehicle uses
the same existing character and restores its original interaction/input context.
Helicopter Shift power persists when released. Hold Ctrl to decelerate even with
W held; release W before releasing Ctrl to stay hovering. The instruments show
actual speed/power and explain refused airborne exits. L toggles the existing
helicopter search light, but housing alignment and the full night visual route
remain unfinished, as recorded in HelicopterExperienceValidation.md.
