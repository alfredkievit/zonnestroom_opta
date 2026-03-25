#!/usr/bin/env node
/**
 * Debug: Check what meter topics are being seen
 */

const mqtt = require('mqtt');

const BROKER_IP = '192.168.0.10';
const BROKER_PORT = 1883;
const BROKER_URL = `mqtt://${BROKER_IP}:${BROKER_PORT}`;

const METER_TOPICS = [
  'b0b21c913c34/PUB/CH1',
  'b0b21c913c34/PUB/CH10',
  'b0b21c913c34/PUB/CH13',
  'b0b21c913c34/PUB/CH14',
];

let client = null;
let received = {};

async function main() {
  return new Promise((resolve) => {
    client = mqtt.connect(BROKER_URL, { clientId: 'meter_debug_' + Date.now() });
    
    client.on('connect', () => {
      console.log('Connected to MQTT. Subscribing to meter topics:\n');
      METER_TOPICS.forEach(topic => {
        console.log(`  → ${topic}`);
        client.subscribe(topic, { qos: 1 });
      });
      console.log('\nWaiting for meter updates (5 seconds)...\n');
      
      setTimeout(() => {
        console.log('\nMeter messages received:');
        if (Object.keys(received).length === 0) {
          console.log('  ✗ NO METER MESSAGES RECEIVED!');
        } else {
          Object.entries(received).forEach(([topic, data]) => {
            console.log(`  ✓ ${topic}`);
            console.log(`     → ${data.msg} (${data.count} messages)`);
          });
        }
        client.end();
        resolve();
      }, 5000);
    });
    
    client.on('message', (topic, msg) => {
      const payload = msg.toString();
      if (!received[topic]) {
        received[topic] = { msg: payload, count: 1 };
        console.log(`\n✓ Received from ${topic}:`);
        console.log(`  ${payload}`);
      } else {
        received[topic].count++;
      }
    });
    
    client.on('error', (err) => {
      console.error('✗ Error:', err.message);
      process.exit(1);
    });
  });
}

main();
