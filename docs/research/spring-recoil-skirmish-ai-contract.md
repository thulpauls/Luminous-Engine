# The Spring / Recoil "Skirmish AI" interface contract

Research report. Everything below is taken from files I fetched and read; each claim carries a source URL.
Anything I could not verify from source is marked **UNVERIFIED** or **UNCERTAIN**.

Repos examined:

- Recoil engine (Beyond All Reason): <https://github.com/beyond-all-reason/RecoilEngine> — branch `master`
- Upstream Spring engine: <https://github.com/spring/spring> — branch `develop` (last release tag visible in the API: `106.0.1`)

Line numbers cited are from the raw files at the time of fetching (they move over time; the URLs are stable).

---

## 1. The C ABI

### 1.1 Where the headers actually live (and where they do not)

The task guessed `AI/Interfaces/C/include/` and `AI/Include/`. **Both are wrong for the current trees:**

| Path guessed | Status |
|---|---|
| `AI/Interfaces/C/include/` | does not exist (contents API: 404) |
| `AI/Include/` | does not exist in `spring/spring@develop` (contents API: 404); `AI/` contains only `CMakeLists.txt`, `Interfaces`, `Skirmish`, `Wrappers` |
| `AI/Interfaces/C/src/` | exists, but contains only the **AI-Interface plugin** sources (`Interface.cpp`, `Interface.h`, `InterfaceExport.cpp`, `SSkirmishAISpecifier.cpp/h`) — not the ABI headers |

The actual public ABI headers live in **`rts/ExternalAI/Interface/`** in both repos:

- Recoil listing: <https://api.github.com/repos/beyond-all-reason/RecoilEngine/contents/rts/ExternalAI/Interface>
- Spring listing: <https://api.github.com/repos/spring/spring/contents/rts/ExternalAI/Interface>

Proof that this is the include root for AI code — `AI/CMakeLists.txt` puts it on the include path for everything under `AI/`:

```cmake
include_directories(
	${rts}
	${CMAKE_BINARY_DIR}/src-generated/engine
	${rts}/ExternalAI/Interface
	Wrappers
)
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/CMakeLists.txt>

and `AI/Interfaces/C/src/Interface.h` includes them through that path:

```cpp
#include "ExternalAI/Interface/SSkirmishAILibrary.h"
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Interfaces/C/src/Interface.h>

### 1.2 Struct names and definitions

| Struct | Header (repo path + URL) | Who implements it | Who calls it |
|---|---|---|---|
| `struct SSkirmishAICallback` | `rts/ExternalAI/Interface/SSkirmishAICallback.h` — [Recoil raw](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/SSkirmishAICallback.h) · [Spring raw](https://raw.githubusercontent.com/spring/spring/develop/rts/ExternalAI/Interface/SSkirmishAICallback.h) | the **engine** (`rts/ExternalAI/SSkirmishAICallbackImpl.cpp/.h`) | the **AI** calls it (~590 function pointers) |
| `struct SSkirmishAILibrary` | `rts/ExternalAI/Interface/SSkirmishAILibrary.h` — [raw](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/SSkirmishAILibrary.h) | the **AI** (or the AI Interface on its behalf) | the **engine / AI Interface** calls it (4 members) |
| `struct SAIInterfaceLibrary` | `rts/ExternalAI/Interface/SAIInterfaceLibrary.h` — [raw](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/SAIInterfaceLibrary.h) | the **AI Interface plugin** (`libAIInterface.so` / `AIInterface.dll`) | the **engine** calls it |
| `struct SAIInterfaceCallback` | `rts/ExternalAI/Interface/SAIInterfaceCallback.h` — [raw](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/SAIInterfaceCallback.h) | the **engine** | the **AI Interface plugin** calls it |
| `enum LevelOfSupport` | `rts/ExternalAI/Interface/ELevelOfSupport.h` — [raw](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/ELevelOfSupport.h) | — | legacy, the getter is commented out in current code |
| `enum EventTopic` + `S*Event` payload structs (28) | `rts/ExternalAI/Interface/AISEvents.h` — [raw](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/AISEvents.h) | engine (produces) | AI (`handleEvent`) |
| `enum CommandTopic` + `S*Command` payload structs (90) | `rts/ExternalAI/Interface/AISCommands.h` — [raw](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/AISCommands.h) | AI (produces) | engine (`Engine_handleCommand`) |
| `aidefines.h` (ABI version macro, `MAX_SKIRMISH_AIS`, data-dir names) | `rts/ExternalAI/Interface/aidefines.h` — [raw](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/aidefines.h) | — | both |

Names the task asked about that do **not** exist in these trees:

- **`SSkirmishAIManagerLibrary`** — no such header/type. `https://raw.githubusercontent.com/spring/spring/develop/rts/ExternalAI/Interface/SSkirmishAIManagerLibrary.h` returns 404. The role it used to play (engine → AI-Interface entry points) is now `SAIInterfaceLibrary`. **UNCERTAIN** whether the old name existed in pre-2010 Spring.
- **`AISEventCallback`** — no such type. There is no event-callback struct: events are plain C structs delivered through `SSkirmishAILibrary::handleEvent(skirmishAIId, topicId, const void* data)`.
- **`Callback.h`** — no such file. The engine-side files are `rts/ExternalAI/AICallback.h` (engine class `CAICallback`, [raw](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/AICallback.h)) and `rts/ExternalAI/SSkirmishAICallbackImpl.h` (the C ABI implementation).
- **`AIExport.h`** — exists, but it is **AI-side** (one per AI project, e.g. `AI/Skirmish/NullAI/src/AIExport.h`, [raw](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Skirmish/NullAI/src/AIExport.h)); it declares which functions the AI exports and sanity-checks the build defines.
- **`Interface.h`** — exists at `AI/Interfaces/C/src/Interface.h` (class `CInterface`, the engine-side loader class of the C AI Interface plugin).

### 1.3 How many function pointers are in `SSkirmishAICallback`?

Read directly from `rts/ExternalAI/Interface/SSkirmishAICallback.h`:

| File | Struct body lines | **Live function pointers** | Commented-out members |
|---|---|---|---|
| Recoil `master` | 26–2259 | **590** (590 unique names) | 6 |
| Spring `develop` | 26–2272 | **593** | 6 |

The six commented-out (dead) members in **both** trees: `File_locateForReading`, `File_locateForWriting`, `Unit_getResourceInfos`, `WeaponDef_getDamages`, `WeaponDef_Damage_getType`, `WeaponDef_getTdfId`.

The file is 96,710 bytes (Recoil) / 97,025 bytes (Spring), 2,265 / 2,279 lines.

**Top-level groups** (prefix before the first `_`), Recoil live counts:

| Group | # | What it covers |
|---|---:|---|
| `UnitDef_` | 193 | static unit blueprint data (168 scalars + `MoveData_` 13 + `WeaponMount_` 7 + `FlankingBonus_` 5) |
| `WeaponDef_` | 113 | static weapon data (98 scalars + `Shield_` 10 + `Damage_` 5) |
| `Unit_` | 53 | live unit state, current command queue, supported commands, weapons |
| `Map_` | 48 | terrain, maps (height/slope/LOS/radar/…), map lines/points, build-site queries |
| `Mod_` | 32 | mod metadata and global game constants (reclaim/transport/decay/…) |
| `Game_` | 30 | version-agnostic game state, teams, rules params, resource queries |
| *(ungrouped `get*`)* | 24 | `getFriendlyUnits`, `getEnemyUnits`, `getFeatures`, `getUnitDefs`, `getWeaponDefs`, `getAlliedTeams`, … |
| `FeatureDef_` | 22 | static feature (wreck/rock/tree) data |
| `Engine_` | 14 | engine version parts + `Engine_handleCommand` / `Engine_executeCommand` |
| `Group_` | 13 | unit groups + their supported commands |
| `SkirmishAI_` | 10 | this AI's team id, its AIInfo properties, its option values |
| `Economy_` | 9 | resource current/storage/income/usage/pull/share/sent/received/excess |
| `Feature_` | 8 | live feature state |
| `DataDirs_` | 7 | config/writable dirs, path location, data-dir roots |
| `Cheats_` | 4 | cheat enable/query flags |
| `Team_` | 3 | `Team_hasAIController`, `Team_getRulesParam{Float,String}` |
| `Log_` | 2 | `Log_log`, `Log_exception` |
| `Resource_` | 2 | `Resource_getName`, `Resource_getOptimum` |
| `File_` | 2 | `File_getContent`, `File_getSize` |
| `Debug_` | 1 | `Debug_GraphDrawer_isEnabled` |
| **Total** | **590** | |

(These counts were produced by parsing the struct body and excluding `//`-commented lines; the raw regex count including comments is 596.)

The first two members are worth calling out explicitly, because they are the *only* way an AI changes game state:

```c
int (CALLING_CONV *Engine_handleCommand)(int skirmishAIId, int toId, int commandId, int commandTopic, void* commandData);
int (CALLING_CONV *Engine_executeCommand)(int skirmishAIId, int unitId, int groupId, void* commandData);
```

The header's own doc comment states the contract: *"This struct contains only activities that leave the game state as it is… Activities that change game state (-> synced events) are handled through AI commands, defined in AISCommands.h."*

### 1.4 Exports: what an AI must implement vs what it just calls

`struct SSkirmishAILibrary` has exactly 4 members ([SSkirmishAILibrary.h](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/SSkirmishAILibrary.h)):

| Member | Required? | Evidence |
|---|---|---|
| `getLevelOfSupportFor(aiShortName, aiVersion, engineVersionString, engineVersionNumber, aiInterfaceShortName, aiInterfaceVersion)` | **optional** | header: `[optional] An AI not exporting this function is still valid.`; loader leaves it `nullptr` without complaining |
| `init(int skirmishAIId, const SSkirmishAICallback*)` | **optional** | header: `[optional]`; loader comment: *"it is permitted that an AI does not export this function, as it can still use EVENT_INIT instead"* |
| `release(int skirmishAIId)` | **optional** | same, `EVENT_RELEASE` covers it |
| `handleEvent(int skirmishAIId, int topicId, const void* data)` | **REQUIRED** | loader calls `reportInterfaceFunctionError()` and logs `Failed loading AI Library from file "…": no "handleEvent" function exported` |

Source of the loading logic: [`AI/Interfaces/C/src/Interface.cpp`](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Interfaces/C/src/Interface.cpp) (`CInterface::LoadSkirmishAILib`).

Minimal real-world proof — **NullAI** implements only `handleEvent` and is a valid AI:

```c
// AI/Skirmish/NullAI/src/AIExport.c
EXPORT(int) handleEvent(int skirmishAIId, int topic, const void* data) {
	// TODO: do something
	return 0; // signal: ok
}
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Skirmish/NullAI/src/AIExport.c> · exports declared in <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Skirmish/NullAI/src/AIExport.h>

The `EXPORT` / `CALLING_CONV` macros that make this a stable C ABI:

```c
#define EXPORT(type) SHARED_EXPORT type CALLING_CONV
// CALLING_CONV: empty on 64-bit; __stdcall on Win32; __attribute__((cdecl)) elsewhere
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/System/ExportDefines.h>

A C++ AI wraps the C ABI itself; CppTestAI is the reference:

```cpp
EXPORT(int) init(int skirmishAIId, const struct SSkirmishAICallback* innerCallback) { … }
EXPORT(int) release(int skirmishAIId) { … }
EXPORT(int) handleEvent(int skirmishAIId, int topic, const void* data) { … }
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Skirmish/CppTestAI/src/AIExport.cpp>

**ABI check.** The engine computes a fail-part ABI number as the *sum of `sizeof()` of every interface struct plus AIINTERFACE_EVENTS_ABI_VERSION + AIINTERFACE_COMMANDS_ABI_VERSION plus the sizes of the primitive types*, and hands it to the interface through `SAIInterfaceCallback::Engine_AIInterface_ABIVersion_getFailPart` ([aidefines.h](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/aidefines.h), [SAIInterfaceCallbackImpl.cpp](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SAIInterfaceCallbackImpl.cpp)). Consequence worth knowing: **the AI ABI is positional** — the engine must supply the full struct with every pointer in the exact same order for a binary AI to work.

**Recoil ≠ Spring, byte for byte.** Recoil's `SSkirmishAICallback` has 590 members vs Spring's 593 (`Teams_getSize`/`SkirmishAIs_getSize`/`SkirmishAIs_getMax`/`Gui_*`/`getAllyTeams` replaced by `getNumTeams`/`getNumSkirmishAIs`/`getMaxSkirmishAIs`/`getAlliedTeams`), and Recoil's `struct SInitEvent` has an extra `bool savedGame;` member that Spring's does not. The ABI version sums differ, so an AI Interface built against Spring is not ABI-identical to Recoil's.

---

## 2. What the AI can query

All names below are members of `SSkirmishAICallback` in [Recoil's header](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/SSkirmishAICallback.h). Every getter takes `int skirmishAIId` as its first parameter.

### 2.1 Map & terrain (48 functions)

- Height: `Map_getHeightMap(float*, sizeMax)`, `Map_getCornersHeightMap`, `Map_getHeight`, `Map_getElevationAt(x,z)`, `Map_getMinHeight`, `Map_getMaxHeight`
- Slope / surface: `Map_getSlopeMap`, `Map_getHardness`, `Map_getHardnessModMap`, `Map_getSpeedModMap`
- Resource (metal) map: `Map_getResourceMapRaw(resourceId, short*, sizeMax)`, `Map_getResourceMapSpotsPositions`, `Map_getResourceMapSpotsAverageIncome`, `Map_getResourceMapSpotsNearest`, `Map_getMaxResource`, `Map_getExtractorRadius`
- Visibility maps: `Map_getLosMap`, `Map_getAirLosMap`, `Map_getRadarMap`, `Map_getJammerMap`, `Map_getSonarMap`, `Map_getSonarJammerMap`, `Map_getSeismicMap`
- Environment: `Map_getCurWind`, `Map_getMinWind`, `Map_getMaxWind`, `Map_getTidalStrength`, `Map_getGravity`, `Map_getWaterDamage`, `Map_isDeformable`
- Build queries: `Map_findClosestBuildSite`, `Map_isPossibleToBuildAt`, `Map_getStartPos`
- Map identity / debug markers: `Map_getChecksum`, `Map_getHash`, `Map_getName`, `Map_getHumanName`, `Map_getWidth`, `Map_getLines`, `Map_getPoints`, `Map_Line_get{First,Second}Position`, `Map_Line_getColor`, `Map_Point_get{Position,Color,Label}`, `Map_getMousePos`, `Map_isPosInCamera`

Note: there is **no "slope class" accessor**; the engine exposes raw float slope/height maps. Discrete terrain classes are an engine-internal notion reached only indirectly (`UnitDef_MoveData_getPathType`, speed-mod map). **UNCERTAIN** whether any AI-visible slope-class mapping exists.

### 2.2 Live unit & feature queries (53 + 8 + 24 ungrouped)

- Unit: `Unit_getPos`, `Unit_getVel`, `Unit_getSpeed`, `Unit_getMaxSpeed`, `Unit_getHealth`, `Unit_getMaxHealth`, `Unit_getPower`, `Unit_getExperience`, `Unit_getParalyzeDamage`, `Unit_getBuildProgress`, `Unit_getCaptureProgress`, `Unit_getTeam`, `Unit_getAllyTeam`, `Unit_getDef`, `Unit_getGroup`, `Unit_getLimit`, `Unit_getMax`, `Unit_getStockpile`, `Unit_getStockpileQueued`, `Unit_isActivated`, `Unit_isBeingBuilt`, `Unit_isCloaked`, `Unit_isParalyzed`, `Unit_isNeutral`, `Unit_getLastUserOrderFrame`, `Unit_getRulesParam{Float,String}`, `Unit_getBuildingFacing`
- Weapons on a unit: `Unit_getWeapons`, `Unit_getWeapon`, `Unit_Weapon_getDef`, `Unit_Weapon_getRange`, `Unit_Weapon_getReloadFrame`, `Unit_Weapon_getReloadTime`, `Unit_Weapon_getShieldPower`, `Unit_Weapon_isShieldEnabled`
- Command queue of a unit: `Unit_getCurrentCommands`, `Unit_getSupportedCommands`, `Unit_CurrentCommand_get{Id,Options,Params,Tag,TimeOut,Type}`, `Unit_SupportedCommand_get{Id,Name,Params,ToolTip}`, `Unit_SupportedCommand_is{Disabled,ShowUnique}`
- Feature: `Feature_getPosition`, `Feature_getHealth`, `Feature_getDef`, `Feature_getReclaimLeft`, `Feature_getResurrectDef`, `Feature_getBuildingFacing`, `Feature_getRulesParam{Float,String}`
- Bulk/team queries (ungrouped): `getFriendlyUnits`, `getFriendlyUnitsIn`, `getEnemyUnits`, `getEnemyUnitsIn`, `getEnemyUnitsInRadarAndLos`, `getNeutralUnits`, `getNeutralUnitsIn`, `getTeamUnits`, `getSelectedUnits`, `getFeatures`, `getFeaturesIn`, `getGroups`, `getAlliedTeams`, `getEnemyTeams`, `getNumTeams`, `getNumSkirmishAIs`, `getMaxSkirmishAIs`, `getUnitDefs`, `getUnitDefByName`, `getWeaponDefs`, `getWeaponDefByName`, `getFeatureDefs`, `getResources`, `getResourceByName`

### 2.3 Unit definitions (`UnitDef_`, 193 functions)

168 scalar getters plus three sub-objects. Representative: `UnitDef_getCost`, `UnitDef_getBuildTime`, `UnitDef_getHealth`, `UnitDef_getHumanName`, `UnitDef_getHeight`, `UnitDef_getSpeed`, `UnitDef_getBuildSpeed`, `UnitDef_getBuildDistance`, `UnitDef_getBuildOptions(…, int* unitDefIds, sizeMax)`, `UnitDef_getCategory` / `UnitDef_getCategoryString`, `UnitDef_getCustomParams`, `UnitDef_getArmorType`, `UnitDef_getArmoredMultiple`, `UnitDef_getAutoHeal`, `UnitDef_getCloakCost`, `UnitDef_getDecloakDistance`, `UnitDef_getExtractsResource`, `UnitDef_getResourceMake`-style economy fields, `UnitDef_getWeaponMounts`, `UnitDef_getWeapons`, `UnitDef_getDeathExplosion`, `UnitDef_canManualFire`, `UnitDef_getHighTrajectoryType`, `UnitDef_getDecoyDef`, `UnitDef_getFlare*`, `UnitDef_getTransport*` (via `Mod_`), plus sub-groups:
`UnitDef_MoveData_*` (13: e.g. `UnitDef_MoveData_getPathType`, depth, slope, crush strength), `UnitDef_WeaponMount_*` (7), `UnitDef_FlankingBonus_*` (5: Mode/Min/Max/Dir/MobilityAdd).

### 2.4 Weapon definitions (`WeaponDef_`, 113 functions)

`WeaponDef_getRange`, `WeaponDef_getCost`, `WeaponDef_getAreaOfEffect`, `WeaponDef_getAccuracy`, `WeaponDef_getReload`-family, `WeaponDef_getProjectiles`, `WeaponDef_getCustomParams`, `WeaponDef_getDescription`, `WeaponDef_getType`, plus sub-groups
`WeaponDef_Shield_*` (10) and `WeaponDef_Damage_*` (5 live: `getCraterBoost`, `getCraterMult`, `getImpulseBoost`, `getImpulseFactor`, `getParalyzeDamageTime`).

### 2.5 Command descriptions

The C ABI does **not** expose a `CommandDescription` struct. It exposes getters, and the generated C++ wrapper materialises `CommandDescription` objects from them:

```
int (CALLING_CONV *Unit_getSupportedCommands)(int skirmishAIId, int unitId); //$ FETCHER:MULTI:NUM:SupportedCommand-CommandDescription
int (CALLING_CONV *Group_getSupportedCommands)(int skirmishAIId, int groupId); //$ FETCHER:MULTI:NUM:SupportedCommand-CommandDescription
int (CALLING_CONV *Unit_SupportedCommand_getId)   (int skirmishAIId, int unitId, int commandId);
int (CALLING_CONV *Unit_SupportedCommand_getName) (…);
int (CALLING_CONV *Unit_SupportedCommand_getParams)(…);
int (CALLING_CONV *Unit_SupportedCommand_getToolTip)(…);
int (CALLING_CONV *Unit_SupportedCommand_isDisabled)(…);
int (CALLING_CONV *Unit_SupportedCommand_isShowUnique)(…);
```

The engine-side object they project is `struct SCommandDescription` in `rts/Sim/Units/CommandAI/CommandDescription.h`:
`id` (`CMD_xxx`, *"custom codes can also be used"*), `type` (`CMDTYPE_xxx`), `queueing`, `hidden`, `disabled`, `showUnique`, `onlyTexture`, `name`, `action`, `iconname`, `mouseicon`, `tooltip`, `params`.
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/Sim/Units/CommandAI/CommandDescription.h>

### 2.6 Economy (9 + parts of `Game_`)

`Economy_getCurrent(aiId, resourceId)`, `Economy_getStorage`, `Economy_getIncome`, `Economy_getUsage`, `Economy_getPull`, `Economy_getShare`, `Economy_getSent`, `Economy_getReceived`, `Economy_getExcess` — all parameterised by `resourceId`. Team-level mirrors: `Game_getTeamResource{Current,Storage,Income,Usage,Pull,Share,Sent,Received,Excess}`, `Game_getTeamIncomeMultiplier`. Resource metadata: `Resource_getName`, `Resource_getOptimum`, `getResources`, `getResourceByName`, `Map_getMaxResource`. Units: `Unit_getResourceMake`, `Unit_getResourceUse`, `UnitDef_getExtractsResource`.

Note this is the **generalised multi-resource** model (`resourceId`), not a hardcoded metal/energy pair.

### 2.7 Pathfinding

There are no `GetPath`/`InitPath` members in `SSkirmishAICallback`. Pathfinding goes through the **command** channel, `Engine_handleCommand`, with these topics (see `AISCommands.h`):

```c
struct SInitPathCommand {
	float* start_posF3;      // start
	float* end_posF3;        // goal
	int    pathType;         // for what movement type
	float  goalRadius;       // default 8.0f
	int    ret_pathId;       // OUT
}; //$ COMMAND_PATH_INIT Pathing_initPath REF:ret_pathId->Path

struct SGetApproximateLengthPathCommand { float* start_posF3; float* end_posF3; int pathType; float goalRadius; float ret_approximatePathLength; }; //$ COMMAND_PATH_GET_APPROXIMATE_LENGTH
struct SGetNextWaypointPathCommand      { int pathId; float* ret_nextWaypoint_posF3_out; }; //$ COMMAND_PATH_GET_NEXT_WAYPOINT
struct SFreePathCommand                 { int pathId; }; //$ COMMAND_PATH_FREE
```
Topics: `COMMAND_PATH_INIT = 16`, `COMMAND_PATH_GET_APPROXIMATE_LENGTH = 17`, `COMMAND_PATH_GET_NEXT_WAYPOINT = 18`, `COMMAND_PATH_FREE = 19`.
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/AISCommands.h>

So the engine must expose an abstract path service keyed by an opaque `pathId`, with a `pathType` (movement class) and a goal radius. The names `GetPath` / `InitPath` / `GetApproximateLength` come from the **C++ / legacy wrapper layer** (the header comments name the engine functions `Pathing_initPath`, `Pathing_getApproximateLength`, `Pathing_getNextWaypoint`, `Pathing_freePath`). Exact wrapper method spellings: **UNVERIFIED** (the OO wrapper headers are generated at build time, not committed).

### 2.8 Rules params

`Game_getRulesParamFloat` / `Game_getRulesParamString`, `Team_getRulesParamFloat` / `Team_getRulesParamString`, `Unit_getRulesParamFloat` / `Unit_getRulesParamString`, `Feature_getRulesParamFloat` / `Feature_getRulesParamString`. These are the Lua-rules ↔ AI data channel.

### 2.9 Cheats (4)

`Cheats_isEnabled`, `Cheats_isOnlyPassive`, `Cheats_setEnabled`, `Cheats_setEventsEnabled`, plus cheat commands `COMMAND_CHEATS_SET_MY_INCOME_MULTIPLIER` (5), `COMMAND_CHEATS_GIVE_ME_RESOURCE` (20), `COMMAND_CHEATS_GIVE_ME_NEW_UNIT` (79), and `COMMAND_TRACE_RAY` (80) / `COMMAND_TRACE_RAY_FEATURE` (95). The AI's own `cheatEvents` flag also gates whether attacker ids are filled in damage events.

### 2.10 Engine / team / game / mod info

- Engine (14): `Engine_Version_getMajor/Minor/Patchset/Commits/Hash/Branch/Additional/BuildTime`, `Engine_Version_isRelease`, `Engine_Version_getNormal/Sync/Full`, `Engine_handleCommand`, `Engine_executeCommand`
- Game (30): `Game_getMyTeam`, `Game_getMyAllyTeam`, `Game_getPlayerTeam`, `Game_getTeamAllyTeam`, `Game_getTeams`, `Game_isAllied`, `Game_getCurrentFrame`, `Game_getSpeedFactor`, `Game_isPaused`, `Game_getMode`, `Game_getSetupScript`, `Game_getTeamColor`, `Game_getTeamSide`, `Game_isDebugModeEnabled`, `Game_getCategoriesFlag` / `Game_getCategoryFlag` / `Game_getCategoryName`
- Team (3) and SkirmishAI (10): `Team_hasAIController`; `SkirmishAI_getTeamId`; `SkirmishAI_Info_get{Size,Key,Value,Description,ValueByKey}` (the AI's own AIInfo.lua), `SkirmishAI_OptionValues_get{Size,Key,Value,ValueByKey}` (AI options chosen in the lobby)
- Mod (32): `Mod_getShortName`, `Mod_getHumanName`, `Mod_getVersion`, `Mod_getFileName`, `Mod_getHash`, `Mod_getDescription`, `Mod_getMutator`, `Mod_getLosMipLevel` / `RadarMipLevel` / `AirMipLevel`, `Mod_getConstructionDecay*`, `Mod_getReclaim*`, `Mod_getRepairEnergyCostFactor`, `Mod_getResurrectEnergyCostFactor`, `Mod_getCaptureEnergyCostFactor`, `Mod_getTransport{Ground,Hover,Ship,Air}`, `Mod_getRequireSonarUnderWater`, …
- `Group_` (13): `Group_isSelected`, `Group_getSupportedCommands`, `Group_SupportedCommand_*`, `Group_OrderPreview_*`

### 2.11 File / log / data dirs

- `File_getContent(aiId, fileName, void* buffer, int bufferLen)`, `File_getSize` — note `File_locateForReading` / `File_locateForWriting` are **commented out** in current headers (dead members)
- `Log_log(aiId, msg)`, `Log_exception(aiId, msg, severity, die)`
- `DataDirs_getConfigDir`, `DataDirs_getWriteableDir`, `DataDirs_getPathSeparator`, `DataDirs_locatePath(…, relPath, writeable, create, dir, common)`, `DataDirs_Roots_getSize`, `DataDirs_Roots_getDir`, `DataDirs_Roots_locatePath`
- `Debug_GraphDrawer_isEnabled` (the only debug-draw query; drawing itself is done via `COMMAND_DEBUG_DRAWER_*` / `COMMAND_DRAWER_*` commands)

---

## 3. The event system

Events are `enum EventTopic` + one payload struct per topic in `AISEvents.h`, delivered to `SSkirmishAILibrary::handleEvent`. `const int NUM_EVENTS = 28;` — the complete verified list:

| # | Topic | Payload struct | Fields |
|---:|---|---|---|
| 0 | `EVENT_NULL` | — | sentinel |
| 1 | `EVENT_INIT` | `SInitEvent` | `int skirmishAIId; const SSkirmishAICallback* callback; bool savedGame;` *(the `savedGame` field is Recoil-only)* |
| 2 | `EVENT_RELEASE` | `SReleaseEvent` | `int reason` (0 unspecified, 1 game ended, 2 team died, 3 AI killed, 4 AI crashed, 5 AI failed to init, 6 connection lost, 7 other) |
| 3 | `EVENT_UPDATE` | `SUpdateEvent` | `int frame` |
| 4 | `EVENT_MESSAGE` | `SMessageEvent` | `int player; const char* message;` |
| 5 | `EVENT_UNIT_CREATED` | `SUnitCreatedEvent` | `int unit; int builder;` |
| 6 | `EVENT_UNIT_FINISHED` | `SUnitFinishedEvent` | `int unit;` |
| 7 | `EVENT_UNIT_IDLE` | `SUnitIdleEvent` | `int unit;` |
| 8 | `EVENT_UNIT_MOVE_FAILED` | `SUnitMoveFailedEvent` | `int unit;` |
| 9 | `EVENT_UNIT_DAMAGED` | `SUnitDamagedEvent` | `unit, attacker, float damage, float* dir_posF3, weaponDefId, bool paralyzer` |
| 10 | `EVENT_UNIT_DESTROYED` | `SUnitDestroyedEvent` | `unit, attacker, weaponDefID` |
| 11 | `EVENT_UNIT_GIVEN` | `SUnitGivenEvent` | `unitId, oldTeamId, newTeamId` |
| 12 | `EVENT_UNIT_CAPTURED` | `SUnitCapturedEvent` | `unitId, oldTeamId, newTeamId` |
| 13 | `EVENT_ENEMY_ENTER_LOS` | `SEnemyEnterLOSEvent` | `int enemy;` |
| 14 | `EVENT_ENEMY_LEAVE_LOS` | `SEnemyLeaveLOSEvent` | `int enemy;` |
| 15 | `EVENT_ENEMY_ENTER_RADAR` | `SEnemyEnterRadarEvent` | `int enemy;` |
| 16 | `EVENT_ENEMY_LEAVE_RADAR` | `SEnemyLeaveRadarEvent` | `int enemy;` |
| 17 | `EVENT_ENEMY_DAMAGED` | `SEnemyDamagedEvent` | as unit-damaged but `int enemy` |
| 18 | `EVENT_ENEMY_DESTROYED` | `SEnemyDestroyedEvent` | `enemy, attacker` |
| 19 | `EVENT_WEAPON_FIRED` | `SWeaponFiredEvent` | `unitId, weaponDefId` (only manual-fire weapons, for performance) |
| 20 | `EVENT_PLAYER_COMMAND` | `SPlayerCommandEvent` | `int* unitIds; int unitIds_size; int commandTopicId; int playerId;` |
| 21 | `EVENT_SEISMIC_PING` | `SSeismicPingEvent` | `float* pos_posF3; float strength;` |
| 22 | `EVENT_COMMAND_FINISHED` | `SCommandFinishedEvent` | `unitId, commandId, commandTopicId` |
| 23 | `EVENT_LOAD` | `SLoadEvent` | `const char* file` (absolute, read-only) |
| 24 | `EVENT_SAVE` | `SSaveEvent` | `const char* file` (absolute, writable) |
| 25 | `EVENT_ENEMY_CREATED` | `SEnemyCreatedEvent` | `int enemy;` |
| 26 | `EVENT_ENEMY_FINISHED` | `SEnemyFinishedEvent` | `int enemy;` |
| 27 | `EVENT_LUA_MESSAGE` | `SLuaMessageEvent` | `const char* inData;` |

Source: <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/AISEvents.h>
Spring's copy has the same 28 topics/values but no `savedGame` in `SInitEvent`: <https://raw.githubusercontent.com/spring/spring/develop/rts/ExternalAI/Interface/AISEvents.h>

(The task's guess list also mentioned `Load`/`Save` — those are `EVENT_LOAD`/`EVENT_SAVE`, present. There is no separate `EnemyDamage` name: it is `EVENT_ENEMY_DAMAGED`.)

**Production side.** The engine's `CEngineOutHandler` has one hook per event, e.g.

```cpp
void UnitEnteredLos(const CUnit& unit, int allyTeamId);
void UnitLeftLos(...); void UnitEnteredRadar(...); void UnitLeftRadar(...);
void UnitIdle(const CUnit&); void UnitCreated(const CUnit&, const CUnit* builder);
void UnitFinished(const CUnit&); void UnitDestroyed(const CUnit&, const CUnit* attacker, int weaponDefID);
void UnitDamaged(const CUnit&, const CUnit* attacker, float damage, int weaponDefID, int projectileID, bool paralyzer);
void UnitMoveFailed(const CUnit&); void UnitCaptured(...); void UnitGiven(...);
void SeismicPing(int allyTeamId, const CUnit&, const float3& pos, float strength);
void WeaponFired(const CUnit&, const WeaponDef&);
void PlayerCommandGiven(const std::vector<int>& selectedUnitIds, const Command& c, int playerId);
void CommandFinished(const CUnit&, const Command&);
void SendChatMessage(const char* msg, int playerId);
bool SendLuaMessages(int aiTeam, const char* inData, std::vector<const char*>& outData);
void Update();
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/EngineOutHandler.h>

Each hook builds the payload struct and calls `CSkirmishAIWrapper::<Event>` which calls `HandleEvent(EVENT_*, &evtData)` on the AI library — verified for all 28 topics in <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAIWrapper.cpp> (e.g. `HandleEvent(EVENT_UNIT_IDLE, &evtData)`, `HandleEvent(EVENT_SAVE, &evtData)`, …). `Update()` is what makes the AI tick:

```cpp
void CSkirmishAIWrapper::Update(int frame) { const SUpdateEvent evtData = {frame}; HandleEvent(EVENT_UPDATE, &evtData); }
```
`EngineOutHandler::Update()` is declared in the header and drives all active AIs once per frame; `PreDestroy()` runs "just before all the units are destroyed".

---

## 4. Commands

### 4.1 The ingress function

```c
int (CALLING_CONV *Engine_handleCommand)(int skirmishAIId, int toId, int commandId, int commandTopic, void* commandData);
int (CALLING_CONV *Engine_executeCommand)(int skirmishAIId, int unitId, int groupId, void* commandData);
```
- `toId` = the team the command is addressed to, or `COMMAND_TO_ID_ENGINE` (`#define COMMAND_TO_ID_ENGINE -1`)
- `commandId` = AI-chosen id, echoed back in `EVENT_COMMAND_FINISHED` for asynchronous commands
- `commandTopic` = one of the `enum CommandTopic` values
- `commandData` = pointer to the matching `S*Command` struct

<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/SSkirmishAICallback.h> · <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/AISCommands.h>

### 4.2 Command topics

`enum CommandTopic` is a frozen, append-only list; the current header defines **96 named topics** spanning ids **0–96** (id 48 is unused/skipped) and **90 `S*Command` payload structs**, with `const int NUM_CMD_TOPICS = 97;`. Examples:

| Topic id | Name | Payload struct (fields abbreviated) |
|---:|---|---|
| 0 | `COMMAND_NULL` | — |
| 4 | `COMMAND_SEND_START_POS` | `SSendStartPosCommand` |
| 6 | `COMMAND_SEND_TEXT_MESSAGE` | `SSendTextMessageCommand { const char* text; int zone; }` |
| 8 | `COMMAND_SEND_RESOURCES` | `SSendResourcesCommand` |
| 9 | `COMMAND_SEND_UNITS` | `SSendUnitsCommand` |
| 16–19 | `COMMAND_PATH_*` | see §2.7 |
| 21 | `COMMAND_CALL_LUA_RULES` | `SCallLuaRulesCommand` |
| 35 | `COMMAND_UNIT_BUILD` | `SBuildUnitCommand` |
| 42 | `COMMAND_UNIT_MOVE` | `SMoveUnitCommand { int unitId; int groupId; short options; int timeOut; float* toPos_posF3; … }` |
| 43 | `COMMAND_UNIT_PATROL` | `SPatrolUnitCommand` |
| 44 | `COMMAND_UNIT_FIGHT` | `SFightUnitCommand` |
| 45 | `COMMAND_UNIT_ATTACK` | `SAttackUnitCommand { int unitId; int groupId; short options; int timeOut; int toAttackUnitId; … }` |
| 46 | `COMMAND_UNIT_ATTACK_AREA` | `SAttackAreaUnitCommand` |
| 47 | `COMMAND_UNIT_GUARD` | `SGuardUnitCommand` |
| 52 / 53 | `COMMAND_UNIT_SET_FIRE_STATE` / `SET_MOVE_STATE` | `SSetFireStateUnitCommand` / `SSetMoveStateUnitCommand` |
| 63 / 64 | `COMMAND_UNIT_RECLAIM_UNIT` / `_AREA` | `SReclaimUnitUnitCommand` / `SReclaimAreaUnitCommand` |
| 65 | `COMMAND_UNIT_CLOAK` | `SCloakUnitCommand` |
| 71 | `COMMAND_UNIT_SET_TRAJECTORY` | `SSetTrajectoryUnitCommand` |
| 77 | `COMMAND_UNIT_SET_IDLE_MODE` | `SSetIdleModeUnitCommand` |
| 78 | `COMMAND_UNIT_CUSTOM` | `SUnitCustomCommand` (AI-defined command id + params) |
| 79 | `COMMAND_CHEATS_GIVE_ME_NEW_UNIT` | `SCheatGiveMeNewUnitCommand` |
| 80 | `COMMAND_TRACE_RAY` | `STraceRayCommand` |
| 81 | `COMMAND_PAUSE` | `SPauseCommand` |
| 96 | `COMMAND_CALL_LUA_UI` | `SCallLuaUICommand` |

(Unused/reserved ids exist, e.g. `COMMAND_UNUSED_0/1 = 10/11`, `COMMAND_GROUP_CREATE/ERASE/ADD_UNIT/REMOVE_UNIT = 12..15` all marked `// unused`, and 48 is absent from the sequence.)

### 4.3 How a topic becomes an engine order

`rts/ExternalAI/AISCommands.cpp` maps each AI topic onto the engine's internal command id:

```cpp
int toInternalUnitCommandTopic(int aiCmdTopic, const void* sUnitCommandData) {
	switch (aiCmdTopic) {
		case COMMAND_UNIT_BUILD:  internalUnitCommandTopic = -cmd->toBuildUnitDefId; break;
		case COMMAND_UNIT_STOP:   internalUnitCommandTopic = CMD_STOP;   break;
		case COMMAND_UNIT_MOVE:   internalUnitCommandTopic = CMD_MOVE;   break;
		case COMMAND_UNIT_ATTACK: internalUnitCommandTopic = CMD_ATTACK; break;
		…
	}
}
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/AISCommands.cpp>

The inverse, `extractAICommandTopic(const Command* internalUnitCmd, int maxUnits)`, is also declared in `AISCommands.h` ("legacy support functions") — that is how `EVENT_PLAYER_COMMAND` and `EVENT_COMMAND_FINISHED` report a topic id.

### 4.4 The engine-side command id conventions

From `rts/Sim/Units/CommandAI/Command.h`:

```c
// ID's lower than 0 are reserved for build options (cmd -x = unitdefs[x])
static constexpr int CMD_STOP         =   0;
static constexpr int CMD_INSERT       =   1;
static constexpr int CMD_REMOVE       =   2;
static constexpr int CMD_WAIT         =   5;
static constexpr int CMD_TIMEWAIT     =   6;
static constexpr int CMD_DEATHWAIT    =   7;
static constexpr int CMD_SQUADWAIT    =   8;
static constexpr int CMD_GATHERWAIT   =   9;
static constexpr int CMD_MOVE         =  10;
static constexpr int CMD_PATROL       =  15;
static constexpr int CMD_FIGHT        =  16;
static constexpr int CMD_ATTACK       =  20;
static constexpr int CMD_AREA_ATTACK  =  21;
static constexpr int CMD_GUARD        =  25;
static constexpr int CMD_GROUPSELECT  =  35;
static constexpr int CMD_GROUPADD     =  36;
static constexpr int CMD_GROUPCLEAR   =  37;
static constexpr int CMD_REPAIR       =  40;
static constexpr int CMD_FIRE_STATE   =  45;
static constexpr int CMD_MOVE_STATE   =  50;
static constexpr int CMD_SETBASE      =  55;
static constexpr int CMD_INTERNAL     =  60;
static constexpr int CMD_SELFD        =  65;
static constexpr int CMD_LOAD_UNITS   =  75;
static constexpr int CMD_LOAD_ONTO    =  76;
static constexpr int CMD_UNLOAD_UNITS =  80;
static constexpr int CMD_UNLOAD_UNIT  =  81;
static constexpr int CMD_ONOFF        =  85;
static constexpr int CMD_RECLAIM      =  90;
static constexpr int CMD_CLOAK        =  95;
static constexpr int CMD_STOCKPILE    = 100;
static constexpr int CMD_MANUALFIRE   = 105;
static constexpr int CMD_RESTORE      = 110;
static constexpr int CMD_REPEAT       = 115;
static constexpr int CMD_TRAJECTORY   = 120;
static constexpr int CMD_RESURRECT    = 125;
static constexpr int CMD_CAPTURE      = 130;
static constexpr int CMD_AUTOREPAIRLEVEL = 135;
static constexpr int CMD_IDLEMODE     = 145;
static constexpr int CMD_FAILED       = 150;

static constexpr uint32_t MAX_COMMAND_PARAMS = 8;
static constexpr uint8_t META_KEY = (1 << 2), INTERNAL_ORDER = (1 << 3),
                         RIGHT_MOUSE_KEY = (1 << 4), SHIFT_KEY = (1 << 5),
                         CONTROL_KEY = (1 << 6), ALT_KEY = (1 << 7);
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/Sim/Units/CommandAI/Command.h>

`CMDTYPE_*` (`CMDTYPE_ICON = 0`, `CMDTYPE_ICON_MODE = 5`, `CMDTYPE_ICON_MAP = 10`, `CMDTYPE_ICON_AREA = 11`, `CMDTYPE_ICON_UNIT = 12`, `CMDTYPE_ICON_UNIT_OR_MAP = 13`, `CMDTYPE_ICON_FRONT = 14`, `CMDTYPE_ICON_UNIT_OR_AREA = 16`, `CMDTYPE_NEXT/PREV = 17/18`, `CMDTYPE_ICON_UNIT_FEATURE_OR_AREA = 19`, `CMDTYPE_ICON_BUILDING = 20`, `CMDTYPE_CUSTOM = 21`, `CMDTYPE_ICON_UNIT_OR_RECTANGLE = 22`, `CMDTYPE_NUMBER = 23`) describe what parameters a command takes and are what the `*_SupportedCommand_*` getters and `SCommandDescription::type` carry.

`Command` itself holds `int id[2]; int timeOut; unsigned int tag; unsigned char options; float params[MAX_COMMAND_PARAMS]` (`id[0]` = command id, `id[1]` = alternate/insert id), i.e. **every order is (id, options, tag, timeout, ≤8 float params)**. `IsBuildCommand()` is simply `GetID() < 0`.

### 4.5 Custom commands

`COMMAND_UNIT_CUSTOM` (topic 78) is the AI-defined-command channel; `Cmd`/`CMDTYPE_CUSTOM` and the comment "*CMD_xxx code (custom codes can also be used)*" in `CommandDescription.h` confirm that custom ids are legal in the command-description space. I found **no engine-enforced numeric threshold** for custom ids (no `CMD_CUSTOM` constant, no `10000` check in `Command.h`, `CommandDescription.h`, or `CommandAI.cpp`). The commonly repeated "custom command ids are ≥ 10000" rule is **UNVERIFIED** in this code; treat it as a mod/Lua convention if you rely on it.

Queuing semantics: options carry `SHIFT_KEY` (= queue instead of execute now), `ALT_KEY`, `CONTROL_KEY`, `META_KEY`, `INTERNAL_ORDER`; a per-command `timeOut` frame and a `tag` for later filtering (`CMD_REMOVE`). See `Command.h` and `CCommandAI::AllowedCommand` in <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/Sim/Units/CommandAI/CommandAI.cpp>.

---

## 5. Lifecycle / hosting

### 5.1 Directory layout and discovery

**AI Interface** (e.g. the C interface):

```
<datadir>/AI/Interfaces/<shortName>/<version>/InterfaceInfo.lua
<datadir>/AI/Interfaces/<shortName>/<version>/libAIInterface.so | AIInterface.dll
```

**Skirmish AI**:

```
<datadir>/AI/Skirmish/<shortName>/<version>/AIInfo.lua
<datadir>/AI/Skirmish/<shortName>/<version>/AIOptions.lua        (optional)
<datadir>/AI/Skirmish/<shortName>/<version>/libSkirmishAI.so | SkirmishAI.dll
```

The engine scans all VFS data dirs two levels deep, then looks for the info file:

```cpp
// GatherSkirmishAILibInfoFromLuaFiles()
// {SKIRMISH_AI_DATA_DIR}/{*}/{*}/AIInfo.lua
// {SKIRMISH_AI_DATA_DIR}/{*}/{*}/AIOptions.lua
for (const auto& possibleDataDir : dataDirsAccess.FindDirsInDirectSubDirs(SKIRMISH_AI_DATA_DIR)) {
	const auto& infoFiles = CFileHandler::FindFiles(possibleDataDir, "AIInfo.lua");
	…
	CSkirmishAILibraryInfo skirmishAIInfo = CSkirmishAILibraryInfo(infoFile, optionFileName);
	skirmishAIInfo.SetDataDir(FileSystem::EnsureNoPathSepAtEnd(possibleDataDir));
	skirmishAIInfo.SetDataDirCommon(FileSystem::GetParent(possibleDataDir) + "common");
	StoreSkirmishAILibInfo(duplicateSkirmishAIs, skirmishAIInfo, infoFile);
}
// interfaces: {AI_INTERFACES_DATA_DIR}/{*}/{*}/InterfaceInfo.lua
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/AILibraryManager.cpp>

So `AIInfo.lua` is parsed by the engine's generic Lua **info-table parser** (`info_parseInfo`) and by `option_parseOptions` for `AIOptions.lua` — [SkirmishAILibraryInfo.cpp](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAILibraryInfo.cpp). `dataDir` and `dataDirCommon` are **set by the engine, not read from the file** (`SSkirmishAILibrary.h` comments).

### 5.2 `AIInfo.lua` contents

Reference file, NullAI's (complete):

```lua
local infos = {
	{ key = 'shortName',        value = 'NullAI' },
	{ key = 'version',          value = '0.1' },
	{ key = 'name',             value = 'Test AI using the new C interface' },
	{ key = 'description',      value = 'This AI does absolutely nothing.' },
	{ key = 'url',              value = 'https://springrts.com/wiki/AI:NullAI' },
	{ key = 'loadSupported',    value = 'no' },
	{ key = 'interfaceShortName', value = 'C' },   -- AI Interface name
	{ key = 'interfaceVersion',   value = '0.1' }, -- minimum AI Interface version
}
return infos
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Skirmish/NullAI/data/AIInfo.lua> · same shape in <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Skirmish/CppTestAI/data/AIInfo.lua>

Recognised keys are the `SKIRMISH_AI_PROPERTY_*` macros: `dataDir`, `dataDirCommon` (engine-set), `shortName`, `version`, `name`, `description`, `url`, `loadSupported` (`"yes"`/`"no"`), `engineVersion`, `interfaceShortName`, `interfaceVersion` — plus vendor keys such as `isLuaAI` (read by `CSkirmishAILibraryInfo::IsLuaAI`). Validation: `shortName` and `version` may not contain any of `"\t _#"` ([SkirmishAILibraryInfo.cpp](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAILibraryInfo.cpp)). If no installed interface matches `interfaceShortName` + `interfaceVersion` (minimum, compared component-wise by `AILibraryManager::VersionCompare`), the AI is rejected: *"required AI Interface for Skirmish AI %s %s not found"*.

`InterfaceInfo.lua` for the C interface (reference):

```lua
local infos = {
	{ key = 'shortName', value = 'C' },
	{ key = 'version',   value = '0.1' },
	{ key = 'name',      value = 'default C AI Interface' },
	{ key = 'description', value = 'This interface is needed for C and C++ AIs' },
	{ key = 'url',       value = 'https://springrts.com/wiki/AIInterface:C' },
	{ key = 'supportedLanguages', value = 'C, C++' },
	{ key = 'supportsLookup',     value = 'false' },
}
return infos
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Interfaces/C/data/InterfaceInfo.lua>

### 5.3 Library file names and required exports

- Interface library: built by `AI/Interfaces/C/CMakeLists.txt` as a `MODULE` with `OUTPUT_NAME "AIInterface"` → `libAIInterface.so` / `AIInterface.dll`; the engine appends the platform lib extension itself (`sharedLib_createFullLibName("AIInterface", …)`) — [AIInterfaceLibrary.cpp `FindLibFile()`](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/AIInterfaceLibrary.cpp).
- **Required interface exports** (engine refuses to use the interface if missing): `loadSkirmishAILibrary`, `unloadSkirmishAILibrary`, `unloadAllSkirmishAILibraries`. **Optional**: `initStatic`, `releaseStatic`, `getLevelOfSupportFor`, `listSkirmishAILibraries`, `listSkirmishAILibraryInfos`, `listSkirmishAILibraryInfoKey`, `listSkirmishAILibraryInfoValue`, `listSkirmishAILibraryOptions` (the `list*` set is the dynamic-lookup alternative to `AIInfo.lua` scanning, enabled by `supportsLookup`).
- AI library: `sharedLib_createFullLibName("SkirmishAI", …)` → `libSkirmishAI.so` on Linux, **`SkirmishAI.dll`** on Windows (note: *not* `libSkirmishAI.dll`), located in the AI's `dataDir`; built with `OUTPUT_NAME "SkirmishAI"` by `configure_native_skirmish_ai` in `AI/Interfaces/C/CMakeLists.txt`. Source: `CInterface::FindLibFile()` in <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Interfaces/C/src/Interface.cpp>.

### 5.4 Instantiation sequence (verified in code)

```
engine start / game start
  CSkirmishAIHandler::LoadFromSetup(CGameSetup::GetAIStartingDataCont())   // who plays which team
  AILibraryManager::Init()
      GatherInterfaceLibInfo()      // AI/Interfaces/*/*/InterfaceInfo.lua
      GatherSkirmishAILibInfo()     // AI/Skirmish/*/*/AIInfo.lua (+ interface lookup)
  per local AI (on create):
      CEngineOutHandler::CreateSkirmishAI(id, savedGame)
        CSkirmishAIWrapper::PreInit(id)      -> CreateCallback() = skirmishAiCallback_GetInstance(this)
        CSkirmishAIWrapper::Init(savedGame)
            InitLibrary():
                AILibraryManager::FetchSkirmishAILibrary(key)
                  -> CAIInterfaceLibrary::FetchSkirmishAILibrary(aiInfo)
                       -> interface lib .so/.dll: loadSkirmishAILibrary(shortName, version)
                          |-> C CInterface::LoadSkirmishAILibrary -> sharedLib_load(dataDir/libSkirmishAI.so)
                              -> resolve "handleEvent" (required), "init", "release",
                                 "getLevelOfSupportFor" (all optional)
                library->Init(skirmishAIId, callback)      // the AI's exported init()
            SendInitEvent(): HandleEvent(EVENT_INIT, SInitEvent{skirmishAIId, callback, savedGame})
  each sim frame:
      CEngineOutHandler::Update()  -> CSkirmishAIWrapper::Update(frame) -> EVENT_UPDATE
  game end / kill:
      CSkirmishAIWrapper::Release(reason) -> EVENT_RELEASE
      library->Release(skirmishAIId)      -> the AI's exported release()
      AILibraryManager::ReleaseSkirmishAILibrary(key)  -> interface unloadSkirmishAILibrary(...)
      skirmishAiCallback_Release(this)
  save/load:
      CSkirmishAIWrapper::Save() writes a temp file and sends EVENT_SAVE{file}; Load() reads the
      temp file back into the stream after EVENT_LOAD{file}
```
Sources: <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAIWrapper.cpp> · <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAIWrapper.h> · <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/AIInterfaceLibrary.cpp> · <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Interfaces/C/src/Interface.cpp>

Instance bookkeeping: `std::array<CSkirmishAIWrapper, MAX_AIS> hostSkirmishAIs` and `std::array<std::vector<uint8_t>, MAX_TEAMS> teamSkirmishAIs` — *"There can be multiple Skirmish AIs per team"* (`EngineOutHandler.h`). `MAX_SKIRMISH_AIS = 255` (`aidefines.h`).

Save/load gating: `CSkirmishAIWrapper::IsLoadSupported()` reads the `loadSupported` property from the AI's `AIInfo.lua`, which is why NullAI/CppTestAI declare `loadSupported = 'no'`.

### 5.5 How AIs are named per team (lobby/setup)

```cpp
void CSkirmishAIHandler::LoadFromSetup(const CGameSetup& setup) {
	const auto& aiDataCont = setup.GetAIStartingDataCont();
	…
}
```
<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAIHandler.cpp>

`SkirmishAIData` carries `shortName`, `version`, an option key list and option values, `isLuaAI`, plus `team`, `hostPlayer`, `status`, `name` inherited from `SkirmishAIBase` ([SkirmishAIData.h](https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAIData.h)). The start-script/lobby therefore only needs to supply *which AI (shortName+version) plays which team, with which option values*. Lua AIs come from the mod archive's `LuaAI.lua` (`LoadPreGame()`; header comment: *"This loads the list of Lua AIs from the mod archives LuaAI.lua"*, and `luaAIShortNames` marks them).

### 5.6 Java and Python interfaces

Both are **AI Interfaces in the same sense as C** (they implement `SAIInterfaceLibrary` and ship their own `InterfaceInfo.lua`); they exist to host AIs written in those languages, and they must themselves bridge the same C ABI callbacks/events/commands.

- **Java**: present in `spring/spring` at `AI/Interfaces/Java` (submodule-style directory). Descriptor:
  ```lua
  { shortName = 'Java', version = '0.1', name = 'default Java AI Interface',
    supportedLanguages = 'Java (possily Groovy, JRuby, ...)' }
  ```
  <https://raw.githubusercontent.com/spring/spring/develop/AI/Interfaces/Java/data/InterfaceInfo.lua>
  Its build is JNI-based: native sources `src/main/native/InterfaceExport.c`, `JavaBridge.c`, `JniUtil.c`, JVM locaters, and AWK-generated bridges (`native_wrappCallback.awk`, `jni_wrappCallback.awk`, …) producing `CallbackFunctionPointerBridge.c`, `CallbackJNIBridge.c`, `EventsJNIBridge.c`; final artifact is a jar (`SkirmishAI.jar`) — <https://raw.githubusercontent.com/spring/spring/develop/AI/Interfaces/Java/CMakeLists.txt>.
- **Python**: a git submodule of `spring/spring` pointing at `spring/Python` (<https://github.com/spring/Python>; the contents API resolves `AI/Interfaces/Python` to that repo). Descriptor: `shortName = 'Python'`, `version = '0.1'`, `supportedLanguages = 'Python'` — <https://raw.githubusercontent.com/spring/Python/master/data/InterfaceInfo.lua>.
- **In Recoil, only C ships.** `AI/Interfaces` contains just `C/` and `CMakeLists.txt` (<https://api.github.com/repos/beyond-all-reason/RecoilEngine/contents/AI/Interfaces>), and that CMakeLists explicitly skips Python: *"Python AI Interface is not yet compatible with pureint changes -> temporarily disabled"* (<https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Interfaces/CMakeLists.txt>). Java is likewise not in Recoil's tree. **Practical consequence: for a BAR/Recoil host, the C ABI + the C/C++ wrapper is the interface that matters.** (The Recoil `AI/Wrappers` tree has `CUtils`, `Cpp`, `LegacyCpp` — <https://api.github.com/repos/beyond-all-reason/RecoilEngine/contents/AI/Wrappers>.)

---

## 6. Minimum engine checklist

Everything an engine must have for a Spring/Recoil Skirmish AI to load, run, and be useful. "Essential" = without it no unmodified AI runs at all; "Optional" = only some AIs/features need it.

### A. Hosting mechanics (all Essential)

- **Essential — dynamic library loading.** Load a `MODULE`/shared object at runtime, resolve symbols by name, unload. Two libraries per AI: the AI Interface plugin (`libAIInterface.so` / `AIInterface.dll`) and the AI itself (`libSkirmishAI.so` / `SkirmishAI.dll`, found inside the AI's `dataDir`).
- **Essential — a stable C calling convention.** `extern "C"`, `__declspec(dllexport)`/`visibility("default")`, `__stdcall` on Win32 / `cdecl` elsewhere, empty on 64-bit (`ExportDefines.h`, `aidefines.h`).
- **Essential — a *bit-exact* `SSkirmishAICallback` struct.** All 590 pointers, same order, same signatures. It is a positional vtable-like struct; a shorter/reordered version breaks every prebuilt AI silently or catastrophically.
- **Essential — an AI Interface plugin layer.** The engine must not call AI code directly: it goes engine → `SAIInterfaceLibrary` (`loadSkirmishAILibrary`, `unloadSkirmishAILibrary`, `unloadAllSkirmishAILibraries`; optionally `initStatic`/`releaseStatic`/`list*`) → `SSkirmishAILibrary` (`handleEvent` required; `init`/`release`/`getLevelOfSupportFor` optional).
- **Essential — an ABI/version handshake.** Provide `Engine_AIInterface_ABIVersion_getFailPart`/`getWarningPart` and the engine version getters through `SAIInterfaceCallback`, and refuse interfaces whose fail-part differs (`aidefines.h`).
- **Essential — AI instance registry.** Unique `skirmishAIId` (0…254, `MAX_SKIRMISH_AIS = 255`), team association, allow multiple AIs per team, host-player/local-vs-remote notion, per-instance kill reason/flags.
- **Essential — per-instance callback object.** One `SSkirmishAICallback` instance per AI, bound to that AI's id/team; every getter receives `skirmishAIId` first.
- **Essential — in-engine logging and file access for AIs.** `Log_log`, `Log_exception`, `File_getContent`/`File_getSize`, plus `DataDirs_getConfigDir`, `DataDirs_getWriteableDir`, `DataDirs_locatePath` (writeable dir so an AI can persist learned data).
- **Essential — the AI's data dirs.** Compute and expose `dataDir` (versioned) and `dataDirCommon` (version-independent) for every AI.

### B. Discovery, descriptors, setup (all Essential)

- **Essential — a virtual filesystem over multiple data dirs** (install + user-writable), searched for AI content.
- **Essential — a Lua info-table parser** (`info_parseInfo`) — `AIInfo.lua` and `InterfaceInfo.lua` are Lua tables, not INI.
- **Essential — the directory convention and 2-level scan**: `AI/Interfaces/*/*/InterfaceInfo.lua`, `AI/Skirmish/*/*/AIInfo.lua` (+ optional `AIOptions.lua`).
- **Essential — `AIInfo.lua` required keys honoured**: `shortName`, `version`, `name`, `description`, `url`, `loadSupported`, `interfaceShortName`, `interfaceVersion` (+ vendor keys like `isLuaAI`); validation of the `"\t _#"` character ban on shortName/version.
- **Essential — interface resolution by name + minimum version** with the "AI's requested interface not found → AI is skipped" path.
- **Essential — a game-setup/start-script input naming AIs per team** (shortName, version, team, host player, name, option values), e.g. `CGameSetup::GetAIStartingDataCont()` → `CSkirmishAIHandler::LoadFromSetup`.
- **Optional — dynamic AI lookup** (`supportsLookup` + the interface's `listSkirmishAILibraries`/`…Infos`/`…InfoKey`/`…InfoValue`/`…Options`) as an alternative to scanning `AIInfo.lua`.
- **Optional — Lua AIs** declared in the mod archive's `LuaAI.lua`.
- **Optional — AI option values** (`AIOptions.lua`, `SkirmishAI_OptionValues_*` getters, `SkirmishAI_OptionValues_getValueByKey`) so lobbies can configure AIs.

### C. Event delivery (Essential)

- **Essential — a per-frame tick**: `EVENT_UPDATE{frame}` once per simulation frame to every active AI, in a deterministic order.
- **Essential — the full 28-topic event vocabulary wired to real simulation hooks**: init/release/update/message/LuaMessage; unit created/finished/idle/move-failed/damaged/destroyed/given/captured; enemy created/finished/enter+leave LOS/enter+leave radar/damaged/destroyed; weapon fired; player command; seismic ping; command finished; load/save.
- **Essential — correct payload structs**, byte-for-byte (`S*Event`), including `ska*Event.dir_posF3` float arrays and the `attacker = -1` unknown case.
- **Essential — a release path** with reasons 0–7, so AIs can free resources.
- **Essential — per-AI event blocking** (`SetBlockEvents`) for killed/crashed AIs.
- **Optional — cheat events**: filling in hidden attacker ids only when cheats/cheat-events are enabled.
- **Optional — save/load**: writing the AI state to a temp file for `EVENT_SAVE`, reading it back for `EVENT_LOAD`, gated by `loadSupported`.

### D. Command ingress (all Essential)

- **Essential — one command funnel**: `Engine_handleCommand(skirmishAIId, toId, commandId, commandTopic, commandData)` (+`Engine_executeCommand` for group/unit execution), with `COMMAND_TO_ID_ENGINE = -1`.
- **Essential — topic translation**: the `enum CommandTopic` table (96 named topics, `NUM_CMD_TOPICS = 97`, 90 payload structs) mapped onto internal command ids (as `toInternalUnitCommandTopic`/`extractAICommandTopic` do).
- **Essential — a per-unit command queue** with `(command id, options byte, tag, timeout frame, ≤8 float params)`, plus the ability to insert/remove/reorder and to report the queue back (`Unit_getCurrentCommands` + `Unit_CurrentCommand_*`).
- **Essential — options semantics**: queue-vs-execute-now (SHIFT), override (ALT), CTRL, META, INTERNAL_ORDER.
- **Essential — per-unit/unitdef/group supported command descriptions** (`*_SupportedCommand_*` getters: id, name, params, tooltip, disabled, showUnique) built from an engine-side `SCommandDescription` with `CMDTYPE_*` parameter signatures and icons.
- **Essential — negative command ids mean "build unitDefId = -cmdId"**, and build orders must be validated against each builder's `buildOptions`.
- **Essential — a pathfinding service addressable by opaque path id** with a movement/path type and goal radius: `COMMAND_PATH_INIT` / `GET_APPROXIMATE_LENGTH` / `GET_NEXT_WAYPOINT` / `FREE` — *essential* if any AI is to move units intelligently; strictly optional only if you accept AIs that never path.
- **Optional — custom commands** (`COMMAND_UNIT_CUSTOM`) so AIs can define commands for Lua/other consumers.
- **Optional — cheat commands** (`COMMAND_CHEATS_*`, `COMMAND_TRACE_RAY`), debug/notification drawer commands (`COMMAND_DRAWER_*`, `COMMAND_DEBUG_DRAWER_*`), `COMMAND_CALL_LUA_RULES` / `COMMAND_CALL_LUA_UI`, `COMMAND_PAUSE`, `COMMAND_SEND_TEXT_MESSAGE`, `COMMAND_SEND_RESOURCES`, `COMMAND_SEND_UNITS`, `COMMAND_SEND_START_POS`.

### E. The simulation model the callbacks expose (Essential unless noted)

- **Essential — teams and ally-teams** with colours, sides, `isAllied`, team↔allyTeam mapping, and `Team_hasAIController`.
- **Essential — a global unit registry** with stable unit ids, per-team (and per-allyteam!) ownership, health/maxHealth, position/velocity/heading, build progress, experience/power, paralysis, cloak state, stockpile, and per-unit rules params.
- **Essential — per-team visibility state**: LOS map, air-LOS map, radar map, jammer map, sonar/sonar-jammer maps, seismic map — because `EVENT_ENEMY_ENTER_LOS/…RADAR/…DESTROYED`, `getEnemyUnitsInRadarAndLos`, and `Map_getLosMap`/`Map_getRadarMap` all depend on it.
- **Essential — a feature registry** (wrecks, rocks, trees) with position/health/reclaimLeft/resurrectable + per-def data, because features are a first-class AI target (reclaim/resurrect).
- **Essential — unit definitions loaded from Lua** (mod data) with the ~190 `UnitDef_*` fields, including `MoveData` (path type/slope/depth/crush), `WeaponMount`s, `FlankingBonus`, build options, build speed/time/distance, cost, resource make/use, category strings, custom params.
- **Essential — weapon definitions loaded from Lua** with the ~113 `WeaponDef_*` fields, shields and damage params.
- **Essential — feature definitions loaded from Lua** (`FeatureDef_*`).
- **Essential — a 3D heightmap terrain** with: heightmap (+ corner heights), slope map, hardness and speed-mod maps, elevation sampling, min/max height, map width/height, gravity, water level/damage, deformability, wind and tidal strength, and start positions. *Discrete "slope classes" are not exposed to AIs — raw float maps are; class-based movement is only implied by `UnitDef_MoveData_getPathType` and the speed-mod map.*
- **Essential — a resource map** with extractor radius, per-spot positions/average income, and raw resource values for arbitrary `resourceId`s.
- **Essential — an economy model**: current/storage/income/usage/pull/share/sent/received/excess per team per resource, plus per-unit resource make/use and per-def extraction. (Spring 104+/Recoil generalises "metal/energy" into an arbitrary resource list — the callbacks are `resourceId`-indexed.)
- **Essential — a build-placement service**: `Map_findClosestBuildSite`, `Map_isPossibleToBuildAt`, plus the footprint/blocking data (feature/unit `isBlocking`) that backs them.
- **Essential — a rules-param channel** between Lua rules and AIs (`Game_/Team_/Unit_/Feature_getRulesParam{Float,String}`) — this is how mods tell AIs about game modes, and how AIs read mod state.
- **Essential — mod/game metadata**: short/long name, version, hash, mutator, file name, description, plus the global simulation constants exposed by `Mod_*` (LOS/radar mip levels, construction decay, reclaim/repair/resurrect/capture cost factors, transport masks, etc.).
- **Essential — game state queries**: current frame, speed factor, paused, game mode, setup script path, debug mode, team categories/collision flags and category names.
- **Essential — engine version identity** (`Engine_Version_getMajor/Minor/Patchset/Commits/Hash/Branch/Additional/isRelease/Normal/Sync/Full`) — AIs version-gate on this.
- **Optional — unit groups** (`Group_*`, group-per-unit, group selected state).
- **Optional — cheat/trace introspection** (`Cheats_isEnabled`, `setEnabled`, trace-ray commands) for debug AIs and for BARb-style AIs that re-simulate terrain.
- **Optional — UI/drawer interaction from AIs** (`Debug_GraphDrawer_isEnabled`, `COMMAND_DRAWER_*`), camera query (`Map_isPosInCamera`, `Map_getMousePos`), chat (`EVENT_MESSAGE`, `COMMAND_SEND_TEXT_MESSAGE`).

### F. Things that are explicitly NOT required

- **A renderer** is not required for the AI contract: there is no drawing/rendering callback in `SSkirmishAICallback`. Only `Debug_GraphDrawer_isEnabled` + drawer *commands* (which an engine may no-op) touch visuals. A headless engine can host AIs (the AI Interface docs even reference `spring-headless`).
- **Audio, input, GUI, camera** are not part of the contract (the `Gui_*` query family exists only in the older Spring header variant; Recoil dropped it — `Gui_getViewRange`, `Gui_getScreenX/Y`, `Gui_Camera_get*` are Spring-only members).
- **A specific game/mod** is not required by the ABI, but AIs are useless without the sim model in §E, and most real AIs hard-depend on mod custom params.
- **Java/Python runtimes** are not required on a Recoil host: only the C interface ships there.

---

## Source index

**Recoil (contract / headers)**
- `rts/ExternalAI/Interface/SSkirmishAICallback.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/SSkirmishAICallback.h>
- `rts/ExternalAI/Interface/SSkirmishAILibrary.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/SSkirmishAILibrary.h>
- `rts/ExternalAI/Interface/SAIInterfaceLibrary.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/SAIInterfaceLibrary.h>
- `rts/ExternalAI/Interface/SAIInterfaceCallback.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/SAIInterfaceCallback.h>
- `rts/ExternalAI/Interface/AISEvents.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/AISEvents.h>
- `rts/ExternalAI/Interface/AISCommands.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/AISCommands.h>
- `rts/ExternalAI/Interface/aidefines.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/aidefines.h>
- `rts/ExternalAI/Interface/ELevelOfSupport.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/Interface/ELevelOfSupport.h>
- `rts/System/ExportDefines.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/System/ExportDefines.h>

**Recoil (engine implementation)**
- `AI/CMakeLists.txt` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/CMakeLists.txt>
- `AI/Interfaces/C/src/Interface.cpp` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Interfaces/C/src/Interface.cpp>
- `AI/Interfaces/C/src/Interface.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Interfaces/C/src/Interface.h>
- `AI/Interfaces/C/CMakeLists.txt` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Interfaces/C/CMakeLists.txt>
- `AI/Interfaces/C/data/InterfaceInfo.lua` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Interfaces/C/data/InterfaceInfo.lua>
- `AI/Interfaces/CMakeLists.txt` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Interfaces/CMakeLists.txt>
- `rts/ExternalAI/AILibraryManager.cpp` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/AILibraryManager.cpp>
- `rts/ExternalAI/AIInterfaceLibrary.cpp` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/AIInterfaceLibrary.cpp>
- `rts/ExternalAI/SkirmishAILibraryInfo.cpp` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAILibraryInfo.cpp>
- `rts/ExternalAI/SkirmishAIWrapper.cpp` / `.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAIWrapper.cpp> · <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAIWrapper.h>
- `rts/ExternalAI/EngineOutHandler.cpp` / `.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/EngineOutHandler.h>
- `rts/ExternalAI/SkirmishAIHandler.cpp` / `.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAIHandler.h>
- `rts/ExternalAI/SkirmishAIData.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SkirmishAIData.h>
- `rts/ExternalAI/AISCommands.cpp` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/AISCommands.cpp>
- `rts/ExternalAI/SAIInterfaceCallbackImpl.cpp` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/SAIInterfaceCallbackImpl.cpp>
- `rts/ExternalAI/AICallback.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/ExternalAI/AICallback.h>
- `rts/Sim/Units/CommandAI/Command.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/Sim/Units/CommandAI/Command.h>
- `rts/Sim/Units/CommandAI/CommandDescription.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/Sim/Units/CommandAI/CommandDescription.h>
- `rts/Sim/Units/CommandAI/CommandAI.cpp` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/rts/Sim/Units/CommandAI/CommandAI.cpp>
- `AI/Skirmish/NullAI/data/AIInfo.lua`, `src/AIExport.c`, `src/AIExport.h` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Skirmish/NullAI/data/AIInfo.lua> · <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Skirmish/NullAI/src/AIExport.c> · <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Skirmish/NullAI/src/AIExport.h>
- `AI/Skirmish/CppTestAI/data/AIInfo.lua`, `src/AIExport.cpp` — <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Skirmish/CppTestAI/data/AIInfo.lua> · <https://raw.githubusercontent.com/beyond-all-reason/RecoilEngine/master/AI/Skirmish/CppTestAI/src/AIExport.cpp>

**Spring (upstream, for comparison)**
- `rts/ExternalAI/Interface/SSkirmishAICallback.h` — <https://raw.githubusercontent.com/spring/spring/develop/rts/ExternalAI/Interface/SSkirmishAICallback.h>
- `rts/ExternalAI/Interface/AISEvents.h` — <https://raw.githubusercontent.com/spring/spring/develop/rts/ExternalAI/Interface/AISEvents.h>
- `rts/ExternalAI/Interface/AISCommands.h` — <https://raw.githubusercontent.com/spring/spring/develop/rts/ExternalAI/Interface/AISCommands.h>
- `AI/CMakeLists.txt`, `AI/Interfaces/C/CMakeLists.txt` — <https://raw.githubusercontent.com/spring/spring/develop/AI/CMakeLists.txt> · <https://raw.githubusercontent.com/spring/spring/develop/AI/Interfaces/C/CMakeLists.txt>
- Java interface — <https://raw.githubusercontent.com/spring/spring/develop/AI/Interfaces/Java/data/InterfaceInfo.lua> · <https://raw.githubusercontent.com/spring/spring/develop/AI/Interfaces/Java/CMakeLists.txt>
- Python interface (`spring/Python` submodule) — <https://raw.githubusercontent.com/spring/Python/master/data/InterfaceInfo.lua> · <https://github.com/spring/Python>
