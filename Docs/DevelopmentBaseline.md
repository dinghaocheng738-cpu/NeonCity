# NeonCity development baseline

Inspection date: 2026-09-18. This phase adds version-control protection and documentation only. Existing Source, Config, Content, project descriptor, GameMode, input handling and template variants remain unchanged.

## Project inventory

- One Runtime module: NeonCity. Source contains 42 C++ implementation files, 42 headers, one Build.cs and Game/Editor Target.cs files.
- Both targets use BuildSettingsVersion.V7 and EngineIncludeOrderVersion.Unreal5_8.
- Module dependencies: Core, CoreUObject, Engine, InputCore, EnhancedInput, AIModule, StateTreeModule, GameplayStateTreeModule, UMG and Slate.
- Main template classes: ANeonCityCharacter (movement, jumping and camera), ANeonCityPlayerController (Enhanced Input mappings and touch UI), ANeonCityGameMode (AGameModeBase).
- Existing Combat, Platforming and SideScrolling variants include their own characters, controllers, game modes and supporting systems. These are pre-existing template content, not new functionality introduced by this baseline.
- Five Config INI files. Startup/game map remains /Game/ThirdPerson/Lvl_ThirdPerson.Lvl_ThirdPerson. GlobalDefaultGameMode remains /Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C.
- Content contains 749 .uasset files and four .umap files, including mannequins, input, prototyping geometry, third-person assets, three variants, and external actors/objects. Asset payloads are not edited.
- No project Plugins directory. Explicit engine plugins are ModelingToolsEditorMode (Editor only), StateTree and GameplayStateTree; all descriptors exist in the installed engine.

## Verified local environment

- Engine: C:/Program Files/Epic Games/UE_5.8, installed build 5.8.2, CL 56702186; compatible CL 55116800.
- EngineAssociation is a local GUID resolving to that engine in the LENOVO user's registry. A different workstation must register/select its corresponding engine; do not silently replace the GUID.
- Visual Studio Community 2026, 18.10.1 (18.10.12210.168).
- MSVC folder 14.51.36231; actual compiler 14.51.36257. Windows SDK 10.0.26100.0. Engine-bundled .NET 10.0 runtime.
- Git 2.53.0.windows.3. Existing user identity is used, not replaced. The repository was initialized by the sandbox account; only D:/UnrealProjects/NeonCity is added to the normal user Git safe.directory list so ordinary Git/IDE access works. No wildcard trust is configured.

## Reproducing the build

Save and close Unreal Editor and Live Coding first. Use the installed UBT directly to avoid rebuilding build-tool binaries under the engine installation:

```powershell
$engineRoot = 'C:\Program Files\Epic Games\UE_5.8'
& "$engineRoot\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe" `
  "$engineRoot\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" `
  NeonCityEditor Win64 Development `
  '-Project=D:\UnrealProjects\NeonCity\NeonCity.uproject' `
  -WaitMutex -NoHotReloadFromIDE -NoEngineChanges
```

Replace NeonCityEditor with NeonCity to compile the Game target. These commands verify C++ builds; they do not cook/package assets or test PIE gameplay. Do not disable module-lock protections, delete functionality, or edit engine files to resolve a build failure.

## Build verification record

- Initial Editor build: project C++ compiled; linking failed with LNK1104 because Unreal Editor held UnrealEditor-NeonCity.dll open.
- Game target Win64 Development: succeeded, 11 actions, 57.20 seconds. Log: Saved/BaselineBuild-Game.log.
- Editor target Win64 Development: succeeded after editor shutdown, two link/metadata actions, 2.64 seconds. Log: Saved/BaselineBuild-EditorRetry.log. No source fix was required.
- Both targets are built again after the baseline commit; those local logs are Saved/BaselineBuild-FinalEditor.log and Saved/BaselineBuild-FinalGame.log. The final user report records their actual results.
- Original core file hashes: Saved/BaselineOriginalHashes.csv (ignored local verification output).

## Version-control policy

- Track Source, all Config, all Content (including external actors/objects), project/plugin descriptors, plugin source/content, packaging Build resources and .vsconfig.
- Ignore Binaries, DerivedDataCache, Intermediate, Saved, generated IDE projects, IDE user state and Visual Studio upgrade logs. Existing generated files are left on disk.
- .gitattributes preserves baseline bytes and marks Unreal packages binary. This baseline uses ordinary Git objects, not Git LFS pointers.
- Before asset volume grows, choose remote storage, Git LFS support, asset locking and backup policy together. A local commit is not an off-machine backup. Never rewrite this baseline automatically to migrate storage.
- Binary-only third-party plugins require explicit exceptions to Binaries ignore rules and a reproducible distribution plan before import.
- Work on short-lived branches, review scoped diffs, compile before committing, and prefer revert commits for published changes. Inspect uncommitted work before any checkout/reset; never discard it automatically.

## Findings to retain for follow-up

- UBT warns that MSVC 14.51.36257 is newer than its latest preferred 14.50.35717. Do not change a working installed toolchain in this phase; agree on and pin a validated toolchain for the team later.
- .vsconfig requests the 14.50 toolset and SDK 22621, while this machine currently provides 14.51 and SDK 26100. A fresh setup from this file may not reproduce this machine exactly.
- C4996 deprecation warnings originate in engine headers/PCH (asset registry, animation, Chaos and Character.h GetMovementBase); project source does not call this function. Leave the engine untouched and review during a planned upgrade.
- DefaultEditor.ini and the default content-browser path refer to legacy /Game/ThirdPersonCPP locations absent from Content. Active startup/game-map paths correctly point to the existing ThirdPerson map. No config cleanup is performed here.
- DefaultEngine.ini repeats DefaultGraphicsRHI and AutoExposure entries with equivalent values. DefaultGame.ini still labels the project Third Person Game Template. These are non-blocking metadata/config cleanup candidates.
- AndroidFileServer configuration contains a generated connection token. Do not publish the repository publicly without reviewing/rotating it and deciding local-secret handling. The token is not reproduced in this report.
- Current module exposes broad public dependencies/include paths and template classes generally lack NEONCITY_API export macros. Cross-module reuse will need deliberate API boundaries and export review.
- Binary assets were inventoried and hashed, not decoded or rewritten. Compilation alone does not validate Blueprint behavior, map loads, asset references, cooked builds or platform performance.

## Recommended modular direction (proposal only)

Keep the current module intact for now. Use components/interfaces for behavior boundaries; introduce modules only as stable dependency boundaries emerge. Public headers should expose small contracts, implementation dependencies should be private, runtime modules should not depend on editor modules, and dependencies should form a directed acyclic graph.

Suggested future grouping:

| Boundary | Systems | Dependency intent |
| --- | --- | --- |
| Shared contracts | identifiers, events, save contracts, common data | No dependency on concrete gameplay systems |
| Player | Character, Traversal, Swimming | Shared contracts and focused movement/interaction interfaces |
| Interaction | Interaction | Contracts reusable by characters, vehicles, property and missions |
| Simulation | Physics | Wrappers/components over engine physics, not a replacement physics engine |
| Transport | Vehicles, Boats, Aircraft | Physics and Interaction contracts; split concrete modes when needed |
| Combat | Combat | Character/Interaction contracts; avoid AI coupling |
| Population | AI, Traffic | Navigation, transport and perception contracts |
| Public response | Police, CityResponse | Consume events from AI/Traffic/Combat; avoid reverse dependencies |
| Progression | Economy, Missions, Property | Persistent data and gameplay events; avoid concrete world-actor dependencies |
| Presentation | UI | Read models and command interfaces; do not own gameplay state |

Next-stage order, subject to a separately approved feature scope:

1. Off-machine private backup, toolchain agreement, reproducible build instructions/CI, asset ownership/LFS policy, and a manual third-person PIE smoke check.
2. Document module boundaries, persistence requirements and performance budgets; establish the smallest necessary shared contracts.
3. Character boundaries, Interaction, Traversal and Swimming, preserving existing behavior until a specific change is approved. Add only the UI needed to observe/test each approved system.
4. Physics integration and Vehicles, then Boats and Aircraft.
5. Combat and AI, then Traffic.
6. Police and CityResponse.
7. Economy and Property, followed by Missions integration and fuller UI.

Do not create all proposed modules or systems up front. The current phase implements none of them.

Primary reference: [Epic: Unreal Engine Modules](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-modules).
