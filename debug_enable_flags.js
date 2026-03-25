#!/usr/bin/env node
/**
 * Debug: Check if enable flags are actually being received and persisted
 */

const mqtt = require('mqtt');
const BROKER_IP = '192.168.0.10';
const BROKER_URL = `mqtt://${BROKER_IP}:1883`;

let client = null;

async function main() {
  return new Promise((resolve) => {
    client = mqtt.connect(BROKER_URL, { clientId: 'debug_flags_' + Date.now() });
    
    client.on('connect', async () => {
      console.log('Connected. Publishing enable flags with RETAIN...\n');
      
      // Publish with RETAIN flag so Opta receives them even after reconnect
      client.publish('opta1/cmd/enable_system', '1', { qos: 1, retain: true });
      console.log('✓ Sent: enable_system = 1 (retained)');
      
      client.publish('opta1/cmd/enable_wp_boiler', '1', { qos: 1, retain: true });
      console.log('✓ Sent: enable_wp_boiler = 1 (retained)');
      
      // Wait for Opta to process
      await new Promise(r => setTimeout(r, 3000));
      
      // Subscribe to status to check effect
      client.subscribe('opta1/status/wp_active');
      client.subscribe('opta1/status/surplus_fase1_w');
      client.subscribe('opta1/status/active_priority');
      
      console.log('\nWaiting 3s for Opta to process and respond...');
      
      await new Promise(r => setTimeout(r, 3000));
      
      console.log('\nInjecting high surplus now...');
      
      // Inject high surplus
      client.publish('b0b21c913c34/PUB/CH1', JSON.stringify({ P: 5000 }));
      client.publish('b0b21c913c34/PUB/CH10', JSON.stringify({ P: 100 }));
      client.publish('b0b21c913c34/PUB/CH13', JSON.stringify({ P: 6000 }));
      client.publish('b0b21c913c34/PUB/CH14', JSON.stringify({ P: 100 }));
      
      console.log('✓ Injected: 4900W phase-1 surplus');
      
      console.log('\nMonitoring responses for 5 seconds...\n');
      
      const responses = {};
      client.on('message', (topic, msg) => {
        const val = msg.toString();
        if (!responses[topic]) {
          responses[topic] = [];
        }
        responses[topic].push(val);
        console.log(`📨 ${topic} = ${val}`);
      });
      
      await new Promise(r => setTimeout(r, 5000));
      
      console.log('\n═══════════════════════════════════════');
      console.log('SUMMARY:');
      console.log('═══════════════════════════════════════');
      console.log(`WP Active: ${responses['opta1/status/wp_active']?.pop() || 'no data'}`);
      console.log(`Priority: ${responses['opta1/status/active_priority']?.pop() || 'no data'} (0=IDLE, 1=WP)`);
      console.log(`Surplus F1: ${responses['opta1/status/surplus_fase1_w']?.pop() || 'no data'}W`);
      
      if (responses['opta1/status/wp_active']?.pop() === '1') {
        console.log('\n✓✓✓ WP ACTIVATED! Issue resolved.\n');
      } else {
        console.log('\n✗ WP still not active. State machine issue.\n');
      }
      
      client.end();
      resolve();
    });
    
    client.on('error', (err) => {
      console.error('Error:', err.message);
      process.exit(1);
    });
  });
}

main();
