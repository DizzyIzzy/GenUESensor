# xBots Integration — Requirements & Usage Guide

## Overview

This feature bridges the **xBots simulation system** (external Python-based autonomous platform simulation) into Unreal Engine 5.5 for real-time 3D visualization. Entity positions, headings, and metadata stream from the xBots API Gateway WebSocket endpoint into UE, where each entity is represented by a spawned static-mesh actor geo-anchored to Cesium's globe.

---

## Architecture

```
xBots API Gateway (ws://10.0.0.100:3000/ws)
        |  sim.state.update JSON @ ~10 Hz
        v
UXBotsSubsystem  (GameInstanceSubsystem)
   - Manages WebSocket connection + auto-reconnect
   - Parses sim.state.update JSON
   - Maintains EntityCache (TMap<FString, FXBotEntityState>)
   - Fires OnEntityAdded / OnEntityUpdated / OnEntityRemoved delegates
        |
        v
UXBotsEntityManager  (WorldSubsystem, Tickable)
   - Listens to XBotsSubsystem delegates
   - Spawns / despawns AXBotEntityActor per entity
   - Evaluates camera distance → assigns LOD tier each frame
   - Propagates label visibility change to all actors
        |
        v
AXBotEntityActor  (Actor per entity)
   - UCesiumGlobeAnchorComponent: positions actor on globe
   - UStaticMeshComponent: visual representation
   - UTextRenderComponent: callsign label
   - Ring buffer of FXBotStateSnapshot + delayed-lerp smoothing
   - Tick interval varies by LOD tier (0 / 0.1 / 0.5 s)
```

---

## Connection Setup

### Default Connection Target

| Setting | Value |
|---------|-------|
| WS Host | `10.0.0.100` |
| WS Port | `3000` |
| WS Path | `/ws` |

### Changing the Connection Target

1. Open **Project Settings → Engine → XBots Subsystem** (or `DefaultGame.ini`).
2. Edit `XBotsIP` and `XBotsWSPort` under the `UXBotsSubsystem` defaults:
   ```ini
   [/Script/Genesis53Sensor.XBotsSubsystem]
   XBotsIP=10.0.0.100
   XBotsWSPort=3000
   bAutoConnect=true
   ReconnectIntervalSeconds=5.0
   ```

### Auto-Connect
`bAutoConnect=true` (default) calls `Connect()` automatically when the game starts. If the connection drops, the subsystem retries after `ReconnectIntervalSeconds`.

### Manual Connect via Blueprint
Call `GetGameInstance → GetSubsystem (XBotsSubsystem) → Connect` from any Blueprint.

---

## Mesh / Model Catalog Wiring

Entity types are identified by their `model` field in the xBots JSON (e.g. `"AZOR"`, `"TIBURON"`, `"ANANSE"`).

### Catalog Table Setup

1. In the Content Browser, create a **Data Table** asset:
   - Row Structure: `XBotModelMappingRow`
   - Suggested path: `Content/xbotsTools/DT_XBotModelCatalog`
2. Add one row per model name. Row name must **exactly** match the `model` string from xBots (uppercase):

   | Row Name  | Mesh                              | Scale       | FallbackColor |
   |-----------|-----------------------------------|-------------|---------------|
   | AZOR      | SM_AZOR (your mesh asset)         | (1, 1, 1)   | White         |
   | TIBURON   | SM_TIBURON                        | (1, 1, 1)   | Blue          |
   | ANANSE    | SM_ANANSE                         | (1, 1, 1)   | Tan           |
   | *(etc.)*  |                                   |             |               |

3. Assign the Data Table to **`XBotsEntityManager.ModelCatalogTable`** in Blueprints or `DefaultGame.ini`:
   ```ini
   [/Script/Genesis53Sensor.XBotsEntityManager]
   ModelCatalogTable=/Game/xbotsTools/DT_XBotModelCatalog.DT_XBotModelCatalog
   ```

If a model is unknown (no row found), the actor spawns with no mesh — you'll see a warning in the Output Log.

---

## Known Model Catalog

| Domain | Models |
|--------|--------|
| AIR    | AZOR, CORVO, GAVIOTA, HAVRON, STORK, BUZZARD, COLT, BLOWFISH, VULPE, MANTA, PANTHER, VENATOR |
| SEA    | TIBURON, MK7, MONTEREY |
| LAND   | ANANSE, CHEROOT, KODIAK, PONDEROSA |

---

## Label Toggle

- **Default:** labels are **ON** for all entities
- **Runtime toggle via Blueprint:**
  ```
  GetWorld → GetSubsystem (XBotsEntityManager) → SetEntityLabelsVisible(false)
  ```
- **Persistent default via .ini:**
  ```ini
  [/Script/Genesis53Sensor.XBotsEntityManager]
  bShowEntityLabels=false
  ```

---

## Smoothing / Jitter Reduction

Entities use delayed-lerp position smoothing:

- A ring buffer (up to 60 snapshots) stores raw state packets with their arrival timestamps.
- Each Tick, the actor renders its position at time `(Now - SmoothingDelaySeconds)`, interpolating between the two bracketing snapshots.
- Lat/lon/alt are linearly interpolated; heading uses shortest-arc interpolation to avoid 0°/360° wrap-around artifacts.

### Config

| Property | Default | Notes |
|----------|---------|-------|
| `EntitySmoothingDelay` (on Manager) | `1.0 s` | Sets delay for all newly spawned actors |
| `SmoothingDelaySeconds` (on Actor) | `1.0 s` | Per-actor override; set after spawn |

**Trade-offs:**
- Higher delay = smoother motion, but positions lag more behind real-time
- Set to `0` to disable smoothing (snap to latest position)
- xBots publishes at ~10 Hz; a 1 s delay keeps ~10 snapshots in the interpolation window

---

## Distance LOD

Reduces tick rate for distant entities to limit GPU/CPU cycles.

| Tier | Default Distance | Tick Interval |
|------|-----------------|---------------|
| Near | < 15 km         | Every frame   |
| Mid  | 15 – 50 km      | Every 0.1 s   |
| Far  | > 50 km         | Every 0.5 s   |

### Config

```ini
[/Script/Genesis53Sensor.XBotsEntityManager]
bEnableDistanceLOD=true
LodNearDistanceKm=15.0
LodMidDistanceKm=50.0
```

To **disable LOD** (all entities tick every frame): `bEnableDistanceLOD=false`.

---

## Domain Filters

Hide entire domains to reduce clutter:

```ini
[/Script/Genesis53Sensor.XBotsEntityManager]
bShowAir=true
bShowSea=true
bShowLand=true
```

Entities whose domain is filtered out at spawn time will not get an actor. Changing the filter at runtime does not retroactively remove already-spawned actors.

---

## Sending Commands (Bidirectional Stub)

The xBots WS connection is bidirectional. The subsystem provides:

```
GetGameInstance → GetSubsystem (XBotsSubsystem) 
  → SendCommand("some.routing.key", "{\"param\":\"value\"}")
```

This wraps the payload in the xBots command envelope:
```json
{ "type": "command", "routing_key": "some.routing.key", "payload": {"param": "value"} }
```

Command routing keys are defined by the xBots API Gateway configuration — consult the xBots documentation for available keys.

---

## Debug HUD

The existing `GenesisDebugHUD` (toggled by `bShowGenesisDebug`) now includes an **XBOTS STREAM** section showing:

- WebSocket connected / disconnected status
- WS target URL
- Entity count and seconds since last update
- Entity breakdown by domain (AIR / SEA / LAND)
- Live actor count, label toggle state, LOD toggle state

---

## Phase 0 Probe Tool

`Content/cameraService/test_scratch/xbots_ws_probe.py` is a standalone Python script that connects to the xBots WebSocket and pretty-prints simulation state updates, without touching UE. Use it to verify connectivity and inspect the JSON payload before running in UE.

```
pip install websocket-client
python xbots_ws_probe.py --host 10.0.0.100 --port 3000 --messages 3
```

---

## File Inventory

| File | Purpose |
|------|---------|
| `Source/Genesis53Sensor/XBotEntityTypes.h` | All shared structs and enums (FXBotEntityState, FXBotStateSnapshot, FXBotModelMappingRow, EXBotLODTier) |
| `Source/Genesis53Sensor/XBotsSubsystem.h/.cpp` | GameInstanceSubsystem — WS connection + entity cache + delegates |
| `Source/Genesis53Sensor/XBotEntityActor.h/.cpp` | Per-entity Cesium-anchored actor with smoothing ring buffer + LOD tick |
| `Source/Genesis53Sensor/XBotsEntityManager.h/.cpp` | WorldSubsystem — spawns/despawns actors, LOD tier evaluation |
| `Source/Genesis53Sensor/GenesisDebugHUD.cpp` | In-play HUD overlay (now includes xBots section) |
| `Content/cameraService/test_scratch/xbots_ws_probe.py` | Phase 0 standalone WS probe (Python) |
| `Content/xbotsTools/xbotToolsRequirements.md` | This document |

---

## Known Limitations / Future Work

- **Entity removal:** Only DEAD status and explicit remove signals are handled. Entities that silently disappear from the feed (no DEAD status, no explicit removal) will remain in the cache until the WS reconnects. Consider adding a TTL sweep timer if needed.
- **Mesh loading:** `LoadSynchronous()` is used for simplicity; swap to async streaming load (`RequestAsyncLoad`) if mesh count is large enough to cause frame hitches.
- **Command routing keys:** `SendCommand()` stub is wired but no specific commands are pre-defined — add task-specific commands as needed.
- **MQTT transport:** xBots also exposes MQTT on port 1883 via RabbitMQ. This was intentionally not used this sprint (UE MQTT plugin issues). The subsystem can be refactored to use MQTT if that transport becomes preferable.
