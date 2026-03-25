#!/usr/bin/env node
/**
 * Power Inject Test
 * Inject strong surplus and monitor WP activation
 */

const mqtt = require('mqtt');

const BROKER_IP = '192.168.0.10';
const BROKER_PORT = 1883;
const BROKER_URL = `mqtt://${BROKER_IP}:${BROKER_PORT}`;

const METER_TOPICS = {
  CH1: 'b0b21c913c34/PUB/CH1',
  CH10: 'b0b21c913c34/PUB/CH10',
  CH13: 'b0b21c913c34/PUB/CH13',
  CH14: 'b0b21c913c34/PUB/CH14',
};

const STATUS_TOPICS = {
  surplus_f1: 'opta1/status/surplus_fase1_w',
  surplus_totaal: 'opta1/status/surplus_totaal_w',
  priority: 'opta1/status/active_priority',
  wp_active: 'opta1/status/wp_active',
  mqtt_valid: 'opta1/status/mqtt_valid',
  boiler_low: 'opta1/status/boiler_temp_low',
};

let client = null;
let status = {};

function injectPower(ch1, ch10, ch13, ch14) {
  console.log(`\n📤 Injecting power: CH1=${ch1}, CH10=${ch10}, CH13=${ch13}, CH14=${ch14}`);
  
  const payload = (value) => JSON.stringify({ P: value });
  
  client.publish(METER_TOPICS.CH1, payload(ch1));
  client.publish(METER_TOPICS.CH10, payload(ch10));
  client.publish(METER_TOPICS.CH13, payload(ch13));
  client.publish(METER_TOPICS.CH14, payload(ch14));
}

function showStatus() {
  console.log('\n  Current status:');
  console.log(`  - MQTT Valid:    ${status[STATUS_TOPICS.mqtt_valid]}`);
  console.log(`  - Surplus F1:    ${status[STATUS_TOPICS.surplus_f1]}W`);
  console.log(`  - Surplus Total: ${status[STATUS_TOPICS.surplus_totaal]}W`);
  console.log(`  - Priority:      ${status[STATUS_TOPICS.priority]} (0=IDLE, 1=WP, 2=ELEMENT, 3=HOTTUB)`);
  console.log(`  - WP Active:     ${status[STATUS_TOPICS.wp_active]}`);
  console.log(`  - Boiler Temp:   ${status[STATUS_TOPICS.boiler_low]}°C`);
}

async function main() {
  return new Promise((resolve) => {
    client = mqtt.connect(BROKER_URL, { clientId: 'power_inject_' + Date.now() });
    
    client.on('connect', async () => {
      console.log('✓ Connected to MQTT\n');
      
      // Subscribe to status
      Object.values(STATUS_TOPICS).forEach(t => client.subscribe(t));
      
      // Wait for initial status
      await new Promise(r => setTimeout(r, 2000));
      
      console.log('\n═══════════════════════════════════════════════════');
      console.log('TEST 1: Strong Phase-1 Surplus (4000W export, 100W import)');
      console.log('═══════════════════════════════════════════════════');
      injectPower(4000, 100, 5000, 100);
      
      await new Promise(r => setTimeout(r, 5000));
      showStatus();
      
      console.log('\n═══════════════════════════════════════════════════');
      console.log('TEST 2: Extreme Surplus (8000W export, 0W import)');
      console.log('═══════════════════════════════════════════════════');
      injectPower(8000, 0, 10000, 0);
      
      await new Promise(r => setTimeout(r, 5000));
      showStatus();
      
      console.log('\n═══════════════════════════════════════════════════');
      console.log('TEST 3: Back to No Surplus');
      console.log('═══════════════════════════════════════════════════');
      injectPower(0, 1500, 0, 2000);
      
      await new Promise(r => setTimeout(r, 5000));
      showStatus();
      
      console.log('\n\n✓ Test completed\n');
      
      client.end();
      resolve();
    });
    
    client.on('message', (topic, msg) => {
      status[topic] = msg.toString();
    });
    
    client.on('error', (err) => {
      console.error('✗ Error:', err.message);
      process.exit(1);
    });
  });
}

main();
