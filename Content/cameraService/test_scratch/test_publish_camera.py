"""
Disposable test script — publishes fake camera data to the broker
so the OSC bridge and Unreal pipeline can be validated end-to-end.
Delete this folder when done.
"""
import time
import math
import paho.mqtt.client as mqtt

BROKER = "10.0.0.17"
PORT   = 1883

topics = {
    "gen/raw/camera/1/xoffset":   0.0,
    "gen/raw/camera/1/yoffset":   -7.5,
    "gen/raw/camera/1/zoffset":   -0.5,
    "gen/raw/camera/1/xrotation": -10.0,
    "gen/raw/camera/1/yrotation": 0.0,
    "gen/raw/camera/1/zrotation": 0.0,
    "gen/raw/camera/1/zoom":      1.0,
    "gen/raw/camera/1/xfov":      60.0,
    "gen/raw/camera/1/yfov":      45.0,
}

c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
c.connect(BROKER, PORT)
c.loop_start()

print(f"Publishing camera test data to {BROKER}:{PORT} — Ctrl+C to stop")
t = 0.0
while True:
    # Slowly sweep yrotation so movement is visible in Unreal
    topics["gen/raw/camera/1/yrotation"] = round(math.sin(t) * 30.0, 3)
    topics["gen/raw/camera/1/zrotation"] = round(math.cos(t) * 15.0, 3)

    for topic, value in topics.items():
        c.publish(topic, str(value))

    print(f"  yrotation={topics['gen/raw/camera/1/yrotation']:+.1f}  zrotation={topics['gen/raw/camera/1/zrotation']:+.1f}")
    t += 0.1
    time.sleep(0.1)
