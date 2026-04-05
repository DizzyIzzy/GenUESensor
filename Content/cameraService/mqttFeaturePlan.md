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
- [ ] **Unreal C++ Expert Agent**: Handles C++ core logic, MQTT connections, parsers, and Cesium API integration. (Assigned Steps 1-4)
- [ ] **Blueprint & UI Agent**: Handles Blueprint parameter exposure, Editor utilities, and UMG interfaces. (Assigned Step 5)
- [ ] **QA & Validation Agent**: Executes testing, validation, and verifies fallback scenarios. (Assigned Step 6)
- [ ] **Technical Writing Agent**: Produces comprehensive project documentation and instructions. (Assigned Step 7)

### 0.5 Version Control Setup
- [x] Initialize a Git repository with standard Unreal Engine `.gitignore`.
- [x] Connect the local repository to a new or existing remote GitHub repository matching your settings.

### 1. Setup & Configuration (Assigned to: Unreal C++ Expert Agent)
- [ ] Create an Unreal Engine standard settings object or blueprint-exposable variables for MQTT connection parameters (IP: 10.0.0.74, Port: 1883).
- [ ] Enable and utilize Unreal Engine's native Experimental MQTT plugin as the core client.

### 2. MQTT Subscription & Parsing (Assigned to: Unreal C++ Expert Agent)
- [ ] Read and parse channelList.json to extract relevant subscription topics (Lat, Long, Alt, and camera1 components).
- [ ] Build a runtime unit test/logger for the MQTT JSON feed to inspect and determine the exact payload format dynamically.
- [ ] Implement a C++ MQTT listener component that subscribes to the processed topics by default.
- [ ] Create parsers for the incoming MQTT payload to extract positional floats/doubles (Lat, Long, Alt).
- [ ] **Topic Fallback Logic:** Add logic to detect if processed topics return empty data. If empty, trigger a debug/UI flag and automatically (or optionally) roll over to subscribe to raw topics.

### 3. Core Geospatial Integration (Cesium Tools) (Assigned to: Unreal C++ Expert Agent)
- [ ] Implement a C++ function to convert incoming Altitude MSL (feet) to meters as required by Cesium/Unreal.
- [ ] Use CesiumGeoreference or the Cesium C++ API to transform absolute Latitude, Longitude, and Altitude coordinates into Unreal Engine world coordinates.
- [ ] Update the primary Actor's world transform based on the converted data.

### 4. Camera Offset Integration (Assigned to: Unreal C++ Expert Agent)
- [ ] Extract camera1 offset data (camera1Xoffset, camera1Yoffset, camera1Zoffset in meters).
- [ ] Extract camera1 rotation data (camera1Xrotation, camera1Yrotation, camera1Zrotation).
- [ ] Create a generic, modular C++ function to calculate the local transform relative to the main Actor's center of gravity that can accept any camera type.
- [ ] Apply the offset transform and rotations specifically to a target OWL Cine Camera, ensuring it works seamlessly with standard Unreal Cine Cameras if swapped out.

### 5. Debugging & UI Integration (Assigned to: Blueprint & UI Agent)
- [ ] Create an Editor utility or debugging tool that connects and validates the MQTT feed natively in the Editor, bypassing the need to enter 'Play In Editor' (PIE) mode.
- [ ] Implement Blueprint-callable getters or delegates within the C++ code to broadcast real-time position and rotation data, making it directly accessible to the Unreal Engine UI (UMG) layer.
- [ ] Surface the "Empty Processed Topic / Using Raw Topic" warning in the UI via the previously mentioned flag.

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
