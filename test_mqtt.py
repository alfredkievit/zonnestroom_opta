#!/usr/bin/env python3
"""
MQTT Connectivity & State Machine Test Script
Tests Opta1 (energy master) and Opta2 (hottub controller)
Simulates energy meter values and monitors system responses
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
from typing import Dict, List
from datetime import datetime

# ─── Configuration ─────────────────────────────────────────────────────────
BROKER_IP = "192.168.0.10"
BROKER_PORT = 1883

# Energy meter topics (Opta's listen to these)
TOPICS_METER = {
    "CH1": "b0b21c913c34/PUB/CH1",      # fase 1 export
    "CH10": "b0b21c913c34/PUB/CH10",    # fase 1 import
    "CH13": "b0b21c913c34/PUB/CH13",    # totaal export
    "CH14": "b0b21c913c34/PUB/CH14",    # totaal import
}

# Opta1 status topics (we subscribe to these)
TOPICS_STATUS = {
    "surplus_f1": "opta1/status/surplus_fase1_w",
    "surplus_totaal": "opta1/status/surplus_totaal_w",
    "priority": "opta1/status/active_priority",
    "mqtt_valid": "opta1/status/mqtt_valid",
    "mqtt_lastseen": "opta1/status/mqtt_last_seen",
    "wp_active": "opta1/status/wp_active",
    "element_active": "opta1/status/element_active",
    "hottub_perm": "opta1/status/hottub_permission",
    "alarms": "opta1/status/alarms",
    "heartbeat": "opta1/device/heartbeat",
}

# Opta2 status topics
TOPICS_OPTA2 = {
    "hottub_status": "opta2/status/hottub_status",
    "alarms": "opta2/status/alarms",
}

# ─── Test Scenarios ────────────────────────────────────────────────────────
SCENARIOS = {
    "idle": {
        "description": "Geen surplus - alle lasten uit",
        "CH1": 100.0,
        "CH10": 100.0,    # fase1 surplus = 0W
        "CH13": 150.0,
        "CH14": 150.0,    # totaal surplus = 0W
        "expected_state": "IDLE",
        "duration": 5,
    },
    "wp_active": {
        "description": "WP zou actief moeten zijn (fase1 overschot, temp laag)",
        "CH1": 1500.0,
        "CH10": 100.0,    # fase1 surplus = 1400W
        "CH13": 2000.0,
        "CH14": 100.0,    # totaal surplus = 1900W
        "expected_state": "WP_BOILER",
        "duration": 5,
    },
    "drop_to_idle": {
        "description": "Surplus weg - terug naar IDLE",
        "CH1": 100.0,
        "CH10": 100.0,    # fase1 surplus = 0W
        "CH13": 150.0,
        "CH14": 150.0,    # totaal surplus = 0W
        "expected_state": "IDLE",
        "duration": 5,
    },
}

# ─── Global State ──────────────────────────────────────────────────────────
received_data = {}
test_active = True
client = None


# ─── MQTT Callbacks ────────────────────────────────────────────────────────
def on_connect(client, userdata, flags, rc):
    """Called when connected to broker"""
    if rc == 0:
        print(f"\n✓ MQTT Connected (code {rc})\n")
        
        # Subscribe to all status topics
        for topic in TOPICS_STATUS.values():
            client.subscribe(topic)
            print(f"  → Subscribed to: {topic}")
        
        for topic in TOPICS_OPTA2.values():
            client.subscribe(topic)
            print(f"  → Subscribed to: {topic}")
            
    else:
        print(f"✗ Connection failed with code {rc}")
        sys.exit(1)


def on_message(client, userdata, msg):
    """Called when a message is received"""
    global received_data
    
    # Store received value
    topic = msg.topic
    payload = msg.payload.decode('utf-8', errors='ignore')
    
    received_data[topic] = {
        'value': payload,
        'timestamp': datetime.now(),
    }
    
    # Pretty print incoming data
    print(f"\n📨 RX: {topic}")
    print(f"   → Value: {payload}")


def on_disconnect(client, userdata, rc):
    """Called when disconnected"""
    if rc != 0:
        print(f"\n✗ Unexpected MQTT disconnection: code {rc}")
    else:
        print(f"\n✓ Disconnected from MQTT broker")


# ─── Test Functions ────────────────────────────────────────────────────────
def publish_meter_values(ch1: float, ch10: float, ch13: float, ch14: float):
    """Publish simulated energy meter values"""
    values = {
        "CH1": ch1,
        "CH10": ch10,
        "CH13": ch13,
        "CH14": ch14,
    }
    
    print(f"\n📤 Publishing meter values:")
    for channel, value in values.items():
        topic = TOPICS_METER[channel]
        payload = f'{{"P":{int(value)}}}'
        client.publish(topic, payload, qos=1, retain=False)
        print(f"   {channel:5s} → {int(value):6d}W ({topic})")
    
    # Publish timestamp so Opta knows when last meter update was
    client.publish(f"{TOPICS_METER['CH1'].rsplit('/', 1)[0]}/timestamp", 
                   str(int(time.time())), qos=1, retain=False)


def run_test_scenario(scenario_name: str, scenario: Dict):
    """Execute a test scenario"""
    global received_data
    
    print(f"\n{'='*70}")
    print(f"SCENARIO: {scenario_name.upper()}")
    print(f"{'='*70}")
    print(f"Description: {scenario['description']}")
    print(f"Expected state: {scenario['expected_state']}")
    print(f"Duration: {scenario['duration']}s\n")
    
    # Publish meter values
    publish_meter_values(
        scenario["CH1"],
        scenario["CH10"],
        scenario["CH13"],
        scenario["CH14"]
    )
    
    # Wait for responses
    remaining = scenario['duration']
    while remaining > 0 and test_active:
        time.sleep(1)
        remaining -= 1
        print(f"   ... waiting ({remaining}s remaining)")
    
    # Display collected responses
    print(f"\nResponses collected during scenario:")
    if not received_data:
        print("   (no responses yet - check MQTT broker connectivity)")
    else:
        for topic, data in sorted(received_data.items()):
            print(f"   {topic}")
            print(f"      → {data['value']}")
    
    received_data.clear()


def run_connectivity_test():
    """Test basic MQTT connectivity"""
    print(f"\n{'='*70}")
    print("CONNECTIVITY TEST")
    print(f"{'='*70}")
    print(f"Broker: {BROKER_IP}:{BROKER_PORT}\n")
    
    # This will be tested in on_connect callback
    print("Waiting for connection...")


def print_summary():
    """Print commissioning summary"""
    print(f"\n{'='*70}")
    print("COMMISSIONING CHECKLIST")
    print(f"{'='*70}\n")
    
    checks = [
        ("MQTT Broker connectivity", "Check connection message above"),
        ("Opta1 receives meter values", "CH1, CH10, CH13, CH14"),
        ("Opta1 calculates surplus", "surplus_fase1_w, surplus_totaal_w"),
        ("Opta1 state machine works", "priority state transitions"),
        ("Opta1 publishes status", "wp_active, element_active, etc."),
        ("Opta2 heartbeat detected", "opta1/device/heartbeat toggling"),
        ("Opta2 hottub control works", "opta2/status/hottub_status"),
        ("Fail-safe interlocks active", "Check alarms when expected"),
    ]
    
    for name, description in checks:
        print(f"  ☐ {name}")
        print(f"      {description}\n")


# ─── Main ──────────────────────────────────────────────────────────────────
def main():
    global client, test_active
    
    try:
        # Connect to MQTT broker
        client = mqtt.Client(client_id="zonnestroom_test_client")
        client.on_connect = on_connect
        client.on_message = on_message
        client.on_disconnect = on_disconnect
        
        print(f"\n{'='*70}")
        print("ZONNESTROOM DUAL-OPTA MQTT TEST")
        print(f"{'='*70}")
        print(f"\nConnecting to MQTT broker at {BROKER_IP}:{BROKER_PORT}...")
        
        client.connect(BROKER_IP, BROKER_PORT, keepalive=60)
        client.loop_start()
        
        # Wait for connection
        time.sleep(2)
        
        if not client.is_connected():
            print("✗ Failed to connect to MQTT broker")
            print("   Ensure:")
            print("   1. Broker is running at 192.168.0.10:1883")
            print("   2. Both Opta boards are connected via Ethernet")
            print("   3. Network connectivity is configured")
            sys.exit(1)
        
        # Run connectivity test first
        run_connectivity_test()
        time.sleep(2)
        
        # Run test scenarios
        for scenario_name, scenario in SCENARIOS.items():
            run_test_scenario(scenario_name, scenario)
            time.sleep(1)
        
        # Print summary
        print_summary()
        
        # Keep running to display real-time updates
        print("\n📊 Monitoring MQTT for 30 seconds (Ctrl+C to exit)...\n")
        time.sleep(30)
        
    except KeyboardInterrupt:
        print("\n\n✓ Test completed by user")
    except Exception as e:
        print(f"\n✗ Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        test_active = False
        if client:
            client.loop_stop()
            client.disconnect()


if __name__ == "__main__":
    main()
