"""
Database operations for sensor data and gateway statistics
"""

import sqlite3
from pathlib import Path
from datetime import datetime, timedelta
from typing import Any, Dict, List

BASE_DIR = Path(__file__).resolve().parent
DB_NAME = 'environmental_data.db'
DB_PATH = BASE_DIR / DB_NAME


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
    from alerts import check_and_generate_alerts
    
    connection = sqlite3.connect(DB_PATH)
    cursor = connection.cursor()
    
    node_id = data.get('node_id', 'unknown')
    timestamp = data.get('timestamp', datetime.utcnow().isoformat())
    
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


def save_gateway_stats(data: Dict[str, Any], gateway_stats_cache: Dict[int, Dict[str, Any]]) -> None:
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
