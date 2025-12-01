#!/usr/bin/env python3
"""
Server for receiving sensor data from LoRa Gateway
Compatible with both old mock client and new LoRa gateway
"""

import socketserver
from pathlib import Path

from database import initialize_database, DB_PATH
from handlers import SensorServerHandler

BASE_DIR = Path(__file__).resolve().parent
HOST = '0.0.0.0'
PORT = 8080


def start_server() -> None:
    """Start the HTTP server"""
    initialize_database()
    
    with socketserver.TCPServer((HOST, PORT), SensorServerHandler) as httpd:
        print(f"\n{'='*60}")
        print(f"  Sensor Data Server")
        print(f"{'='*60}")
        print(f"  Server running at http://{HOST}:{PORT}")
        print(f"  Dashboard: http://localhost:{PORT}/")
        print(f"  API endpoint: http://localhost:{PORT}/api/sensor-data")
        print(f"  Database: {DB_PATH}")
        print(f"{'='*60}")
        print(f"\nWaiting for sensor data... (Press Ctrl+C to stop)\n")
        
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n\n✓ Server stopped.")


if __name__ == "__main__":
    start_server()
