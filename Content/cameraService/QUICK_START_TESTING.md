# Quick Start Testing Guide

## Fast Track: Get Testing in 5 Minutes

### 1. Build the Project
```powershell
# From project root
# Option A: Build from UE Editor
# - Open Genesis53Sensor.uproject
# - Tools → Compile → Compile Genesis53SensorEditor
# - Wait for compile to complete

# Option B: Build from Visual Studio
# Right-click .uproject → Generate VS project files
# Open Genesis53Sensor.sln → Build Solution (F7)
```

### 2. Setup in Editor (One-Time)

1. **Add OSC Receiver Actor:**
   - Open your level
   - Place Actors panel → Search "GenesisOSCReceiver"
   - Drag into level

2. **Configure the Actor (in Details panel):**
   ```
   OSC Configuration:
   ✓ bEnabled = true  (Toggle to enable/disable receiver)
   - Receive IP = 127.0.0.1
   - Receive Port = 8000
   
   MQTT Bridge Configuration:
   ✓ bAutoStartPythonBridge = true
   - MQTT Broker IP = 10.0.0.17  (or your broker IP)
   - MQTT Broker Port = 1883
   
   Genesis Wiring:
   - Georeference Target = [Drag CesiumGeoreference from Outliner]
   - Camera1 Actor = [Drag your OWL Camera from Outliner]
   ```
   
   **Toggle Control:**
   - Uncheck `bEnabled` to disable the receiver without removing the actor
   - Re-check to re-enable it (automatically restarts OSC server & Python bridge)

3. **Add Debug HUD (for Play Mode visualization):**
   - Open your GameMode Blueprint (e.g., BP_SimGameMode)
   - Set **HUD Class** = `GenesisDebugHUD`
   - Save
   
   **Important:** The HUD only appears during **Play In Editor (PIE)** mode, not while editing in the viewport.
   - Press **Play** button to see the HUD overlay
   - The HUD automatically finds and monitors the GenesisOSCReceiver
   - No manual wiring required - it auto-discovers the actor

### 3. Test Run

1. **Click Play in Editor (PIE button in toolbar)**
   - GenesisOSCReceiver starts automatically if `bEnabled = true`
   - Python bridge launches if `bAutoStartPythonBridge = true`

2. **Check Debug HUD (appears on screen):**
   - Should appear on screen automatically during Play mode
   - First line shows: `✓ RECEIVER ENABLED` (green) or `✗ RECEIVER DISABLED` (red)
   - Look for green "✓ CONNECTED - Receiving Data"

3. **Verify Data Flow:**
   ```
   Green checkmarks = Good
   Yellow warnings = Needs attention
   Red errors = Critical issue
   Gray/Disabled = Receiver is toggled off
   ```

**Note:** The Debug HUD only works during **Play mode**. It does not appear in the editor viewport while editing.

### 4. Manual Testing (if auto-start fails)

```powershell
# Terminal 1: Run Python Bridge  manually
cd "Content\cameraService\PythonClient"
.\venv\Scripts\python.exe mqtt_osc_bridge.py --broker 10.0.0.17 --port 1883

# Terminal 2: Monitor MQTT (optional - requires mosquitto_sub)
mosquitto_sub -h 10.0.0.17 -p 1883 -t "#" -v
```

## Understanding Editor Mode vs Play Mode

### GenesisOSCReceiver Behavior:
- **Editor Mode (Editing viewport):** Does NOT run - no data processing
- **Play In Editor (PIE):** Runs automatically if `bEnabled = true`
- **Toggle bEnabled:** Can be changed in Details panel at any time
  - Unchecking stops OSC server and terminates Python bridge
  - Re-checking restarts both automatically

### Debug HUD Behavior:
- **Editor Mode:** Does NOT appear (HUDs only work in Play mode)
- **Play In Editor (PIE):** Appears automatically as overlay
- **Alternative for Editor:** Use Output Log window to see log messages
  - Window → Developer Tools → Output Log
  - Filter by "LogTemp" to see GenesisOSCReceiver messages

### How the Debug HUD Works:
1. Set `GenesisDebugHUD` as your GameMode's HUD Class
2. When you press Play, Unreal spawns the HUD automatically
3. HUD finds the GenesisOSCReceiver actor in the level (auto-discovery)
4. HUD draws overlay on screen every frame during Play mode
5. No viewport/camera/actor wiring needed - it's automatic!

## Debug HUD Quick Reference

### Status Indicators

| Indicator | Meaning |
|-----------|---------|
| ✓ Green | Working correctly |
| ⚠ Yellow | Warning - system working but needs attention |
| ✗ Red | Error - critical issue |

### Common Messages

**"✓ RECEIVER ENABLED"**
- Receiver is active and processing data

**"✗ RECEIVER DISABLED"**
- bEnabled is unchecked - toggle it on in Details panel

**"✓ CONNECTED - Receiving Data"**
- All good! Data flowing normally

**"✗ NO DATA - Check Python Bridge & MQTT Broker"**
- Python bridge not running OR
- MQTT broker not accessible OR
- No publishers sending data

**"⊝ Receiver Disabled"** (gray)
- OSC receiver is toggled off via bEnabled checkbox

**"⚠ WARNING: Cesium Georeference NOT SET"**
- Assign CesiumGeoreference actor in Details panel

**"⚠ WARNING: Camera1Actor NOT SET"**
- Assign OWL Camera actor in Details panel

**"⚠ 'latitude' proc topic silent/empty — switching to raw fallback"**
- Processed topic not publishing, using raw topic instead
- This is expected behavior and not an error

## Quick Diagnostics

### Receiver Not Working?
1. **Check bEnabled checkbox** in Details panel (must be checked)
2. **Press Play** - receiver only runs during Play mode, not in editor viewport
3. **Check Output Log** for startup messages:
   - "GenesisOSCReceiver: Listening for OSC on..."
   - "GenesisOSCReceiver: Launching Python Bridge..."

### No Connection?
```powershell
# Test network connectivity to broker
Test-NetConnection -ComputerName 10.0.0.17 -Port 1883

# Check if Python bridge is running
Get-Process python -ErrorAction SilentlyContinue

# Check if OSC port is listening
netstat -an | findstr "8000"
```

### No Data Updates?
1. **Verify bEnabled = true** (check Details panel)
2. **Confirm you're in Play mode** (Press Play button)
3. Check Output Log for OSC messages ("OSC packet received...")
4. Verify channelList.conf exists and is valid JSON
5. Confirm MQTT topics match what broker is publishing
6. Use MQTT Explorer to verify broker has active publishers

### Camera Not Moving?
1. Is Camera1Actor assigned in Details panel? (Check debug HUD)
2. Is camera receiving non-zero offset values? (Check debug HUD)
3. Is camera attached/parented to georeference actor?

## Performance Checks

### Frame Rate Impact
```
Console: stat fps
Expected: <1ms overhead
```

### Memory Usage
```
Console: stat memory
Monitor: Genesis.OSCServer and related objects
```

## Files Created/Modified for Steps 5 & 6

### New Files:
- `Source/Genesis53Sensor/GenesisDebugHUD.h` - Debug HUD header
- `Source/Genesis53Sensor/GenesisDebugHUD.cpp` - Debug HUD implementation
- `Content/cameraService/TESTING_GUIDE.md` - Full testing guide
- `Content/cameraService/QUICK_START_TESTING.md` - This file

### Modified Files:
- `Source/Genesis53Sensor/GenesisOSCReceiver.h` - Added status tracking
- `Source/Genesis53Sensor/GenesisOSCReceiver.cpp` - Added connection health & warning handling
- `Content/cameraService/PythonClient/mqtt_osc_bridge.py` - Added OSC warning messages

## Expected Debug HUD Output (Success)

**Remember:** Debug HUD only appears during **Play In Editor** mode!

```
=== GENESIS MQTT/OSC DEBUG MONITOR ===
✓ RECEIVER ENABLED

--- CONNECTION STATUS ---
✓ CONNECTED - Receiving Data
MQTT Broker: 10.0.0.17:1883
OSC Listening: 127.0.0.1:8000

--- GEOSPATIAL POSITION ---
Latitude:     38.912345°
Longitude:    -77.543210°
Altitude MSL: 425.50 ft (129.69 m)
✓ Cesium Georeference: CesiumGeoreference_2

--- CAMERA 1 DATA ---
Offset (cm):  X=150.00 Y=75.00 Z=200.00
Rotation:     Roll=0.00 Pitch=-15.00 Yaw=45.00
✓ Camera Actor: CineCameraActor_OWL

--- SYSTEM STATUS ---
✓ All Systems Operational
Python Auto-Start: Enabled
```

## Monitoring Without Debug HUD (Editor Mode)

If you want to monitor the receiver **without entering Play mode**, use the Output Log:

1. **Open Output Log:**
   - Window → Developer Tools → Output Log
   - Or press Ctrl+Shift+` (backtick)

2. **Filter for Genesis messages:**
   - In the search box, type: `Genesis`
   - Or filter by category: `LogTemp`

3. **Watch for messages:**
   ```
   GenesisOSCReceiver: Listening for OSC on 127.0.0.1:8000
   GenesisOSCReceiver: OSC packet received from 127.0.0.1:8000 | Address: /genesis/latitude
   GenesisOSCReceiver: Georeference updated -> Lat:38.912345 Lon:-77.543210
   ```

**Note:** These log messages appear during **Play mode only**. The receiver does not process data while in editor viewport editing mode.

## Troubleshooting Commands

```powershell
# Rebuild Python venv if corrupt
cd Content\cameraService\PythonClient
rm -r venv -Force
python -m venv venv
.\venv\Scripts\pip install paho-mqtt python-osc

# Check Python dependencies
.\venv\Scripts\pip list

# Test MQTT connection with Python script
.\venv\Scripts\python.exe mqtt_osc_bridge.py --broker 10.0.0.17 --port 1883 --osc-ip 127.0.0.1 --osc-port 8000

# View Unreal Engine logs
Get-Content "Saved\Logs\Genesis53Sensor.log" -Tail 50 -Wait
```

## Next Actions After Successful Testing

1. ✓ Verify all checkboxes in TESTING_GUIDE.md
2. ✓ Update mqttFeaturePlan.md (mark steps 5 & 6 complete)
3. → Proceed to Step 7: Documentation & User Guide
4. → Consider packaging as a Plugin for reusability
5. → Set up automated testing pipeline

## Support

For detailed testing procedures, see: `Content/cameraService/TESTING_GUIDE.md`

For implementation details, see: `Content/cameraService/mqttFeaturePlan.md`
