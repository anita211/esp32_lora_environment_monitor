"""
HTTP request handler for sensor data
"""

import http.server
import json
from datetime import datetime
from typing import Dict, Any

from database import save_sensor_data, save_gateway_stats, fetch_recent_data, fetch_historical_data, fetch_gateway_stats_history
from alerts import fetch_alerts, acknowledge_alert


class SensorServerHandler(http.server.SimpleHTTPRequestHandler):
    """HTTP request handler for sensor data"""
    
    # Class variable to store gateway stats cache
    gateway_stats_cache: Dict[int, Dict[str, Any]] = {}
    
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
                
                save_gateway_stats(payload, SensorServerHandler.gateway_stats_cache)
                
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
                    acknowledge_alert(alert_id)
                
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
                stats_list = list(SensorServerHandler.gateway_stats_cache.values())
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
