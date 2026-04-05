# Genesis MQTT Integration Feature Sprint Plan

## Overview
Integrate an MQTT feed to drive real-time geospatial positioning and camera control within Unreal Engine. The environment utilizes the Cesium plugin for accurate geospatial transformations. This is in unreal engine 5.5, with off word live plugins, mqtt experimental plugin, and cesium geospatial plugins alread installed.

## Configuration
*   **Default Broker IP:** 10.0.0.74
*   **Default Port:** 1883
*   *Note: Broker IP and Port must be exposed as settable parameters (e.g., in Blueprint/Project Settings), but default to the values above.*

## Data Source
*   Channels and topics are defined in channelList.json.
*   **Feed Scope for this Sprint:** Focus on camera1 data first.

## User Stories & Requirements
1. **As a user,** I want to parse the core position data (Latitude, Longitude, Altitude MSL in feet) from the MQTT feed and apply it as the primary transform position of the actor using Cesium's geospatial tools.
2. **As a user,** I want to parse camera-specific offset and rotation data from MQTT and apply it to a specific camera component in Unreal Engine (targeting the OWL Cine Camera initially).
    *   **Position Offset:** In meters, representing the transform offset from the primary streamed lat/long/alt position (center of gravity).
    *   **Rotation:** Reflects the camera's being pointed away from the forward direction.
3. **As a user,** I want the application to automatically use the processed topics. If processed topics are empty, I want a UI/debug warning raised with the option to roll over to 
aw topics.

## To-Do List

### 0. Agent Team Setup & Assignment
- [ ] **Manager Agent**: Oversees the project, orchestrates sub-agents, and verifies final integration.
- [ ] **Python & Middleware Agent**: Handles external Python script, MQTT connections (`paho-mqtt`), JSON parsing, and OSC/WebSocket transmission to Unreal. (Assigned Steps 1-2)
- [ ] **Blueprint & UI Agent**: Handles receiving live data in Unreal Editor/Runtime, Cesium integration, rendering offsets, and UMG interfaces. (Assigned Steps 3-5)
- [ ] **QA & Validation Agent**: Executes testing, validation, and verifies fallback scenarios. (Assigned Step 6)
- [ ] **Technical Writing Agent**: Produces comprehensive project documentation and instructions. (Assigned Step 7)

### 0.5 Version Control Setup
- [x] Initialize a Git repository with standard Unreal Engine `.gitignore`.
- [x] Connect the local repository to a new or existing remote GitHub repository matching your settings.

### 1. Python Environment Setup & Configuration (Assigned to: Python & Middleware Agent)
- [x] Create a standalone Python virtual environment (`venv`) for development to isolate dependencies.
- [x] Install required Python packages (`paho-mqtt` for MQTT connection, `python-osc` to bridge to Unreal Engine).
- [x] Enable Unreal Engine's native OSC (Open Sound Control) plugin to receive data from Python.

### 2. Python MQTT Subscription & Parsing (Assigned to: Python & Middleware Agent)
- [x] Develop a Python script that connects to the broker (IP: 10.0.0.74, Port: 1883).
- [x] Read and parse `channelList.json` in Python to extract relevant subscription topics (Lat, Long, Alt, and camera1 components).
- [x] Extract positional floats/doubles (Lat, Long, Alt) and camera rotation/offsets from the incoming JSON payload.
- [x] **Topic Fallback Logic:** Add logic in Python to detect if processed topics return empty data. If empty, print a warning and roll over to raw topics.
- [x] Package the parsed float data into OSC messages and transmit them to a local Unreal Engine OSC server port.

### 3. Core Geospatial Integration (Cesium Tools) (Assigned to: Blueprint & UI Agent)
- [ ] Create an Unreal Engine Blueprint (OSC Server) that listens for the packed Python messages.
- [ ] Implement a Blueprint function to convert incoming Altitude MSL (feet) to meters as required by Cesium/Unreal.
- [ ] Use `CesiumGeoreference` Blueprint nodes to transform absolute Latitude, Longitude, and Altitude coordinates into Unreal Engine world coordinates.
- [ ] Update the primary Actor's world transform based on the converted data.

### 4. Camera Offset Integration (Assigned to: Blueprint & UI Agent)
- [ ] Receive OSC messages containing camera1 offset data (X, Y, Z in meters) and rotation data.
- [ ] Create a generic, modular Blueprint function to calculate the local transform relative to the main Actor's center of gravity.
- [ ] Apply the offset transform and rotations specifically to a target OWL Cine Camera via Blueprint.

### 5. Debugging & UI Integration (Assigned to: Blueprint & UI Agent)
- [ ] Create an Editor utility or debugging UI in Unreal that visualizes the incoming OSC stream from the Python script.
- [ ] Surface any fallback warnings (Raw vs Processed topics) routed from Python or detected locally via the UI.

### 6. Testing & Validation (Assigned to: QA & Validation Agent)
- [ ] Verify connection to 10.0.0.74:1883 in Editor.
- [ ] Validate standard movement (Lat/Long/Alt updates map to correct Cesium coordinates visually).
- [ ] Validate camera1 offsets and rotations update smoothly and in the correct coordinate space relative to the primary transform.
- [ ] Test the pipeline using the OWL Cine Camera to verify plugin compatibility.
- [ ] Trigger an empty processed topic event to test the fallback to 
aw topics.

### 7. Documentation & User Guide (Assigned to: Technical Writing Agent)
- [ ] Create a comprehensive user guide detailing how the MQTT tool works and how it integrates with Unreal Engine.
- [ ] Write step-by-step instructions for installing necessary dependencies, including the Cesium for Unreal plugin.
- [ ] Document the process for modifying configuration parameters like the Default Broker IP and Port.
- [ ] Explain how to map additional channels from channelList.json if future inputs are required.
