#!/usr/bin/env node
/**
 * MQTT Connectivity & State Machine Test Script
 * Tests Opta1 (energy master) and Opta2 (hottub controller)
 * Simulates energy meter values and monitors system responses
 */

const mqtt = require('mqtt');

// Configuration
const BROKER_IP = '192.168.0.10';
const BROKER_PORT = 1883;
const BROKER_URL = `mqtt://${BROKER_IP}:${BROKER_PORT}`;

// Energy meter topics (Opta's listen to these)
const TOPICS_METER = {
  CH1: 'b0b21c913c34/PUB/CH1',      // fase 1 export
  CH10: 'b0b21c913c34/PUB/CH10',    // fase 1 import
  CH13: 'b0b21c913c34/PUB/CH13',    // totaal export
  CH14: 'b0b21c913c34/PUB/CH14',    // totaal import
};

// Opta1 status topics (we subscribe to these)
const TOPICS_STATUS = {
  surplus_f1: 'opta1/status/surplus_fase1_w',
  surplus_totaal: 'opta1/status/surplus_totaal_w',
  priority: 'opta1/status/active_priority',
  mqtt_valid: 'opta1/status/mqtt_valid',
  mqtt_lastseen: 'opta1/status/mqtt_last_seen',
  wp_active: 'opta1/status/wp_active',
  element_active: 'opta1/status/element_active',
  hottub_perm: 'opta1/status/hottub_permission',
  alarms: 'opta1/status/alarms',
  heartbeat: 'opta1/device/heartbeat',
};

// Opta2 status topics
const TOPICS_OPTA2 = {
  hottub_status: 'opta2/status/hottub_status',
  opta2_alarms: 'opta2/status/alarms',
};

// Test Scenarios
const SCENARIOS = {
  idle: {
    description: 'Geen surplus - alle lasten uit',
    CH1: 100.0,
    CH10: 100.0,    // fase1 surplus = 0W
    CH13: 150.0,
    CH14: 150.0,    // totaal surplus = 0W
    expected_state: 'IDLE',
    duration: 5,
  },
  wp_active: {
    description: 'WP zou actief moeten zijn (fase1 overschot, temp laag)',
    CH1: 1500.0,
    CH10: 100.0,    // fase1 surplus = 1400W
    CH13: 2000.0,
    CH14: 100.0,    // totaal surplus = 1900W
    expected_state: 'WP_BOILER',
    duration: 5,
  },
  drop_to_idle: {
    description: 'Surplus weg - terug naar IDLE',
    CH1: 100.0,
    CH10: 100.0,    // fase1 surplus = 0W
    CH13: 150.0,
    CH14: 150.0,    // totaal surplus = 0W
    expected_state: 'IDLE',
    duration: 5,
  },
};

// Global state
let receivedData = {};
let client = null;
let testActive = true;

// MQTT event handlers
function setupMqtt() {
  return new Promise((resolve, reject) => {
    console.log(`\nConnecting to MQTT broker at ${BROKER_URL}...\n`);
    
    client = mqtt.connect(BROKER_URL, {
      reconnectPeriod: 1000,
      connectTimeout: 5000,
      clientId: 'zonnestroom_test_client_' + Date.now(),
    });

    client.on('connect', () => {
      console.log('✓ MQTT Connected\n');
      
      // Subscribe to all status topics
      const allTopics = [...Object.values(TOPICS_STATUS), ...Object.values(TOPICS_OPTA2)];
      console.log('Subscribing to status topics:');
      allTopics.forEach(topic => {
        client.subscribe(topic, (err) => {
          if (!err) console.log(`  → ${topic}`);
        });
      });
      
      console.log('');
      resolve();
    });

    client.on('message', (topic, message) => {
      const payload = message.toString('utf-8');
      const timestamp = new Date().toLocaleTimeString();
      
      receivedData[topic] = {
        value: payload,
        timestamp: timestamp,
      };
      
      console.log(`\n📨 [${timestamp}] RX: ${topic}`);
      console.log(`   → Value: ${payload}`);
    });

    client.on('error', (err) => {
      console.error(`✗ MQTT Error: ${err.message}`);
      reject(err);
    });

    client.on('disconnect', () => {
      console.log('\n✓ Disconnected from MQTT broker');
    });

    setTimeout(() => {
      if (!client.connected) {
        reject(new Error('Failed to connect within timeout'));
      }
    }, 6000);
  });
}

// Publish meter values
function publishMeterValues(ch1, ch10, ch13, ch14) {
  console.log('\n📤 Publishing meter values:');
  
  const values = { CH1: ch1, CH10: ch10, CH13: ch13, CH14: ch14 };
  
  Object.entries(values).forEach(([channel, value]) => {
    const topic = TOPICS_METER[channel];
    const payload = JSON.stringify({ P: Math.round(value) });
    client.publish(topic, payload);
    console.log(`   ${channel.padEnd(5)} → ${Math.round(value).toString().padStart(6)}W`);
  });
}

// Run a test scenario
async function runTestScenario(name, scenario) {
  console.log('\n' + '='.repeat(70));
  console.log('SCENARIO: ' + name.toUpperCase());
  console.log('='.repeat(70));
  console.log(`Description: ${scenario.description}`);
  console.log(`Expected state: ${scenario.expected_state}`);
  console.log(`Duration: ${scenario.duration}s`);
  
  receivedData = {};
  
  // Publish meter values
  publishMeterValues(scenario.CH1, scenario.CH10, scenario.CH13, scenario.CH14);
  
  // Wait for responses
  console.log(`\nWaiting for responses...`);
  for (let i = scenario.duration; i > 0; i--) {
    await new Promise(resolve => setTimeout(resolve, 1000));
    process.stdout.write(`\r   ... ${i}s remaining`);
  }
  console.log('\n');
  
  // Display collected responses
  console.log('Responses collected during scenario:');
  if (Object.keys(receivedData).length === 0) {
    console.log('   (no responses - check MQTT broker connectivity!)');
  } else {
    Object.entries(receivedData).forEach(([topic, data]) => {
      console.log(`   ${topic}`);
      console.log(`      → ${data.value} (${data.timestamp})`);
    });
  }
}

// Print checklist
function printChecklist() {
  console.log('\n' + '='.repeat(70));
  console.log('COMMISSIONING CHECKLIST');
  console.log('='.repeat(70) + '\n');
  
  const checks = [
    ['MQTT Broker connectivity', 'Connection successful if above'],
    ['Opta1 receives meter values', 'CH1, CH10, CH13, CH14 topics'],
    ['Opta1 calculates surplus', 'surplus_fase1_w, surplus_totaal_w'],
    ['Opta1 state machine works', 'priority state transitions'],
    ['Opta1 publishes status', 'wp_active, element_active, etc.'],
    ['Opta2 heartbeat detected', 'opta1/device/heartbeat toggling'],
    ['Opta2 hottub control works', 'opta2/status/hottub_status'],
    ['Fail-safe interlocks active', 'Check alarms when expected'],
  ];
  
  checks.forEach(([name, desc]) => {
    console.log(`  ☐ ${name}`);
    console.log(`      ${desc}\n`);
  });
}

// Main execution
async function main() {
  try {
    console.log('\n' + '='.repeat(70));
    console.log('ZONNESTROOM DUAL-OPTA MQTT TEST');
    console.log('='.repeat(70));
    
    // Setup MQTT
    await setupMqtt();
    
    // Give broker time to confirm subscriptions
    await new Promise(resolve => setTimeout(resolve, 2000));
    
    // Run scenarios
    for (const [name, scenario] of Object.entries(SCENARIOS)) {
      await runTestScenario(name, scenario);
      await new Promise(resolve => setTimeout(resolve, 1000));
    }
    
    // Print checklist
    printChecklist();
    
    // Monitor for 30 seconds
    console.log('📊 Monitoring MQTT topics for 30 more seconds...\n');
    await new Promise(resolve => setTimeout(resolve, 30000));
    
    console.log('\n✓ Test completed successfully!');
    
  } catch (err) {
    console.error('\n✗ Error:', err.message);
    console.error('\nTroubleshooting:');
    console.error('  • Is MQTT broker running at 192.168.0.10:1883?');
    console.error('  • Are both Opta boards connected via Ethernet?');
    console.error('  • Check network connectivity: ping 192.168.0.10');
    process.exit(1);
  } finally {
    testActive = false;
    if (client) {
      client.end();
    }
  }
}

// Run if called directly
if (require.main === module) {
  main();
}

module.exports = { setupMqtt, publishMeterValues, runTestScenario };
