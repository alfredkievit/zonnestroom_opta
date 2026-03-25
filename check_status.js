#!/usr/bin/env node
/**
 * Opta1 State Machine Diagnostics (simplified)
 * Checks boiler temperatures and state immediately
 */

const mqtt = require('mqtt');

const BROKER_IP = '192.168.0.10';
const BROKER_PORT = 1883;
const BROKER_URL = `mqtt://${BROKER_IP}:${BROKER_PORT}`;

const TOPICS = {
  boiler_temp_low: 'opta1/status/boiler_temp_low',
  boiler_temp_high: 'opta1/status/boiler_temp_high',
  surplus_f1: 'opta1/status/surplus_fase1_w',
  priority: 'opta1/status/active_priority',
  wp_active: 'opta1/status/wp_active',
  mqtt_valid: 'opta1/status/mqtt_valid',
};

let status = {};
let messagesReceived = 0;

function showStatus() {
  console.log('\n╔════════════════════════════════════════════╗');
  console.log('║        OPTA1 STATE MACHINE STATUS         ║');
  console.log('╚════════════════════════════════════════════╝\n');
  
  const boilerLow = parseFloat(status[TOPICS.boiler_temp_low] || 'N/A');
  const boilerHigh = parseFloat(status[TOPICS.boiler_temp_high] || 'N/A');
  const surplusF1 = status[TOPICS.surplus_f1] || 'N/A';
  const priority = ['IDLE', 'WP_BOILER', 'ELEMENT', 'HOTTUB', '?'][parseInt(status[TOPICS.priority] || 0)] || 'UNKNOWN';
  const wpActive = status[TOPICS.wp_active] === '1' ? '✓ ON' : '✗ OFF';
  const mqttValid = status[TOPICS.mqtt_valid] === '1' ? '✓ VALID' : '✗ INVALID';
  
  console.log('Temperature:');
  console.log(`  Boiler Low:   ${boilerLow}°C`);
  console.log(`  Boiler High:  ${boilerHigh}°C\n`);
  
  console.log('Energy:');
  console.log(`  Fase1 Surplus: ${surplusF1}W`);
  console.log(`  MQTT:          ${mqttValid}\n`);
  
  console.log('State:');
  console.log(`  Priority:      ${priority}`);
  console.log(`  WP Active:     ${wpActive}\n`);
  
  // Diagnosis
  if (typeof boilerLow === 'number') {
    if (boilerLow < 30) {
      console.log('✓ Boiler temperature OK for WP start');
    } else {
      console.log(`⚠  Boiler temp ${boilerLow}°C - may inhibit WP (threshold usually ~35-40°C)`);
    }
  }
  
  if (surplusF1 !== 'N/A') {
    const surplus = parseInt(surplusF1);
    if (surplus < 1000) {
      console.log(`⚠  Fase1 surplus ${surplus}W - may be below WP start threshold (~1400W)`);
    }
  }
}

async function main() {
  return new Promise((resolve) => {
    const client = mqtt.connect(BROKER_URL, { clientId: 'diagnostic_' + Date.now() });
    
    client.on('connect', () => {
      console.log('Connecting to MQTT and fetching status...');
      Object.values(TOPICS).forEach(topic => client.subscribe(topic));
      
      // Wait 3 seconds for messages, then display
      setTimeout(() => {
        showStatus();
        client.end();
        resolve();
      }, 3000);
    });
    
    client.on('message', (topic, msg) => {
      status[topic] = msg.toString();
      messagesReceived++;
    });
    
    client.on('error', (err) => {
      console.error('✗ MQTT Error:', err.message);
      process.exit(1);
    });
  });
}

main();
