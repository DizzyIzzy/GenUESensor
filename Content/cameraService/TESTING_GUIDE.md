# Genesis MQTT Integration - Testing & Validation Guide

This guide provides step-by-step instructions for testing and validating the MQTT/OSC integration in Unreal Engine.

## Prerequisites

Before testing, ensure:
- [ ] Unreal Engine 5.5 project is open
- [ ] Python virtual environment is set up at `Content/cameraService/PythonClient/venv`
- [ ] `channelList.conf` is properly configured
- [ ] MQTT broker is running and accessible at configured IP:Port (default: 10.0.0.17:1883)

## Step 1: Compile and Setup

### 1.1 Compile C++ Code
1. Close Unreal Editor if open
2. Right-click `Genesis53Sensor.uproject` → "Generate Visual Studio project files"
3. Open `Genesis53Sensor.sln` in Visual Studio
4. Build the solution (F7) - ensure no errors
5. Launch the editor from Visual Studio (F5) or directly open the `.uproject`

### 1.2 Add GenesisOSCReceiver to Level
1. Open your test level (e.g., `Content/SimBlank/Levels/SimBlank.umap`)
2. In the **Place Actors** panel, search for `GenesisOSCReceiver`
3. Drag it into the level
4. Select the actor in the Outliner

### 1.3 Configure GenesisOSCReceiver
In the **Details** panel:

**OSC Configuration:**
- `Receive IP`: `127.0.0.1` (default)
- `Receive Port`: `8000` (default)

**MQTT Bridge Configuration:**
- `bAutoStartPythonBridge`: `✓ true` (checked)
- `MQTT Broker IP`: `10.0.0.17` (or your broker IP)
- `MQTT Broker Port`: `1883`

**Genesis Wiring:**
- `Georeference Target`: Drag your `CesiumGeoreference` actor from the Outliner
- `Camera1 Actor`: Drag your OWL Cine Camera actor from the Outliner

### 1.4 Setup Debug HUD
1. Open your GameMode Blueprint (e.g., `BP_SimGameMode`)
2. Find the **HUD Class** setting
3. Set it to `GenesisDebugHUD`
4. Save and compile

Alternatively, for quick testing:
1. Press ` (backtick) to open console
2. Type: `showdebug` and press Enter to see debug info

## Step 2: Connection Validation

### 2.1 Start Play in Editor
1. Click **Play** button in Unreal Editor
2. The Python bridge should auto-start (check Output Log for confirmation)
3. Look for log messages:
   ```
   GenesisOSCReceiver: Listening for OSC on 127.0.0.1:8000
   GenesisOSCReceiver: Launching Python Bridge: ...
   ```

### 2.2 Verify MQTT Connection
Check the debug HUD (should appear on screen):

**Expected Display:**
```
=== GENESIS MQTT/OSC DEBUG MONITOR ===
--- CONNECTION STATUS ---
✓ CONNECTED - Receiving Data
MQTT Broker: 10.0.0.17:1883
OSC Listening: 127.0.0.1:8000
```

**If showing "✗ NO DATA":**
1. Check Output Log for Python bridge errors
2. Manually test Python bridge:
   ```powershell
   cd Content\cameraService\PythonClient
   .\venv\Scripts\python.exe mqtt_osc_bridge.py --broker 10.0.0.17 --port 1883
   ```
3. Verify MQTT broker is accessible:
   ```powershell
   Test-NetConnection -ComputerName 10.0.0.17 -Port 1883
   ```

### 2.3 Verify Broker IP
The default broker IP in the plan is **10.0.0.74**, but current code shows **10.0.0.17**.

**Action Required:** Confirm correct broker IP with your network admin and update if needed.

## Step 3: Geospatial Data Validation

### 3.1 Monitor Incoming Position Data
Watch the debug HUD **GEOSPATIAL POSITION** section:

**Expected Values:**
```
--- GEOSPATIAL POSITION ---
Latitude:     38.XXXXXX°
Longitude:    -77.XXXXXX°
Altitude MSL: XXXX.XX ft (XXXX.XX m)
✓ Cesium Georeference: CesiumGeoreference_X
```

### 3.2 Validate Coordinate Transformation
1. Note the Lat/Long/Alt values from the debug HUD
2. Verify they match expected real-world coordinates
3. Check that the Cesium world origin updates (the map should pan/zoom to location)

**Troubleshooting:**
- If coordinates are zeros: MQTT topics may be misconfigured in `channelList.conf`
- If showing warning about Georeference: Assign the `CesiumGeoreference` actor in Details panel

### 3.3 Test Altitude Conversion
The system converts altitude from feet MSL to meters:
- Formula: `meters = feet × 0.3048`
- Verify the conversion on the debug HUD displays correctly

## Step 4: Camera Offset & Rotation Validation

### 4.1 Monitor Camera1 Data
Watch the debug HUD **CAMERA 1 DATA** section:

**Expected Display:**
```
--- CAMERA 1 DATA ---
Offset (cm):  X=XXX.XX Y=XXX.XX Z=XXX.XX
Rotation:     Roll=XX.XX Pitch=XX.XX Yaw=XX.XX
✓ Camera Actor: CineCameraActor_X
```

### 4.2 Validate Offset Application
1. **Verify Unit Conversion:** Offsets arrive in meters and convert to centimeters (×100)
2. **Visual Check:** 
   - Move the platform actor in the level
   - Camera should maintain its relative offset from the center
   - Offset should match the debug HUD values

### 4.3 Validate Rotation Application
1. **Check Rotation Mapping:**
   - `camera1Xrotation` → Roll
   - `camera1Yrotation` → Pitch
   - `camera1Zrotation` → Yaw
2. **Visual Check:**
   - Camera should point in the direction indicated by rotation values
   - Rotation should be relative to the platform orientation

### 4.4 Test with OWL Cine Camera
1. Ensure the Camera1Actor is set to your **OWL Cine Camera** instance
2. Verify smooth updates (no stuttering or lag)
3. Check that camera FOV and other properties are not being overwritten

## Step 5: Fallback Behavior Validation

### 5.1 Trigger Processed Topic Fallback
To test the automatic rollover from processed → raw topics:

**Option A: Edit channelList.conf**
1. Stop playback
2. Open `Content/cameraService/channelList.conf`
3. Set a processed topic to an invalid/non-existent topic name
4. Save and restart play
5. Watch for warning message on debug HUD

**Option B: Simulate Empty Messages**
1. Use an MQTT test tool to publish empty messages to processed topics
2. Python bridge should detect and switch to raw

**Expected Behavior:**
- Debug HUD shows: `⚠ 'channel_name' proc topic silent/empty — switching to raw fallback.`
- Data continues flowing from raw topics
- Output Log shows warning

### 5.2 Verify Warning Display
The debug HUD should show any fallback warnings in the **SYSTEM STATUS** section:
```
--- SYSTEM STATUS ---
⚠ 'latitude' proc topic silent/empty — switching to raw fallback.
```

## Step 6: Performance & Stability Testing

### 6.1 Monitor Frame Rate
1. Enable FPS display: Console command `stat fps`
2. Verify no significant performance impact from MQTT/OSC processing
3. Target: <1ms overhead per frame

### 6.2 Long-Duration Test
1. Let the simulation run for 10+ minutes
2. Monitor for:
   - Memory leaks (stat memory)
   - Connection drops
   - Data flow consistency

### 6.3 Reconnection Testing
1. **Disconnect MQTT Broker:**
   - Stop the broker service temporarily
   - Verify debug HUD shows connection error
   - Restart broker
   - Verify auto-reconnection

2. **Restart Python Bridge:**
   - Stop the Python script (Ctrl+C in terminal if running manually)
   - The auto-start feature should handle this on next play session

## Step 7: Integration Testing Checklist

Run through this complete checklist:

- [ ] **Connection Tests**
  - [ ] MQTT broker connection established
  - [ ] OSC server receiving messages
  - [ ] Python bridge auto-starts correctly
  - [ ] Manual Python bridge launch works

- [ ] **Data Flow Tests**
  - [ ] Latitude updates in real-time
  - [ ] Longitude updates in real-time
  - [ ] Altitude MSL updates and converts correctly
  - [ ] Camera1 X/Y/Z offsets update
  - [ ] Camera1 Roll/Pitch/Yaw rotations update

- [ ] **Geospatial Tests**
  - [ ] Cesium Georeference updates world origin
  - [ ] Coordinates map to correct real-world location
  - [ ] Altitude conversion (feet → meters) is accurate

- [ ] **Camera Tests**
  - [ ] Camera offset applies correctly (meters → centimeters)
  - [ ] Camera rotation applies correctly
  - [ ] Camera moves relative to platform
  - [ ] OWL Cine Camera plugin compatibility confirmed

- [ ] **Fallback Tests**
  - [ ] Processed topic preference works
  - [ ] Raw topic fallback triggers on empty processed
  - [ ] Warning messages display on HUD
  - [ ] Warning messages log to Output window

- [ ] **UI/Debug Tests**
  - [ ] Debug HUD displays all sections
  - [ ] Connection status accurate
  - [ ] Real-time value updates visible
  - [ ] Color coding works (green/yellow/red)
  - [ ] Warnings display prominently

- [ ] **Error Handling Tests**
  - [ ] Missing Georeference actor shows warning
  - [ ] Missing Camera actor shows warning
  - [ ] Invalid MQTT broker address fails gracefully
  - [ ] Python bridge failure shows user-friendly message

## Troubleshooting Reference

### Issue: No OSC Messages Received
**Solutions:**
1. Check Windows Firewall isn't blocking UDP port 8000
2. Run: `netstat -an | findstr "8000"` to verify port is listening
3. Verify Python bridge is running (check Task Manager for python.exe)
4. Check Output Log for OSC server creation errors

### Issue: Coordinates Stuck at Zero
**Solutions:**
1. Verify MQTT broker has active publishers on subscribed topics
2. Check `channelList.conf` topic names match broker topics exactly
3. Use MQTT Explorer tool to monitor broker messages
4. Enable Python script verbose logging

### Issue: Camera Not Moving
**Solutions:**
1. Verify Camera1Actor is assigned in Details panel
2. Check camera is child of or attached to georeference actor
3. Verify camera offset values are non-zero in debug HUD
4. Check camera isn't locked in editor viewport

### Issue: Python Bridge Won't Start
**Solutions:**
1. Verify venv exists at `Content/cameraService/PythonClient/venv`
2. Check venv has required packages: `pip list` should show `paho-mqtt` and `python-osc`
3. Manually run bridge to see error messages
4. Check file paths in launch command (see Output Log)

### Issue: Fallback Warnings Not Showing
**Solutions:**
1. Verify Python bridge is sending warnings via `/genesis/warning` OSC address
2. Check OSCReceiver OnOSCMessageReceived handles `/genesis/warning`
3. Enable more verbose logging in Python script
4. Manually send test warning: `osc_client.send_message("/genesis/warning", "Test Warning")`

## Success Criteria

All tests pass when:

✓ MQTT connection establishes to broker (10.0.0.17:1883 or configured IP)  
✓ OSC messages flow from Python bridge to Unreal  
✓ Latitude, Longitude, Altitude update in real-time  
✓ Cesium Georeference transforms coordinates correctly  
✓ Camera offsets apply in correct units (centimeters)  
✓ Camera rotations apply correctly  
✓ OWL Cine Camera operates smoothly with live data  
✓ Processed topics are preferred over raw topics  
✓ Fallback to raw topics works when processed are empty  
✓ Warning messages display on HUD and in logs  
✓ Debug HUD provides clear status and data visualization  
✓ No performance degradation during sustained operation  

## Next Steps After Testing

Once all tests pass:
1. Update `mqttFeaturePlan.md` to mark Steps 5 & 6 complete
2. Proceed to Step 7: Documentation & User Guide
3. Consider adding automated tests for CI/CD pipeline
4. Document any configuration changes or gotchas discovered during testing
