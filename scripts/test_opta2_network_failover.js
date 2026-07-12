#!/usr/bin/env node
/**
 * Opta2 network failover monitor.
 *
 * Use this script while physically unplugging/plugging the Opta2 LAN cable.
 * It validates that transport falls back to WiFi and returns to LAN priority
 * when the cable is restored.
 */

const mqtt = require('mqtt');

const BROKER_URL = process.env.MQTT_URL || 'mqtt://192.168.0.10:1883';
const TEST_TIMEOUT_MS = 10 * 60 * 1000;

const TOPICS = [
  'opta2/status/network_transport',
  'opta2/device/heartbeat',
  'opta2/status/comm_ok',
  'opta2/status/alarms',
];

const observed = [];
let sawWifi = false;
let sawLanAfterWifi = false;
let firstTransport = null;
let finished = false;

function ts() {
  return new Date().toISOString();
}

function printHeader() {
  console.log('============================================================');
  console.log('Opta2 Network Failover Test');
  console.log('============================================================');
  console.log(`Broker: ${BROKER_URL}`);
  console.log('');
  console.log('Steps to execute physically:');
  console.log('1. Start with LAN cable connected to Opta2.');
  console.log('2. Wait until transport = lan.');
  console.log('3. Unplug LAN cable and wait until transport = wifi.');
  console.log('4. Plug LAN cable back in and verify transport returns to lan.');
  console.log('');
}

function recordTransition(transport) {
  const entry = { at: ts(), transport };
  observed.push(entry);
  console.log(`[${entry.at}] transport=${transport}`);

  if (!firstTransport) {
    firstTransport = transport;
  }
  if (transport === 'wifi') {
    sawWifi = true;
  }
  if (sawWifi && transport === 'lan') {
    sawLanAfterWifi = true;
  }
}

function summarizeAndExit(code) {
  if (finished) {
    return;
  }
  finished = true;

  console.log('');
  console.log('-------------------- Summary --------------------');
  console.log(`Transitions seen: ${observed.length}`);
  for (const item of observed) {
    console.log(`- ${item.at}: ${item.transport}`);
  }
  console.log(`Saw WiFi fallback: ${sawWifi ? 'YES' : 'NO'}`);
  console.log(`Saw LAN recovery after WiFi: ${sawLanAfterWifi ? 'YES' : 'NO'}`);

  if (code === 0) {
    console.log('Result: PASS - LAN recovered and regained priority.');
  } else {
    console.log('Result: FAIL - expected fallback/recovery sequence incomplete.');
  }

  process.exit(code);
}

printHeader();

const client = mqtt.connect(BROKER_URL, {
  clientId: `opta2_network_test_${Date.now()}`,
  reconnectPeriod: 1000,
  connectTimeout: 5000,
});

const timeout = setTimeout(() => {
  console.log('');
  console.log('Timeout reached before full sequence was observed.');
  client.end(true);
  summarizeAndExit(1);
}, TEST_TIMEOUT_MS);

client.on('connect', () => {
  console.log(`[${ts()}] MQTT connected`);
  client.subscribe(TOPICS, (err) => {
    if (err) {
      console.error('Subscribe failed:', err.message);
      clearTimeout(timeout);
      summarizeAndExit(1);
      return;
    }
    console.log(`Subscribed to ${TOPICS.length} topics`);
  });
});

client.on('message', (topic, message) => {
  const payload = message.toString('utf8');
  if (topic === 'opta2/status/network_transport') {
    recordTransition(payload);
    if (sawLanAfterWifi) {
      clearTimeout(timeout);
      client.end(true);
      summarizeAndExit(0);
    }
    return;
  }

  if (topic === 'opta2/device/heartbeat' || topic === 'opta2/status/comm_ok') {
    console.log(`[${ts()}] ${topic}=${payload}`);
  }
});

client.on('error', (err) => {
  console.error(`[${ts()}] MQTT error: ${err.message}`);
});

client.on('close', () => {
  if (!finished) {
    console.log(`[${ts()}] MQTT disconnected`);
  }
});

process.on('SIGINT', () => {
  console.log('\nInterrupted by user');
  clearTimeout(timeout);
  client.end(true);
  summarizeAndExit(130);
});
