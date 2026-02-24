import streamlit as st
import os
from PIL import Image
import pandas as pd

# Config
ALERTS_DIR = "/home/vikas_vashistha/security_daemon/alerts"
LOG_FILE   = "/home/vikas_vashistha/security_daemon/events.log"

st.set_page_config(
    page_title="Security System Dashboard",
    page_icon="🔒",
    layout="wide"
)

# Auto refresh every 5 seconds
st.markdown(
    '<meta http-equiv="refresh" content="5">',
    unsafe_allow_html=True
)

st.title("🔒 Smart Security System")
st.markdown("**Real-time monitoring dashboard**")

def parse_logs():
    events = []
    if not os.path.exists(LOG_FILE):
        return events
    with open(LOG_FILE, "r") as f:
        for line in f.readlines():
            try:
                parts      = line.strip().split("|")
                timestamp  = parts[0].split("]")[0].replace("[","").strip()
                alert_type = "FIRE" if "FIRE" in parts[0] else "INTRUDER"
                temp       = parts[1].replace("Temp:","").replace("C","").strip()
                humidity   = parts[2].replace("Humidity:","").replace("%","").strip()
                distance   = parts[3].replace("Distance:","").replace("cm","").strip()
                events.append({
                    "timestamp": timestamp,
                    "type":      alert_type,
                    "temp":      float(temp),
                    "humidity":  float(humidity),
                    "distance":  float(distance)
                })
            except:
                continue
    return events

def get_images():
    if not os.path.exists(ALERTS_DIR):
        return []
    files = sorted(os.listdir(ALERTS_DIR), reverse=True)
    return [f for f in files if f.endswith(".jpg")]

events = parse_logs()
images = get_images()

# Stats Row
col1, col2, col3, col4 = st.columns(4)
fire_count     = sum(1 for e in events if e["type"] == "FIRE")
intruder_count = sum(1 for e in events if e["type"] == "INTRUDER")
last_temp      = events[-1]["temp"]     if events else 0
last_hum       = events[-1]["humidity"] if events else 0

col1.metric("🔥 Fire Alerts",     fire_count)
col2.metric("🚨 Intruder Alerts", intruder_count)
col3.metric("🌡️ Last Temp",       f"{last_temp}°C")
col4.metric("💧 Last Humidity",   f"{last_hum}%")

st.divider()

left, right = st.columns([1, 1])

# Recent Images
with left:
    st.subheader("📸 Recent Captures")
    shown = 0
    for img_file in images:
        if shown >= 6:
            break
        img_path   = os.path.join(ALERTS_DIR, img_file)
        alert_type = "🔥 FIRE" if "FIRE" in img_file else "🚨 INTRUDER"
        timestamp  = img_file.replace(".jpg","").split("_",1)[1]
        try:
            if os.path.getsize(img_path) > 500:
                img = Image.open(img_path)
                st.image(img, caption=f"{alert_type} — {timestamp}", width=400)
                shown += 1
        except:
            continue
    if shown == 0:
        st.info("No captures yet — trigger an alert!")

# Event Log
with right:
    st.subheader("📋 Event Log")
    if events:
        for e in reversed(events[-15:]):
            color = "🔥" if e["type"] == "FIRE" else "🚨"
            st.markdown(
                f"{color} **{e['type']}** — {e['timestamp']}  \n"
                f"Temp: {e['temp']}°C | Humidity: {e['humidity']}% | Distance: {e['distance']}cm"
            )
            st.divider()
    else:
        st.info("No events yet")

# Temperature Chart
if len(events) > 1:
    st.subheader("🌡️ Temperature Over Time")
    df = pd.DataFrame(events)
    st.line_chart(df[["temp", "humidity"]])
