import json
import os
import time
import argparse
import paho.mqtt.client as mqtt
from pythonosc.udp_client import SimpleUDPClient

# Default Config
DEFAULT_BROKER_IP = "10.0.0.17"
DEFAULT_BROKER_PORT = 1883
DEFAULT_OSC_IP = "127.0.0.1"
DEFAULT_OSC_PORT = 8000 # Default unreal OSC port

# Target channels we need for the sprint
TARGET_CHANNELS = [
    "latitude", "longitude", "altitudeMSL",
    "camera1Xoffset", "camera1Yoffset", "camera1Zoffset",
    "camera1Xrotation", "camera1Yrotation", "camera1Zrotation",
    "camera1Zoom", "camera1Xfov", "camera1Yfov", "camera1Spectrum"
]

# Track state: channel -> which topic is currently "winning" (proc preferred)
active_topics = {}   # topic_string -> channel_name
topic_priority = {}  # channel_name -> "proc" | "raw"  (proc wins when both live)

def load_topics(json_path):
    with open(json_path, 'r') as f:
        channel_data = json.load(f)
    return channel_data.get("topics", {})

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"[MQTT] Connected with result code {reason_code}")

    topics_map = userdata['topics_map']

    # Subscribe to BOTH proc and raw for every channel at startup.
    # Proc is preferred; raw is the automatic fallback if proc is silent.
    for channel in TARGET_CHANNELS:
        if channel not in topics_map:
            continue
        proc_topic = topics_map[channel]["processed"]
        raw_topic  = topics_map[channel]["raw"]

        active_topics[proc_topic] = channel
        active_topics[raw_topic]  = channel
        topic_priority[channel]   = "raw"    # default to raw; upgrades to proc when proc data arrives

        client.subscribe(proc_topic)
        client.subscribe(raw_topic)
        print(f"[MQTT] Subscribed to {proc_topic} and {raw_topic} for {channel}")

def on_message(client, userdata, msg):
    topics_map = userdata['topics_map']
    osc_client = userdata['osc_client']

    topic   = msg.topic
    payload = msg.payload.decode('utf-8').strip()

    channel = active_topics.get(topic)
    if not channel:
        return

    # Determine whether this message is from the proc or raw topic
    is_proc = (topic == topics_map[channel]["processed"])
    src_label = "proc" if is_proc else "raw"

    # Drop raw messages when the proc topic is actively delivering data
    if not is_proc and topic_priority.get(channel) == "proc":
        return

    # Empty / null payload — mark this source as inactive
    if not payload or payload == "null":
        if is_proc:
            # proc went silent — promote raw
            if topic_priority.get(channel) != "raw":
                warning_msg = f"'{channel}' proc topic silent/empty — switching to raw fallback."
                print(f"[WARNING] {warning_msg}")
                # Send warning to Unreal via OSC
                osc_client.send_message("/genesis/warning", warning_msg)
                topic_priority[channel] = "raw"
        return

    # A proc message arrived — (re-)assert proc as winner
    if is_proc:
        topic_priority[channel] = "proc"

    try:
        # Payloads are either a plain float or a JSON object {"v": <float>, ...}
        try:
            parsed = json.loads(payload)
            value = float(parsed["v"])
        except (json.JSONDecodeError, KeyError, TypeError):
            value = float(payload)

        osc_address = f"/genesis/{channel}"
        osc_client.send_message(osc_address, value)
        print(f"[OSC] {src_label} -> {osc_address} : {value:.4f}")

    except ValueError:
        print(f"[ERROR] Could not parse payload as float for '{channel}': {payload}")

def main():
    parser = argparse.ArgumentParser(description="MQTT to OSC Bridge for Unreal Engine")
    parser.add_argument("--broker", default=DEFAULT_BROKER_IP, help="MQTT Broker IP")
    parser.add_argument("--port", type=int, default=DEFAULT_BROKER_PORT, help="MQTT Broker Port")
    parser.add_argument("--osc-ip", default=DEFAULT_OSC_IP, help="Unreal Engine OSC UDP Server IP")
    parser.add_argument("--osc-port", type=int, default=DEFAULT_OSC_PORT, help="Unreal Engine OSC UDP Server Port")
    args = parser.parse_args()

    json_path = os.path.join(os.path.dirname(__file__), "..", "channelList.conf")
    if not os.path.exists(json_path):
        print(f"[ERROR] channelList.conf not found at {json_path}")
        return

    topics_map = load_topics(json_path)
    
    print(f"Starting MQTT to OSC Bridge...")
    print(f"Targeting MQTT Broker: {args.broker}:{args.port}")
    print(f"Targeting OSC Server:  {args.osc_ip}:{args.osc_port}")
    
    osc_client = SimpleUDPClient(args.osc_ip, args.osc_port)

    # Note: Using MQTTv5 callback standard
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.user_data_set({
        "topics_map": topics_map,
        "osc_client": osc_client
    })
    
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(args.broker, args.port, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n[INFO] Disconnecting...")
        client.disconnect()
    except Exception as e:
        print(f"[ERROR] Connection failed: {e}")

if __name__ == "__main__":
    main()