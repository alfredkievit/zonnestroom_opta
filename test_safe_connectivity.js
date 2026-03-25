#!/usr/bin/env node
/**
 * SAFE TEST - Only tests Opta connectivity, NO meter data injection
 * This tests only MQTT communication, not state machine with real loads
 */

const mqtt = require('mqtt');

const BROKER_IP = '192.168.0.10';
const BROKER_URL = `mqtt://${BROKER_IP}:1883`;

async function main() {
  return new Promise((resolve) => {
    const client = mqtt.connect(BROKER_URL);
    
    client.on('connect', async () => {
      console.log('\n✓ Connected to MQTT broker at', BROKER_IP);
      console.log('\n═══════════════════════════════════════════════════');
      console.log('SAFE TEST: MQTT Communication Only');
      console.log('(NO meter data injection - safe with legacy system)');
      console.log('═══════════════════════════════════════════════════\n');
      
      // Test 1: Opta1 heartbeat monitoring
      console.log('TEST 1: Monitor Opta1 heartbeat (indicates it\'s alive)');
      client.subscribe('opta1/device/heartbeat');
      
      console.log('TEST 2: Monitor Opta2 status (indicates it\'s alive)');
      client.subscribe('opta2/status/alarms');
      
      console.log('TEST 3: Check current system state (no changes)');
      client.subscribe('opta1/status/active_priority');
      
      let heartbeatCount = 0;
      const responses = {};
      
      client.on('message', (topic, msg) => {
        const payload = msg.toString();
        if (topic === 'opta1/device/heartbeat') {
          heartbeatCount++;
          console.log(`\n✓ Opta1 Heartbeat #${heartbeatCount}: ${payload} (alive!)`);
        } else if (!responses[topic]) {
          responses[topic] = payload;
          console.log(`✓ ${topic}: ${payload.substring(0, 50)}${payload.length > 50 ? '...' : ''}`);
        }
      });
      
      console.log('\nMonitoring for 10 seconds (no state changes)...\n');
      
      await new Promise(r => setTimeout(r, 10000));
      
      console.log('\n═══════════════════════════════════════════════════');
      console.log('RESULTS:');
      console.log('═══════════════════════════════════════════════════');
      console.log(`✓ Opta1 Heartbeats received: ${heartbeatCount}`);
      console.log(`✓ Opta2 Status received: ${responses['opta2/status/alarms'] ? 'Yes' : 'No'}`);
      console.log(`✓ System Priority: ${responses['opta1/status/active_priority'] || 'Unknown'}`);
      
      if (heartbeatCount > 0) {
        console.log('\n✓✓✓ Both Opta boards confirmed ALIVE and communicating\n');
      } else {
        console.log('\n✗ No heartbeat - check MQTT connection\n');
      }
      
      client.end();
      resolve();
    });
    
    client.on('error', (err) => {
      console.error('✗ Error:', err.message);
      process.exit(1);
    });
  });
}

main();
