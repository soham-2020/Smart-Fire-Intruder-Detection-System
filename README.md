# Smart-Fire-Intruder-Detection-System
An embedded IoT security system that detects fire and intruders in real-time, captures photographic evidence, and streams live data to a Linux server with a web dashboard.


📋 Problem Statement
Traditional security systems are expensive, closed-source, and require professional installation. Homes and small offices in developing regions often lack affordable, intelligent monitoring solutions that can simultaneously detect fire hazards and unauthorized intrusions while providing visual evidence.
This project addresses that gap by building a low-cost, fully open-source embedded security system using commodity hardware. The system detects environmental threats in real time, captures timestamped photographic evidence automatically, and presents all data on a live web dashboard — all running on a Linux server with no cloud dependency.

🏗️ System Architecture
┌─────────────────────────────────────────────────────────┐
│              SENSOR LAYER (Arduino UNO R4 Minima)        │
│  DHT11  │  Flame Sensor  │  Ultrasonic  │  IR Receiver  │
│  Temp/  │  Fire          │  Intruder    │  Arm/Disarm    │
│  Humid  │  Detection     │  Distance    │  Remote        │
└──────────────────────────┬──────────────────────────────┘
                           │ Hardware UART (Serial1)
                           │ 9600 baud
                           ▼
┌─────────────────────────────────────────────────────────┐
│           EDGE PROCESSING LAYER (AI Thinker ESP32-CAM)   │
│                                                          │
│  FreeRTOS Task 1: UART Reader                           │
│  FreeRTOS Task 2: Camera Capture + HTTP POST            │
│  FreeRTOS Task 3: WiFi Monitor + Auto Reconnect         │
│                                                          │
│  On Alert → Capture JPEG → POST to Linux Server         │
└──────────────────────────┬──────────────────────────────┘
                           │ WiFi HTTP POST
                           │ Port 8080
                           ▼
┌─────────────────────────────────────────────────────────┐
│              SERVER LAYER (Linux / WSL Ubuntu)           │
│                                                          │
│  Pure C HTTP Daemon (server.c)                          │
│  ├── Receives JPEG images                               │
│  ├── Parses HTTP headers (alert type, temp, humidity)   │
│  ├── Saves timestamped .jpg to alerts/                  │
│  └── Logs events to events.log                          │
│                                                          │
│  systemd Service → Auto-starts on boot                  │
│  Streamlit Dashboard → Live web UI on :8501             │
└─────────────────────────────────────────────────────────┘

✨ Features

Multi-sensor fusion — DHT11, flame sensor, ultrasonic, and IR receiver working simultaneously
FreeRTOS multitasking — 3 concurrent tasks on ESP32-CAM for UART reading, camera capture, and WiFi monitoring
Hardware UART communication — Arduino R4 Minima → ESP32-CAM via Serial1 at 9600 baud
Automatic photo capture — ESP32-CAM captures JPEG image the moment an alert triggers
WiFi HTTP transmission — Images and sensor data sent over WiFi to Linux server
Pure C Linux daemon — Custom HTTP server written in C, no frameworks
systemd service — Daemon auto-starts on boot, auto-restarts on crash
Alert cooldown — 10 second cooldown prevents alert spam
IR arm/disarm — System can be armed or disarmed via any IR remote
LCD real-time display — 16x2 I2C LCD shows live temperature, humidity, and alert status
Timestamped event logging — All events logged to events.log with precise timestamps
Streamlit dashboard — Live web dashboard with alert counts, captured images, and temperature graph


🛠️ Hardware Requirements
ComponentPurposeArduino UNO R4 MinimaSensor hub, logic controllerAI Thinker ESP32-CAMWiFi, camera capture, FreeRTOSDHT11 (3-pin module)Temperature and humidity sensingFlame sensor moduleFire detectionHC-SR04 UltrasonicIntruder distance detectionIR receiver moduleRemote arm/disarm16x2 LCD with I2C moduleReal-time status displayCP2102 USB-to-SerialESP32-CAM programming

📌 Wiring Diagram
Arduino UNO R4 Minima
DHT11 DATA        → Pin 7
Flame Sensor DO   → Pin 6
Ultrasonic TRIG   → Pin 9
Ultrasonic ECHO   → Pin 10
IR Receiver OUT   → Pin 11
LCD SDA           → A4
LCD SCL           → A5
ESP32-CAM GPIO14  → Pin 1 (TX)
ESP32-CAM GPIO15  → Pin 0 (RX)
All VCC           → 5V
All GND           → GND
ESP32-CAM (AI Thinker)
Arduino 5V  → ESP32-CAM 5V
Arduino GND → ESP32-CAM GND
Arduino Pin 1 (TX) → GPIO14 (RX)
Arduino Pin 0 (RX) → GPIO15 (TX)

💻 Software Stack
LayerTechnologySensor firmwareArduino C++ (UNO R4 Minima)Edge firmwareESP32 Arduino framework + FreeRTOSCommunicationHardware UART + WiFi HTTPServerPure C (POSIX sockets)Process managementsystemdDashboardPython + Streamlit

🚀 Setup Instructions
1. Arduino UNO R4 Minima
Install these libraries in Arduino IDE:
DHT sensor library     (Adafruit)
Adafruit Unified Sensor (Adafruit)
LiquidCrystal I2C      (Frank de Brabander)
IRremote               (shirriff)
NewPing                (Tim Eckel)
Upload arduino_sensors/arduino_sensors.ino with board set to Arduino UNO R4 Minima.
2. ESP32-CAM
Install ESP32 board package in Arduino IDE:
Boards Manager → search "esp32" → install Espressif Systems
Upload esp32_cam/esp32_cam.ino with:

Board: AI Thinker ESP32-CAM
Update WiFi credentials and server IP in sketch

3. Linux Server (WSL/Ubuntu)
bash# Clone repo
git clone https://github.com/YOUR_USERNAME/smart-security-system
cd smart-security-system/linux_daemon

# Compile
make

# Create alerts directory
mkdir -p ~/security_daemon/alerts

# Install as systemd service
sudo cp security_daemon.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable security_daemon
sudo systemctl start security_daemon
4. Streamlit Dashboard
bashpip install streamlit pillow pandas
python3 -m streamlit run dashboard.py --server.address 0.0.0.0
Open browser at http://localhost:8501
5. Windows Port Forwarding (if using WSL)
netsh interface portproxy add v4tov4 listenaddress=YOUR_PC_IP listenport=8080 connectaddress=WSL_IP connectport=8080
netsh advfirewall firewall add rule name="Security Daemon" dir=in action=allow protocol=TCP localport=8080

Repository Structure
smart-security-system/
│
├── arduino_sensors/
│   └── arduino_sensors.ino      # Sensor reading + UART transmission
│
├── esp32_cam/
│   └── esp32_cam.ino            # FreeRTOS tasks + camera + WiFi
│
├── linux_daemon/
│   ├── server.c                 # Pure C HTTP daemon
│   ├── Makefile                 # Build system
│   └── security_daemon.service  # systemd service file
│
├── dashboard/
│   └── dashboard.py             # Streamlit web dashboard
│
└── README.md

How It Works

Arduino reads all sensors every 500ms
When flame or intruder detected, Arduino sends sensor data over Hardware UART to ESP32-CAM
ESP32-CAM FreeRTOS UART task receives the data
Alert data is pushed to a FreeRTOS queue
Camera task picks up alert from queue → captures JPEG
Image is HTTP POSTed to Linux server with sensor data in headers
C daemon receives image, saves to disk with timestamp, logs event
Streamlit dashboard reads logs and images, displays live updates every 5 seconds
10 second cooldown prevents spam captures


Skills Demonstrated

Embedded C / C++ programming
FreeRTOS multitasking (task creation, queues)
Hardware UART protocol (Serial communication between MCUs)
I2C protocol (LCD display)
WiFi HTTP client/server programming
Linux systems programming (POSIX sockets, file I/O)
systemd service deployment
Cross-device embedded system design
Makefile build system
Python dashboard development


Demo

https://youtu.be/bXGjp7xpIzE


<img width="1600" height="1200" alt="image" src="https://github.com/user-attachments/assets/9ff0b4bd-c9b2-4dea-a4df-d5c9ff23dd61" />


 Future Improvements

Replace Python dashboard with pure C web server
Add email/SMS alert notifications
Implement HTTPS for secure image transmission
Port to Yocto-based custom Linux image
Add OpenCV face recognition on Linux server
Battery backup with deep sleep mode on ESP32

 Author
Soham Vashistha

Built as a personal embedded systems project
Hardware: Arduino UNO R4 Minima + AI Thinker ESP32-CAM



📄 License
MIT License — feel free to use and modify
