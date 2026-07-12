#!/usr/bin/env node
/**
 * Opta1 State Machine Diagnostics
 * Checks boiler temperatures, state transitions, and WP activation
 */

const mqtt = require('mqtt');

const BROKER_IP = '192.168.0.10';
const BROKER_PORT = 1883;
const BROKER_URL = `mqtt://${BROKER_IP}:${BROKER_PORT}`;

// All Opta1 status topics (plus temperatures)
const TOPICS = {
  boiler_temp_low: 'opta1/status/boiler_temp_low',
  boiler_temp_high: 'opta1/status/boiler_temp_high',
  surplus_f1: 'opta1/status/surplus_fase1_w',
  surplus_totaal: 'opta1/status/surplus_totaal_w',
  priority: 'opta1/status/active_priority',
  wp_active: 'opta1/status/wp_active',
  element_active: 'opta1/status/element_active',
  hottub_perm: 'opta1/status/hottub_permission',
  mqtt_valid: 'opta1/status/mqtt_valid',
  alarms: 'opta1/status/alarms',
};

let client = null;
let status = {};

async function connect() {
  return new Promise((resolve, reject) => {
    client = mqtt.connect(BROKER_URL, { clientId: 'diagnostic_' + Date.now() });
    
    client.on('connect', () => {
      console.log('✓ Connected to MQTT\n');
      Object.values(TOPICS).forEach(topic => client.subscribe(topic));
      resolve();
    });
    
    client.on('message', (topic, msg) => {
      status[topic] = msg.toString();
    });
    
    client.on('error', reject);
    setTimeout(() => reject(new Error('Connection timeout')), 5000);
  });
}

function displayStatus() {
  console.clear();
  console.log('╔════════════════════════════════════════════╗');
  console.log('║        OPTA1 STATE MACHINE DIAGNOSTICS     ║');
  console.log('╚════════════════════════════════════════════╝\n');
  
  const boilerLow = parseFloat(status[TOPICS.boiler_temp_low] || 0);
  const boilerHigh = parseFloat(status[TOPICS.boiler_temp_high] || 0);
  const surplusF1 = parseInt(status[TOPICS.surplus_f1] || 0);
  const surplassTotal = parseInt(status[TOPICS.surplus_totaal] || 0);
  const priority = ['IDLE', 'WP_BOILER', 'ELEMENT', 'HOTTUB', '?', '?', '?', '?', '?', '?', 'FAULT'][parseInt(status[TOPICS.priority] || 0)] || 'UNKNOWN';
  const wpActive = status[TOPICS.wp_active] === '1' ? '🟢 ON' : '🔴 OFF';
  const elemActive = status[TOPICS.element_active] === '1' ? '🟢 ON' : '🔴 OFF';
  const hottubPerm = status[TOPICS.hottub_perm] === '1' ? '🟢 ALLOWED' : '🔴 BLOCKED';
  
  console.log('📊 TEMPERATURES:');
  console.log(`   Boiler Low:     ${boilerLow.toFixed(1)}°C`);
  console.log(`   Boiler High:    ${boilerHigh.toFixed(1)}°C\n`);
  
  console.log('⚡ SURPLUS:');
  console.log(`   Fase 1:         ${surplusF1}W`);
  console.log(`   Totaal:         ${surplassTotal}W\n`);
  
  console.log('🎛️  STATE MACHINE:');
  console.log(`   Priority:       ${priority}`);
  console.log(`   WP Status:      ${wpActive}`);
  console.log(`   Element:        ${elemActive}`);
  console.log(`   Hottub Perm:    ${hottubPerm}\n`);
  
  // Analysis
  console.log('🔍 ANALYSIS:');
  if (surplusF1 < 500) {
    console.log('   ⓘ Low fase-1 surplus (<500W) - WP may be inhibited');
  }
  if (boilerLow > 50) {
    console.log(`   ℹ️  Boiler temp HIGH (${boilerLow.toFixed(1)}°C) - may prevent WP start`);
  } else if (boilerLow < 30) {
    console.log(`   ℹ️  Boiler temp OK (${boilerLow.toFixed(1)}°C) - ready for WP`);
  }
  if (status[TOPICS.mqtt_valid] !== '1') {
    console.log('   ⚠️  MQTT invalid! Check meter input');
  }
  
  const alarmStr = status[TOPICS.alarms] || '{}';
  try {
    const alarms = JSON.parse(alarmStr);
    const activeAlarms = Object.entries(alarms)
      .filter(([_, v]) => v === true)
      .map(([k]) => k);
    if (activeAlarms.length > 0) {
      console.log(`   🚨 Active alarms: ${activeAlarms.join(', ')}`);
    }
  } catch (e) {}
  
  console.log('\nPress Ctrl+C to exit. Updates every 2s...\n');
}

async function main() {
  try {
    await connect();
    await new Promise(r => setTimeout(r, 1000));
    
    setInterval(displayStatus, 2000);
    displayStatus();
    
  } catch (err) {
    console.error('✗ Error:', err.message);
    process.exit(1);
  }
}

main();
