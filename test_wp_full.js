#!/usr/bin/env node
/**
 * Complete WP Activation Test
 * Ensures enableSystem is set and tests WP with various temperatures
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

const CMD_TOPICS = {
  ENABLE_SYSTEM: 'opta1/cmd/enable_system',
  ENABLE_WP: 'opta1/cmd/enable_wp_boiler',
};

const STATUS_TOPICS = {
  surplus_f1: 'opta1/status/surplus_fase1_w',
  priority: 'opta1/status/active_priority',
  wp_active: 'opta1/status/wp_active',
  mqtt_valid: 'opta1/status/mqtt_valid',
  boiler_low: 'opta1/status/boiler_temp_low',
};

let client = null;
let status = {};

function sleep(ms) {
  return new Promise(r => setTimeout(r, ms));
}

function injectPower(ch1, ch10, ch13, ch14) {
  const payload = (v) => JSON.stringify({ P: v });
  client.publish(METER_TOPICS.CH1, payload(ch1));
  client.publish(METER_TOPICS.CH10, payload(ch10));
  client.publish(METER_TOPICS.CH13, payload(ch13));
  client.publish(METER_TOPICS.CH14, payload(ch14));
}

function setCommand(cmd, value) {
  console.log(`  Publishing: ${cmd} = ${value}`);
  client.publish(cmd, value.toString());
}

function showStatus(label) {
  console.log(`\n  [${label}] Status:`);
  console.log(`    - MQTT Valid:    ${status[STATUS_TOPICS.mqtt_valid]}`);
  console.log(`    - Surplus F1:    ${status[STATUS_TOPICS.surplus_f1]}W`);
  console.log(`    - Priority:      ${status[STATUS_TOPICS.priority]} (0=IDLE, 1=WP)`);
  console.log(`    - WP Active:     ${status[STATUS_TOPICS.wp_active]}`);
  console.log(`    - Boiler Temp:   ${status[STATUS_TOPICS.boiler_low]}°C`);
}

async function main() {
  return new Promise(async (resolve) => {
    client = mqtt.connect(BROKER_URL, { clientId: 'wp_test_' + Date.now() });
    
    client.on('connect', async () => {
      console.log('✓ Connected\n');
      
      // Subscribe to status
      Object.values(STATUS_TOPICS).forEach(t => client.subscribe(t));
      
      await sleep(1000);
      
      console.log('═══════════════════════════════════════════════════');
      console.log('PHASE 1: Enable System & WP');
      console.log('═══════════════════════════════════════════════════');
      setCommand(CMD_TOPICS.ENABLE_SYSTEM, 1);
      setCommand(CMD_TOPICS.ENABLE_WP, 1);
      
      await sleep(2000);
      showStatus('After Enable');
      
      console.log('\n───────────────────────────────────────────────────');
      console.log('PHASE 2: Inject High Surplus (4000W F1)');
      console.log('───────────────────────────────────────────────────');
      injectPower(4000, 100, 5000, 100);
      console.log('  Injected: CH1=4000, CH10=100, CH13=5000, CH14=100');
      
      await sleep(3000);
      showStatus('After Power Injection');
      
      if (status[STATUS_TOPICS.wp_active] === '0') {
        console.log('\n⚠️  WP still not active!');
        console.log('  Possible causes:');
        console.log('    1. enableSystem/enableWp command not received');
        console.log('    2. State machine has additional conditions');
        console.log('    3. Boiler temp fault (shows 0°C constantly)');
        
        console.log('\n  Debugging: publish a test with exact meter payload format:');
      }
      
      console.log('\n───────────────────────────────────────────────────');
      console.log('PHASE 3: Check Raw Meter Values on Broker');
      console.log('───────────────────────────────────────────────────');
      
      // Subscribe raw to meter topics to see what Opta1 sees
      client.subscribe(METER_TOPICS.CH1, { qos: 1 });
      
      await sleep(2000);
      
      console.log('\n✓ Test complete\n');
      client.end();
      resolve();
    });
    
    client.on('message', (topic, msg) => {
      const payload = msg.toString();
      if (STATUS_TOPICS[Object.keys(STATUS_TOPICS).find(k => STATUS_TOPICS[k] === topic)]) {
        status[topic] = payload;
      }
    });
    
    client.on('error', (err) => {
      console.error('✗ Error:', err.message);
      process.exit(1);
    });
  });
}

main();
