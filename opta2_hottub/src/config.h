#pragma once

#include <cstdint>
#include <Arduino.h>

// ─── Network ───────────────────────────────────────────────────────────────
// Opta2 WiFi – uses DHCP in same subnet as broker
static const char* OPTA2_WIFI_SSID = "optimusnexus";
static const char* OPTA2_WIFI_PASS = "kinderpindakaas";
static const uint8_t  BROKER_IP[]  = { 192, 168, 0, 10 };
static const uint16_t BROKER_PORT  = 1883;

// ─── MQTT – receive from Opta 1 ────────────────────────────────────────────
#define TOPIC_MASTER_PERM_HOTTUB  "opta1/device/permission_hottub"
#define TOPIC_MASTER_HEARTBEAT    "opta1/device/heartbeat"

// ─── MQTT – Opta2 → Home Assistant status ──────────────────────────────────
#define TOPIC_HA_HOTTUB_TEMP      "opta2/status/hottub_temp"
#define TOPIC_HA_HEATER_ACTIVE    "opta2/status/heater_active"
#define TOPIC_HA_PUMP_ACTIVE      "opta2/status/pump_active"
#define TOPIC_HA_LEVEL_HIGH       "opta2/status/level_high"
#define TOPIC_HA_LEVEL_PUMP_ACT   "opta2/status/level_pump_active"
#define TOPIC_HA_COMM_OK          "opta2/status/comm_ok"
#define TOPIC_HA_ALARM_JSON       "opta2/status/alarms"
#define TOPIC_HA_CLOCK_OK         "opta2/status/clock_ok"
#define TOPIC_HA_HEARTBEAT        "opta2/device/heartbeat"

// ─── MQTT – Home Assistant → Opta2 commands ────────────────────────────────
#define TOPIC_CMD_SP_HOTTUB_TARGET  "opta2/cmd/sp_hottub_target_c"
#define TOPIC_CMD_SP_HOTTUB_HYST    "opta2/cmd/sp_hottub_hyst_c"
#define TOPIC_CMD_SP_HOTTUB_MAX     "opta2/cmd/sp_hottub_max_c"
#define TOPIC_CMD_SP_PUMP_1_HOUR    "opta2/cmd/sp_pump_run1_hour"
#define TOPIC_CMD_SP_PUMP_2_HOUR    "opta2/cmd/sp_pump_run2_hour"
#define TOPIC_CMD_SP_PUMP_DURATION  "opta2/cmd/sp_pump_run_duration_min"
#define TOPIC_CMD_ENABLE_HOTTUB     "opta2/cmd/enable_hottub"
#define TOPIC_CMD_ENABLE_AUTO_PUMP  "opta2/cmd/enable_auto_pump"
#define TOPIC_CMD_MANUAL_PUMP       "opta2/cmd/manual_force_pump"
#define TOPIC_CMD_MANUAL_LEVEL_PUMP "opta2/cmd/manual_force_level_pump"
#define TOPIC_CMD_ENABLE_LEVELPUMP  "opta2/cmd/enable_level_pump"
#define TOPIC_CMD_CLOCK_MINUTE      "opta2/device/current_minute_of_day"

// ─── Pin mapping – Opta2 ───────────────────────────────────────────────────
// Relay outputs
#define PIN_DO_HOTTUB_HEATER    D0    // hottub warmtepomp / heater
#define PIN_DO_HOTTUB_PUMP      D1    // circulatiepomp
#define PIN_DO_HOTTUB_LEVELPUMP D2    // niveaupomp
#define PIN_DO_HOTTUB_ALARM     D3    // alarm uitgang

// Analog inputs
#define PIN_AI_HOTTUB_TEMP      A0    // PT1000 0-10V

// Digital inputs
#define PIN_DI_LEVEL_HIGH       A1    // hoog-niveau schakelaar
#define PIN_DI_HOTTUB_FAULT     A2    // externe foutsignaal
#define PIN_DI_FLOW_OK          A3    // flow schakelaar

// ─── ADC / sensor constants (identical to Opta1) ───────────────────────────
#define ADC_MAX_RAW        65535
#define SENSOR_TEMP_AT_0V  0.0f
#define SENSOR_TEMP_AT_10V 160.0f
#define SENSOR_TEMP_MIN    (-5.0f)
#define SENSOR_TEMP_MAX    165.0f

// ─── Timing constants ──────────────────────────────────────────────────────
#define HA_PUBLISH_INTERVAL_MS  5000UL
#define FILTER_PUMP_RUN_DURATION_MS  (5UL  * 60UL * 1000UL)

// ─── Diagnostics / hardening ──────────────────────────────────────────────
#define DEBUG_DIAG 1
#define SETTINGS_SAVE_DEBOUNCE_MS 3000UL
#define WIFI_RETRY_INTERVAL_MS 15000UL
#define MQTT_RETRY_INTERVAL_MS 5000UL
#define CONNECT_LOG_INTERVAL_MS 10000UL
#define LOOP_HEARTBEAT_INTERVAL_MS 5000UL
#define LOOP_WARN_MS 250UL
#define LOOP_RESET_MS 3000UL
