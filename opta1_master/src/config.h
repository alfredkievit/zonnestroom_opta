#pragma once

#include <stdint.h>

// ─── Network ───────────────────────────────────────────────────────────────
// Opta1 WiFi – will use DHCP to get IP in 192.168.0.x subnet
static const char* OPTA1_WIFI_SSID = "optimusnexus";
static const char* OPTA1_WIFI_PASS = "kinderpindakaas";
static const uint8_t  OPTA1_MAC[]  = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 };
// Note: With DHCP, IP will be auto-assigned; MQTT broker should be on same subnet
static const uint8_t  BROKER_IP[]  = { 192, 168, 0, 10 };
static const uint16_t BROKER_PORT  = 1883;

// ─── MQTT – energy meter topics (energiemeter ID: b0b21c913c34) ───────────
#define TOPIC_METER_ROOT  "b0b21c913c34/PUB/#"    // wildcard: any meter publish
#define TOPIC_METER_PREFIX "b0b21c913c34/PUB/"    // prefix for keepalive detection
// Fase 1: WP + elektrisch element
#define TOPIC_METER_CH1   "b0b21c913c34/PUB/CH1"    // fase 1 export [W]
#define TOPIC_METER_CH10  "b0b21c913c34/PUB/CH10"   // fase 1 import [W]
// Totaal: gebruikt voor hottub beslissing
#define TOPIC_METER_CH13  "b0b21c913c34/PUB/CH13"   // totaal export [W]
#define TOPIC_METER_CH14  "b0b21c913c34/PUB/CH14"   // totaal import [W]

// ─── MQTT – Opta1 → Opta2 device topics ───────────────────────────────────
#define TOPIC_MASTER_PERM_HOTTUB  "opta1/device/permission_hottub"
#define TOPIC_MASTER_HEARTBEAT    "opta1/device/heartbeat"

// ─── MQTT – Opta1 → Home Assistant status topics ──────────────────────────
#define TOPIC_HA_SURPLUS_F1       "opta1/status/surplus_fase1_w"
#define TOPIC_HA_SURPLUS_TOTAAL   "opta1/status/surplus_totaal_w"
#define TOPIC_HA_BOILER_TEMP_LOW  "opta1/status/boiler_temp_low"
#define TOPIC_HA_BOILER_TEMP_HIGH "opta1/status/boiler_temp_high"
#define TOPIC_HA_ACTIVE_PRIORITY  "opta1/status/active_priority"
#define TOPIC_HA_MQTT_LAST_SEEN   "opta1/status/mqtt_last_seen"
#define TOPIC_HA_MQTT_VALID       "opta1/status/mqtt_valid"
#define TOPIC_HA_WP_ACTIVE        "opta1/status/wp_active"
#define TOPIC_HA_ELEMENT_ACTIVE   "opta1/status/element_active"
#define TOPIC_HA_HOTTUB_PERM      "opta1/status/hottub_permission"
#define TOPIC_HA_AUTO_WP_REQ      "opta1/status/auto_wp_request"
#define TOPIC_HA_AUTO_ELEMENT_REQ "opta1/status/auto_element_request"
#define TOPIC_HA_AUTO_HOTTUB_REQ  "opta1/status/auto_hottub_request"
#define TOPIC_HA_WP_BLOCK_REASON  "opta1/status/wp_block_reason"
#define TOPIC_HA_ELEMENT_THERM_OK "opta1/status/element_thermostat_ok"
#define TOPIC_HA_ALARM_JSON       "opta1/status/alarms"

// ─── MQTT – Opta1 → Home Assistant extra status ───────────────────────────
#define TOPIC_HA_COMFORT_ACTIVE   "opta1/status/comfort_active"
#define TOPIC_HA_MANUAL_MODE      "opta1/status/manual_mode"

// ─── MQTT – External sensor data → Opta1 (live, not retained) ──────────────
#define TOPIC_EXTERN_COMPRESSOR_FREQ  "opta1/extern/compressor_freq_hz"

// ─── MQTT – Home Assistant → Opta1 command topics (retained) ──────────────
#define TOPIC_CMD_ENABLE_ELEMENT      "opta1/cmd/enable_boiler_element"
#define TOPIC_CMD_ENABLE_HOTTUB       "opta1/cmd/enable_hottub"
#define TOPIC_CMD_SP_WP_TARGET        "opta1/cmd/sp_boiler_wp_target_c"
#define TOPIC_CMD_SP_WP_HYST          "opta1/cmd/sp_boiler_wp_hyst_c"
#define TOPIC_CMD_SP_ELEMENT_TARGET   "opta1/cmd/sp_boiler_element_target_c"
#define TOPIC_CMD_SP_ELEMENT_HYST     "opta1/cmd/sp_boiler_element_hyst_c"
#define TOPIC_CMD_SP_SURPLUS_WP       "opta1/cmd/sp_surplus_wp_start_w"
#define TOPIC_CMD_SP_SURPLUS_ELEMENT  "opta1/cmd/sp_surplus_element_start_w"
#define TOPIC_CMD_SP_SURPLUS_HOTTUB   "opta1/cmd/sp_surplus_hottub_start_w"
#define TOPIC_CMD_SP_SURPLUS_STOP     "opta1/cmd/sp_surplus_stop_w"
#define TOPIC_CMD_MANUAL_FORCE_WP     "opta1/cmd/manual_force_wp"
#define TOPIC_CMD_MANUAL_FORCE_HOTTUB "opta1/cmd/manual_force_hottub"
#define TOPIC_CMD_MANUAL_FORCE_COMFORT "opta1/cmd/manual_force_comfort"
#define TOPIC_CMD_FAULT_RESET          "opta1/cmd/fault_reset"

// ─── MQTT – extra status (comfort / manual mode) ──────────────────────────
#define TOPIC_HA_COMFORT_ACTIVE        "opta1/status/comfort_active"
#define TOPIC_HA_MANUAL_MODE           "opta1/status/manual_mode"

// ─── Pin mapping – Opta1 ───────────────────────────────────────────────────
// Relay outputs (digital)
#define PIN_DO_WP_EXTRA_WW      D0    // contact extra warmwater WP
#define PIN_DO_WP_COMFORT_EXTRA D1    // optionele comfortverwarming (toekomstig)
#define PIN_DO_BOILER_ELEMENT   D2    // relais boiler-element 3 kW
#define PIN_DO_SPARE            D3    // reserverelais

// Analog inputs (0–10 V → PT1000 sensor)
#define PIN_AI_BOILER_LOW      A0    // boiler sensor op PLC I1 (regelwaarheid)
#define PIN_AI_BOILER_HIGH     A1    // boiler bovenste sensor (optioneel)

// Digital inputs
#define PIN_DI_ELEMENT_THERM   A2    // boilerthermostaat / beveiliging OK
#define PIN_DI_FAULT_RESET     A3    // handmatige fout-reset drukknop

// ─── ADC / sensor constants ────────────────────────────────────────────────
// Opta analoge ingang: 0–10 V op 16-bit ADC (0–65535)
// PT1000 converter: 0 V = 0 °C,  10 V = 160 °C
#define ADC_MAX_RAW        65535
#define SENSOR_VOLT_MAX    10.0f
#define SENSOR_TEMP_AT_0V  0.0f
#define SENSOR_TEMP_AT_10V 160.0f
#define SENSOR_TEMP_MIN    (-5.0f)   // grens onder nul → sensorstoring
#define SENSOR_TEMP_MAX    165.0f    // grens boven max  → sensorstoring

// ─── Heartbeat interval ────────────────────────────────────────────────────
#define HEARTBEAT_INTERVAL_MS  5000UL   // elke 5 s togglet het bit

// ─── HA publish interval ──────────────────────────────────────────────────
#define HA_PUBLISH_INTERVAL_MS 5000UL
#define HA_BOILER_HIGH_PUBLISH_INTERVAL_MS 60000UL

// ─── Diagnostics / hardening ──────────────────────────────────────────────
#define DEBUG_DIAG 1
#define SETTINGS_SAVE_DEBOUNCE_MS 3000UL
#define LOOP_HEARTBEAT_INTERVAL_MS 5000UL
#define LOOP_WARN_MS 250UL
#define LOOP_RESET_MS 3000UL
