#pragma once

#include <cstdint>
#include <Arduino.h>

// ─── Network ───────────────────────────────────────────────────────────────
static const uint8_t  OPTA2_MAC[]  = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x02 };
static const uint8_t  OPTA2_IP[]   = { 192, 168, 0, 51 };
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

// ─── MQTT – Home Assistant → Opta2 commands ────────────────────────────────
#define TOPIC_CMD_SP_HOTTUB_TARGET  "opta2/cmd/sp_hottub_target_c"
#define TOPIC_CMD_SP_HOTTUB_HYST    "opta2/cmd/sp_hottub_hyst_c"
#define TOPIC_CMD_SP_HOTTUB_MAX     "opta2/cmd/sp_hottub_max_c"
#define TOPIC_CMD_ENABLE_HOTTUB     "opta2/cmd/enable_hottub"
#define TOPIC_CMD_ENABLE_LEVELPUMP  "opta2/cmd/enable_level_pump"

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
