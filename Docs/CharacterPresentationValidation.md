# Shared character presentation checkpoint

This is one part of Workstream B, not completion of the vertical slice.

## Scope and preservation

- Original Epic land locomotion remains a linked animation graph, with the
  existing swimming extension retained. No movement/input implementation changed.
- Reviewed Quaternius pistol, hit and collapse clips drive real Quinn bones.
  Shared grip attachment serves player, police and humanoid combat AI. Generic
  weapons, damage and vehicle hardpoints keep their existing independent APIs.
- Nonlethal hits keep the same equipped weapon and ammunition. Incapacitation
  follows the existing stow/recovery policy; actors are retained on death.
- Versioned player weapon persistence still restores to the shared grip.

## Evidence

Editor: `Saved/VerticalSlice/CharacterEditorBuild03.log`, successful.
Game: `Saved/VerticalSlice/CharacterGameBuild01.log`, successful.
Both Development Win64 targets have zero compile/link errors. Existing compiler
version/Engine deprecation warnings are not described as a warning-free build.

`Saved/VerticalSlice/CharacterIntegration01` contains 13 unique successful tests:
combat foundation, NPC physical feedback, cross-system effects, fixed-wing
building impact, swimming, character presentation and seven weapon checks.
The police check now uses real equip/aim/fire input and confirms real police
damage, reload, Wanted evidence, weapon identity and ammunition continuity.

`Saved/VerticalSlice/CharacterPresentation01/Report` contains the successful
pose-specific test. `Saved/VisualOverhaul/CharacterPresentation01/Motion/`
contains 99 actual PIE frames at 1600x900, Epic, 100% render scale. Aim, fire,
reload and downed frames were visually inspected. The inspection side camera
does not change bone simulation, input, weapon logic or damage, and is not
claimed as normal continuous gameplay acceptance.

Full regression: `Saved/VisualOverhaul/VerticalSliceCharacterRegression01/`.
First full run: 115 unique tests, 114 successful, one failed (`LivingCityPIE`).
All twelve failures inside that case asserted the old AnimationSingleNode mode;
route movement, ecology, reaction and traffic checks passed. NPCs now deliberately
use the shared AnimBlueprint. The test was updated to require that graph and to
measure real foot-bone movement during its existing route observation. The first
failed report is retained. `Saved/VerticalSlice/CharacterLivingCityRecheck01`
passes the updated case, including actual foot-bone motion and all historic
ecology/traffic assertions. Thus all 115 unique cases have passing evidence,
with the first full run and this repeat reported separately; a single clean
full queue will be required again for the next integration checkpoint.

## Remaining acceptance work

The support hand still has loose finger contact, the current pistol is oversized,
and long guns currently share a pistol pose. Recovery blends to standing and
needs a dedicated get-up clip. NPC variation is a material palette, not finished
clothing. Library 2 and the OpenGameArt pistol are source evaluation only.
No claim of final animation or hero-art acceptance is made here.

The prior environment regression's intermittent fixed-wing damage failure is
retained in CompleteVerticalSlice.md. Its unchanged isolated and focused
repeats passed; this does not establish the original failure's cause.
