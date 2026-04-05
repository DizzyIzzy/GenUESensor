import json
import os
import time
import argparse
import paho.mqtt.client as mqtt
from pythonosc.udp_client import SimpleUDPClient

# Default Config
DEFAULT_BROKER_IP = "10.0.0.74"
DEFAULT_BROKER_PORT = 1883
DEFAULT_OSC_IP = "127.0.0.1"
DEFAULT_OSC_PORT = 8000 # Default unreal OSC port

# Target channels we need for the sprint
TARGET_CHANNELS = [
    "latitude", "longitude", "altitudeMSL",
    "camera1Xoffset", "camera1Yoffset", "camera1Zoffset",
    "camera1Xrotation", "camera1Yrotation", "camera1Zrotation"
]

# Track state
active_topics = {} 
using_raw = {} 

def load_topics(json_path):
    with open(json_path, 'r') as f:
        channel_data = json.load(f)
    return channel_data.get("topics", {})

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"[MQTT] Connected with result code {reason_code}")
    
    topics_map = userdata['topics_map']
    
    # Subscribe to processed topics first
    for channel in TARGET_CHANNELS:
        if channel in topics_map:
            proc_topic = topics_map[channel]["processed"]
            
            # Map topic -> channel
            active_topics[proc_topic] = channel
            using_raw[channel] = False
            
            client.subscribe(proc_topic)
            print(f"[MQTT] Subscribed to {proc_topic} for {channel}")

def on_message(client, userdata, msg):
    topics_map = userdata['topics_map']
    osc_client = userdata['osc_client']
    
    topic = msg.topic
    payload = msg.payload.decode('utf-8')
    
    channel = active_topics.get(topic)
    if not channel:
        return

    # Fallback Logic: Empty or null processed topic 
    if not payload or payload.strip() == "" or payload.strip() == "null":
        if not using_raw.get(channel, False):
            print(f"\n[WARNING] Empty payload on processed topic '{topic}' for channel '{channel}'. Rolling over to raw topic.\n")
            
            # Switch to raw topic
            raw_topic = topics_map[channel]["raw"]
            
            client.unsubscribe(topic)
            active_topics.pop(topic, None)
            
            active_topics[raw_topic] = channel
            using_raw[channel] = True
            
            client.subscribe(raw_topic)
            print(f"[MQTT] Subscribed to RAW topic {raw_topic} for {channel}")
        return

    try:
        value = float(payload)
        
        # We will use /genesis/channel as our OSC address protocol
        # Example: /genesis/latitude
        osc_address = f"/genesis/{channel}"
        
        osc_client.send_message(osc_address, value)
        # Uncomment below to debug prints
        # print(f"[OSC] Sent {osc_address} : {value}")
        
    except ValueError:
        print(f"[ERROR] Could not parse payload as float: {payload}")

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