#!/usr/bin/env python3
"""
Mock server for testing the ESP32 LoRa Environment Monitor dashboard.
Generates random sensor data every 10 seconds.
"""

import http.server
import socketserver
import json
import random
import threading
import time
from datetime import datetime
from pathlib import Path

HOST = '127.0.0.1'
PORT = 8888
BASE_DIR = Path(__file__).resolve().parent

# Simulated sensor data storage
sensor_data = []
alerts = []

# Node configurations
NODES = [
    {"id": "NODE_01", "name": "Garden Bed A"},
    {"id": "NODE_02", "name": "Garden Bed B"},
    {"id": "NODE_03", "name": "Greenhouse"},
]


def generate_random_data():
    """Generate random sensor data for all nodes."""
    global sensor_data, alerts
    
    new_data = []
    timestamp = datetime.utcnow().isoformat() + "Z"
    
    for node in NODES:
        # Random sensor values
        temperature = random.uniform(15, 40)  # Temperature °C
        humidity = random.uniform(15, 95)  # Soil moisture %
        distance = random.randint(5, 200)  # Distance cm
        luminosity = random.randint(50, 15000)  # Luminosity lux
        battery = random.randint(20, 100)  # Battery %
        rssi = random.randint(-120, -40)   # RSSI dBm
        snr = random.uniform(-5, 15)       # SNR dB
        
        entry = {
            "node_id": node["id"],
            "timestamp": timestamp,
            "sensors": {
                "temperature_celsius": temperature,
                "humidity_percent": humidity,
                "distance_cm": distance,
                "luminosity_lux": luminosity,
            },
            "battery_percent": battery,
            "radio": {
                "rssi_dbm": rssi,
                "snr_db": snr,
            }
        }
        new_data.append(entry)
        
        # Generate alerts for critical conditions
        if humidity < 20:
            alerts.insert(0, {
                "timestamp": timestamp,
                "node_id": node["id"],
                "alert_type": "LOW_MOISTURE",
                "message": f"Low soil moisture: {humidity:.1f}%",
                "acknowledged": False
            })
        if battery < 25:
            alerts.insert(0, {
                "timestamp": timestamp,
                "node_id": node["id"],
                "alert_type": "LOW_BATTERY",
                "message": f"Low battery: {battery}%",
                "acknowledged": False
            })
        if rssi < -100:
            alerts.insert(0, {
                "timestamp": timestamp,
                "node_id": node["id"],
                "alert_type": "WEAK_SIGNAL",
                "message": f"Weak LoRa signal: {rssi} dBm",
                "acknowledged": False
            })
        if temperature > 35:
            alerts.insert(0, {
                "timestamp": timestamp,
                "node_id": node["id"],
                "alert_type": "HIGH_TEMP",
                "message": f"High temperature: {temperature:.1f}°C",
                "acknowledged": False
            })
    
    # Keep only last 10 alerts
    alerts = alerts[:10]
    sensor_data = new_data
    
    print(f"[{datetime.now().strftime('%H:%M:%S')}] Generated data for {len(NODES)} nodes")


def data_generator_thread():
    """Background thread that generates data every 10 seconds."""
    while True:
        generate_random_data()
        time.sleep(10)


class MockHandler(http.server.SimpleHTTPRequestHandler):
    """HTTP handler that serves static files and API endpoints."""
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(BASE_DIR), **kwargs)
    
    def do_GET(self):
        # API: Get sensor data
        if self.path == '/data':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps(sensor_data).encode())
            return
        
        # API: Get alerts
        if self.path == '/api/alerts':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps(alerts).encode())
            return
        
        # Serve static files (dashboard.html, style.css, etc.)
        return super().do_GET()
    
    def log_message(self, format, *args):
        # Quieter logging - only log non-data requests
        if '/data' not in args[0] and '/api/alerts' not in args[0]:
            super().log_message(format, *args)


def main():
    print("=" * 50)
    print("🌿 ESP32 LoRa Environment Monitor - Mock Server")
    print("=" * 50)
    print(f"📡 Serving on http://{HOST}:{PORT}")
    print(f"📊 Dashboard: http://{HOST}:{PORT}/dashboard.html")
    print(f"🔄 Generating random data every 10 seconds")
    print(f"📦 Simulating {len(NODES)} sensor nodes")
    print("=" * 50)
    print("Press Ctrl+C to stop\n")
    
    # Generate initial data
    generate_random_data()
    
    # Start background data generator
    thread = threading.Thread(target=data_generator_thread, daemon=True)
    thread.start()
    
    # Start HTTP server
    with socketserver.TCPServer((HOST, PORT), MockHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n\n🛑 Server stopped.")


if __name__ == '__main__':
    main()
