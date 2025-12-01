"""
Alert management and generation functions
"""

import sqlite3
from datetime import datetime
from typing import Any, Dict, List
from database import DB_PATH


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


def acknowledge_alert(alert_id: int) -> None:
    """Mark an alert as acknowledged"""
    connection = sqlite3.connect(DB_PATH)
    cursor = connection.cursor()
    cursor.execute('UPDATE alerts SET acknowledged = TRUE WHERE id = ?', (alert_id,))
    connection.commit()
    connection.close()
