#!/usr/bin/env python3
"""
Server for receiving sensor data from LoRa Gateway
Compatible with both old mock client and new LoRa gateway
"""

import http.server
import socketserver
import json
import sqlite3
from pathlib import Path
from datetime import datetime, timedelta
from typing import Any, Dict, List

BASE_DIR = Path(__file__).resolve().parent
DB_NAME = 'environmental_data.db'
DB_PATH = BASE_DIR / DB_NAME
HOST = '0.0.0.0'
PORT = 8080

gateway_stats_cache: Dict[int, Dict[str, Any]] = {}
server_start_time: datetime = None  # Tempo de início do servidor


def initialize_database() -> None:
    """Initialize SQLite database with sensor data table"""
    connection = sqlite3.connect(DB_PATH)
    cursor = connection.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS sensor_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            node_id TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            temperature_celsius REAL,
            humidity_percent REAL,
            distance_cm INTEGER,
            luminosity_lux INTEGER,
            presence_detected BOOLEAN,
            battery_percent INTEGER,
            rssi_dbm REAL,
            snr_db REAL,
            gateway_id INTEGER
        )
    ''')
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS gateway_stats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            gateway_id INTEGER NOT NULL,
            timestamp TEXT NOT NULL,
            uptime_seconds INTEGER,
            rx_total INTEGER,
            rx_valid INTEGER,
            rx_invalid INTEGER,
            rx_checksum_error INTEGER,
            packet_loss_percent REAL,
            tx_total INTEGER,
            tx_success INTEGER,
            tx_failed INTEGER,
            server_success_rate REAL,
            latency_avg_ms REAL,
            latency_min_ms INTEGER,
            latency_max_ms INTEGER,
            latency_last_ms INTEGER,
            energy_mah REAL,
            wifi_rssi INTEGER
        )
    ''')
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            node_id TEXT,
            alert_type TEXT NOT NULL,
            message TEXT NOT NULL,
            acknowledged BOOLEAN DEFAULT FALSE
        )
    ''')
    connection.commit()
    connection.close()
    print(f"✓ Database '{DB_NAME}' initialized.")


def save_sensor_data(data: Dict[str, Any]) -> None:
    """Save sensor data to database"""
    connection = sqlite3.connect(DB_PATH)
    cursor = connection.cursor()
    
    node_id = data.get('node_id', 'unknown')
    
    # Converter timestamp da ESP (milissegundos desde conexão) para timestamp absoluto
    esp_timestamp_ms = data.get('timestamp')
    if esp_timestamp_ms and isinstance(esp_timestamp_ms, (int, float)):
        # Somar os milissegundos da ESP ao tempo de início do servidor
        absolute_time = server_start_time + timedelta(milliseconds=esp_timestamp_ms)
        timestamp = absolute_time.isoformat()
    else:
        # Fallback para timestamp atual se não houver timestamp da ESP
        timestamp = datetime.utcnow().isoformat()
    
    sensors = data.get('sensors', data)
    radio = data.get('radio', {})
    
    cursor.execute('''
        INSERT INTO sensor_data (
            node_id, timestamp, temperature_celsius, humidity_percent, distance_cm,
            luminosity_lux, presence_detected, battery_percent, rssi_dbm, snr_db, gateway_id
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (
        node_id,
        timestamp,
        sensors.get('temperature_celsius', sensors.get('temperature')),
        sensors.get('humidity_percent', sensors.get('humidity')),
        sensors.get('distance_cm'),
        sensors.get('luminosity_lux', sensors.get('luminosity')),
        sensors.get('presence_detected'),
        data.get('battery_percent'),
        radio.get('rssi_dbm', sensors.get('rssi_dbm')),
        radio.get('snr_db', sensors.get('snr_db')),
        data.get('gateway_id')
    ))
    connection.commit()
    
    check_and_generate_alerts(data, connection)
    
    connection.close()


def save_gateway_stats(data: Dict[str, Any]) -> None:
    """Save gateway statistics to database"""
    connection = sqlite3.connect(DB_PATH)
    cursor = connection.cursor()
    
    gateway_id = data.get('gateway_id', 0)
    lora = data.get('lora_stats', {})
    server = data.get('server_stats', {})
    latency = data.get('latency', {})
    
    cursor.execute('''
        INSERT INTO gateway_stats (
            gateway_id, timestamp, uptime_seconds,
            rx_total, rx_valid, rx_invalid, rx_checksum_error, packet_loss_percent,
            tx_total, tx_success, tx_failed, server_success_rate,
            latency_avg_ms, latency_min_ms, latency_max_ms, latency_last_ms,
            energy_mah, wifi_rssi
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (
        gateway_id,
        data.get('timestamp', datetime.utcnow().isoformat()),
        data.get('uptime_seconds', 0),
        lora.get('rx_total', 0),
        lora.get('rx_valid', 0),
        lora.get('rx_invalid', 0),
        lora.get('rx_checksum_error', 0),
        lora.get('packet_loss_percent', 0),
        server.get('tx_total', 0),
        server.get('tx_success', 0),
        server.get('tx_failed', 0),
        server.get('success_rate_percent', 0),
        latency.get('avg_ms', 0),
        latency.get('min_ms', 0),
        latency.get('max_ms', 0),
        latency.get('last_ms', 0),
        data.get('energy_mah', 0),
        data.get('wifi_rssi')
    ))
    connection.commit()
    connection.close()
    
    gateway_stats_cache[gateway_id] = data


def check_and_generate_alerts(data: Dict[str, Any], connection: sqlite3.Connection) -> None:
    """Check sensor data and generate alerts if thresholds are exceeded"""
    cursor = connection.cursor()
    node_id = data.get('node_id', 'unknown')
    sensors = data.get('sensors', data)
    battery = data.get('battery_percent', 100)
    timestamp = datetime.utcnow().isoformat()
    
    if sensors.get('presence_detected'):
        cursor.execute('''
            INSERT INTO alerts (timestamp, node_id, alert_type, message)
            VALUES (?, ?, ?, ?)
        ''', (timestamp, node_id, 'presence', f'Presence detected by {node_id}'))
    
    if battery is not None and battery < 20:
        cursor.execute('''
            INSERT INTO alerts (timestamp, node_id, alert_type, message)
            VALUES (?, ?, ?, ?)
        ''', (timestamp, node_id, 'low_battery', f'Low battery ({battery}%) on {node_id}'))
    
    humidity = sensors.get('humidity_percent')
    if humidity is not None and humidity > 80:
        cursor.execute('''
            INSERT INTO alerts (timestamp, node_id, alert_type, message)
            VALUES (?, ?, ?, ?)
        ''', (timestamp, node_id, 'high_humidity', f'High humidity ({humidity:.1f}%) on {node_id}'))
    
    connection.commit()


def fetch_recent_data(limit: int = 100) -> List[Dict[str, Any]]:
    """Fetch recent sensor data from database"""
    connection = sqlite3.connect(DB_PATH)
    cursor = connection.cursor()
    cursor.execute('''
        SELECT 
            node_id, timestamp, temperature_celsius, humidity_percent, distance_cm,
            luminosity_lux, presence_detected, battery_percent, rssi_dbm, snr_db, gateway_id
        FROM sensor_data 
        ORDER BY timestamp DESC 
        LIMIT ?
    ''', (limit,))
    rows = cursor.fetchall()
    connection.close()

    data_rows: List[Dict[str, Any]] = []
    for row in rows:
        data_rows.append({
            'node_id': row[0],
            'timestamp': row[1],
            'sensors': {
                'temperature_celsius': row[2],
                'humidity_percent': row[3],
                'distance_cm': row[4],
                'luminosity_lux': row[5],
                'presence_detected': bool(row[6]),
                'rssi_dbm': row[8],
                'snr_db': row[9]
            },
            'battery_percent': row[7],
            'gateway_id': row[10]
        })
    return data_rows


def fetch_alerts(limit: int = 50, unacknowledged_only: bool = False) -> List[Dict[str, Any]]:
    """Fetch recent alerts from database"""
    connection = sqlite3.connect(DB_PATH)
    cursor = connection.cursor()
    
    query = '''
        SELECT id, timestamp, node_id, alert_type, message, acknowledged
        FROM alerts
    '''
    if unacknowledged_only:
        query += ' WHERE acknowledged = FALSE'
    query += ' ORDER BY timestamp DESC LIMIT ?'
    
    cursor.execute(query, (limit,))
    rows = cursor.fetchall()
    connection.close()
    
    return [{
        'id': row[0],
        'timestamp': row[1],
        'node_id': row[2],
        'alert_type': row[3],
        'message': row[4],
        'acknowledged': bool(row[5])
    } for row in rows]


def fetch_historical_data(hours: int = 24) -> Dict[str, Any]:
    """Fetch historical sensor data for charts"""
    connection = sqlite3.connect(DB_PATH)
    cursor = connection.cursor()
    
    since = (datetime.utcnow() - timedelta(hours=hours)).isoformat()
    
    cursor.execute('''
        SELECT timestamp, humidity_percent, distance_cm, battery_percent
        FROM sensor_data
        WHERE timestamp >= ?
        ORDER BY timestamp ASC
    ''', (since,))
    rows = cursor.fetchall()
    connection.close()
    
    return {
        'timestamps': [row[0] for row in rows],
        'humidity': [row[1] for row in rows],
        'distance': [row[2] for row in rows],
        'battery': [row[3] for row in rows]
    }


def fetch_gateway_stats_history(gateway_id: int = 1, limit: int = 100) -> List[Dict[str, Any]]:
    """Fetch gateway statistics history"""
    connection = sqlite3.connect(DB_PATH)
    cursor = connection.cursor()
    
    cursor.execute('''
        SELECT timestamp, uptime_seconds, rx_total, rx_valid, rx_invalid,
               packet_loss_percent, latency_avg_ms, energy_mah
        FROM gateway_stats
        WHERE gateway_id = ?
        ORDER BY timestamp DESC
        LIMIT ?
    ''', (gateway_id, limit))
    rows = cursor.fetchall()
    connection.close()
    
    return [{
        'timestamp': row[0],
        'uptime_seconds': row[1],
        'rx_total': row[2],
        'rx_valid': row[3],
        'rx_invalid': row[4],
        'packet_loss_percent': row[5],
        'latency_avg_ms': row[6],
        'energy_mah': row[7]
    } for row in rows]


class SensorServerHandler(http.server.SimpleHTTPRequestHandler):
    """HTTP request handler for sensor data"""
    
    def do_POST(self) -> None:
        """Handle POST requests for sensor data"""
        if self.path in ['/data', '/api/sensor-data']:
            try:
                content_length = int(self.headers.get('Content-Length', 0))
                request_body = self.rfile.read(content_length)
                
                print(f"\n[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] POST received from {self.client_address[0]}")
                
                payload = json.loads(request_body)
                
                if isinstance(payload, list):
                    print(f"  [BATCH] Received {len(payload)} messages")
                    for item in payload:
                        save_sensor_data(item)
                        print(f"    - Node: {item.get('node_id', 'unknown')}")
                    saved_count = len(payload)
                else:
                    sensors = payload.get('sensors', payload)
                    print(f"  Node: {payload.get('node_id', 'unknown')}")
                    if 'humidity_percent' in sensors:
                        print(f"  Humidity: {sensors.get('humidity_percent')}%")
                    if 'distance_cm' in sensors:
                        print(f"  Distance: {sensors.get('distance_cm')} cm")
                    if 'presence_detected' in sensors:
                        print(f"  Presence: {'✓ DETECTED' if sensors.get('presence_detected') else 'No'}")
                    save_sensor_data(payload)
                    saved_count = 1
                
                self.send_response(200, 'OK')
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                response = {'status': 'success', 'message': f'{saved_count} message(s) stored'}
                self.wfile.write(json.dumps(response).encode())
                
            except json.JSONDecodeError as e:
                print(f"Error: Invalid JSON - {e}")
                self.send_response(400, 'Bad Request')
                self.end_headers()
                self.wfile.write(b'Invalid JSON')
                
            except Exception as e:
                print(f"Error: {e}")
                import traceback
                traceback.print_exc()
                self.send_response(500, 'Internal Server Error')
                self.end_headers()
                self.wfile.write(f'Server error: {e}'.encode())
                
        elif self.path == '/api/gateway-stats':
            try:
                content_length = int(self.headers.get('Content-Length', 0))
                request_body = self.rfile.read(content_length)
                payload = json.loads(request_body)
                
                print(f"\n[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Gateway Stats received")
                print(f"  Gateway ID: {payload.get('gateway_id')}")
                print(f"  Uptime: {payload.get('uptime_seconds', 0)}s")
                
                lora = payload.get('lora_stats', {})
                print(f"  RX: {lora.get('rx_valid', 0)}/{lora.get('rx_total', 0)} valid")
                print(f"  Packet Loss: {lora.get('packet_loss_percent', 0):.1f}%")
                
                latency = payload.get('latency', {})
                print(f"  Latency: avg={latency.get('avg_ms', 0):.1f}ms, last={latency.get('last_ms', 0)}ms")
                
                save_gateway_stats(payload)
                
                self.send_response(200, 'OK')
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                response = {'status': 'success', 'message': 'Stats received'}
                self.wfile.write(json.dumps(response).encode())
                
            except Exception as e:
                print(f"Error saving gateway stats: {e}")
                self.send_response(500, 'Internal Server Error')
                self.end_headers()
                self.wfile.write(f'Server error: {e}'.encode())
                
        elif self.path == '/api/alerts/acknowledge':
            try:
                content_length = int(self.headers.get('Content-Length', 0))
                request_body = self.rfile.read(content_length)
                payload = json.loads(request_body)
                alert_id = payload.get('id')
                
                if alert_id:
                    connection = sqlite3.connect(DB_PATH)
                    cursor = connection.cursor()
                    cursor.execute('UPDATE alerts SET acknowledged = TRUE WHERE id = ?', (alert_id,))
                    connection.commit()
                    connection.close()
                
                self.send_response(200, 'OK')
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({'status': 'success'}).encode())
                
            except Exception as e:
                self.send_response(500, 'Internal Server Error')
                self.end_headers()
                self.wfile.write(f'Server error: {e}'.encode())
        else:
            self.send_response(404, 'Not Found')
            self.end_headers()

    def do_GET(self) -> None:
        """Handle GET requests"""
        print(f"[{datetime.now().strftime('%H:%M:%S')}] GET {self.path} from {self.client_address[0]}")
        
        if self.path == '/':
            self.path = '/dashboard.html'
            return http.server.SimpleHTTPRequestHandler.do_GET(self)
        
        elif self.path == '/data':
            try:
                recent_data = fetch_recent_data()
                
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps(recent_data).encode())
            except Exception as e:
                print(f"Error: {e}")
                self.send_response(500, 'Internal Server Error')
                self.end_headers()
                self.wfile.write(f'Server error: {e}'.encode())
        
        elif self.path == '/api/alerts':
            try:
                alerts = fetch_alerts(50, False)
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps(alerts).encode())
            except Exception as e:
                self.send_response(500, 'Internal Server Error')
                self.end_headers()
                self.wfile.write(f'Server error: {e}'.encode())
        
        elif self.path == '/api/alerts/unacknowledged':
            try:
                alerts = fetch_alerts(50, True)
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps(alerts).encode())
            except Exception as e:
                self.send_response(500, 'Internal Server Error')
                self.end_headers()
                self.wfile.write(f'Server error: {e}'.encode())
        
        elif self.path.startswith('/api/history'):
            try:
                hours = 24
                if '?' in self.path:
                    params = self.path.split('?')[1]
                    for param in params.split('&'):
                        if param.startswith('hours='):
                            hours = int(param.split('=')[1])
                
                history = fetch_historical_data(hours)
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps(history).encode())
            except Exception as e:
                self.send_response(500, 'Internal Server Error')
                self.end_headers()
                self.wfile.write(f'Server error: {e}'.encode())
        
        elif self.path == '/api/gateway-stats':
            try:
                stats_list = list(gateway_stats_cache.values())
                if not stats_list:
                    stats_list = fetch_gateway_stats_history(1, 1)
                
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps(stats_list).encode())
            except Exception as e:
                self.send_response(500, 'Internal Server Error')
                self.end_headers()
                self.wfile.write(f'Server error: {e}'.encode())
        
        elif self.path == '/api/gateway-stats/history':
            try:
                history = fetch_gateway_stats_history(1, 100)
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps(history).encode())
            except Exception as e:
                self.send_response(500, 'Internal Server Error')
                self.end_headers()
                self.wfile.write(f'Server error: {e}'.encode())
        
        elif self.path == '/health':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({'status': 'ok'}).encode())
        
        elif self.path == '/api/sensor-data':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            info = b"""
            <html>
            <head><title>API Endpoint</title></head>
            <body>
                <h1>Sensor Data API</h1>
                <p>This endpoint accepts POST requests with sensor data.</p>
                <p>Send data in JSON format to: <code>POST /api/sensor-data</code></p>
                <p><a href="/data">View recent data (GET /data)</a></p>
            </body>
            </html>
            """
            self.wfile.write(info)
        
        else:
            super().do_GET()
    
    def log_message(self, format, *args):
        """Override to customize logging"""
        # Only log errors, not every request
        if args[1][0] in ['4', '5']:  # 4xx and 5xx errors
            super().log_message(format, *args)


def start_server() -> None:
    """Start the HTTP server"""
    global server_start_time
    server_start_time = datetime.utcnow()  # Registrar tempo de início do servidor
    
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
