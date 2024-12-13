from flask import Flask, request, jsonify
import time
from datetime import datetime
import pytz

app = Flask(__name__)

# Stores rfid scans + add some sample data for the past few days
rfid_scans = [
    {"pet_id": "12345", "date": "2024-12-06", "time": "08:15:00 AM", "timestamp": 1733716500.0},
    {"pet_id": "67890", "date": "2024-12-06", "time": "10:30:00 AM", "timestamp": 1733723400.0},
    {"pet_id": "12345", "date": "2024-12-06", "time": "07:45:00 PM", "timestamp": 1733761500.0},
    {"pet_id": "67890", "date": "2024-12-07", "time": "09:00:00 AM", "timestamp": 1733792400.0},
    {"pet_id": "67890", "date": "2024-12-07", "time": "06:30:00 PM", "timestamp": 1733826600.0},
    {"pet_id": "11223", "date": "2024-12-07", "time": "07:45:00 PM", "timestamp": 1733831100.0},
    {"pet_id": "12345", "date": "2024-12-08", "time": "08:20:00 AM", "timestamp": 1733883600.0},
    {"pet_id": "11223", "date": "2024-12-08", "time": "11:00:00 AM", "timestamp": 1733892000.0},
    {"pet_id": "67890", "date": "2024-12-08", "time": "06:45:00 PM", "timestamp": 1733923500.0},
    {"pet_id": "12345", "date": "2024-12-09", "time": "08:30:00 AM", "timestamp": 1733961000.0},
    {"pet_id": "67890", "date": "2024-12-09", "time": "05:45:00 PM", "timestamp": 1733995500.0},
]

# Stores feeding events + add some sample data for the past few days 
feeding_events = [
    {"date": "2024-12-06", "time": "08:30:00 AM", "timestamp": 1733717400.0},
    {"date": "2024-12-06", "time": "07:50:00 PM", "timestamp": 1733761800.0},
    {"date": "2024-12-07", "time": "09:15:00 AM", "timestamp": 1733793300.0},
    {"date": "2024-12-07", "time": "06:45:00 PM", "timestamp": 1733827500.0},
    {"date": "2024-12-08", "time": "08:40:00 AM", "timestamp": 1733884800.0},
    {"date": "2024-12-08", "time": "06:50:00 PM", "timestamp": 1733923800.0},
    {"date": "2024-12-09", "time": "08:45:00 AM", "timestamp": 1733961900.0},
    {"date": "2024-12-09", "time": "05:50:00 PM", "timestamp": 1733995800.0},
]

@app.route("/")
def index():
    return app.send_static_file("index.html")

# Logs a feeding event 
@app.route("/feeding")
def log_feeding():
    current_time = time.time()
    time_zone = pytz.timezone("America/Los_Angeles")
    local_time = datetime.fromtimestamp(current_time, time_zone)

    feeding_event = {
        "date": local_time.strftime("%Y-%m-%d"),
        "time": local_time.strftime("%I:%M:%S %p"),
        "timestamp": current_time
    }
    feeding_events.append(feeding_event)
    return jsonify({"message": "Feeding event logged", "feeding_event": feeding_event})

# Logs an RFID scan
@app.route("/rfid_scan")
def log_rfid_scan():
    pet_id = request.args.get("pet_id")
    if not pet_id:
        return jsonify({"error": "Missing 'pet_id' parameter"}), 400

    current_time = time.time()
    time_zone = pytz.timezone("America/Los_Angeles")
    local_time = datetime.fromtimestamp(current_time, time_zone)

    scan_event = {
        "pet_id": pet_id,
        "date": local_time.strftime("%Y-%m-%d"),
        "time": local_time.strftime("%I:%M:%S %p"),
        "timestamp": current_time
    }
    rfid_scans.append(scan_event)
    return jsonify({"message": "RFID scan logged", "scan_event": scan_event})

# Fetch all RFID scan data
@app.route("/rfid_data")
def get_rfid_data():
    return jsonify(rfid_scans)

# Fetch all feeding events
@app.route("/feeding_data")
def get_feeding_data():
    return jsonify(feeding_events)

