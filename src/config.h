#pragma once

#include <freertos/FreeRTOS.h>
#include <driver/i2s.h>
#include <driver/gpio.h>
#include "../data/credentials.h"

// SD Card Settings
#define SD_CS_PIN 15
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18
#define AUDIO_FOLDER "/audio"

// WAV settings
#define WAV_SAMPLE_RATE 16000
#define WAV_BITS_PER_SAMPLE 16
#define WAV_CHANNELS 1

// API Settings
#define MAX_AUDIO_SIZE (1024 * 1024)  // 1MB
#define MAX_JSON_SIZE 16384
#define GEMINI_API_URL "https://generativelanguage.googleapis.com/v1/models/gemini-pro:generateContent"

// sample rate for the system
#define SAMPLE_RATE 16000

// Gemini API Settings
#define GEMINI_API_URL "https://generativelanguage.googleapis.com/v1/models/gemini-pro:generateContent"
#define MAX_AUDIO_SIZE 1024 * 1024  // 1MB maximum for audio file
#define MAX_JSON_SIZE 16384         // 16KB for JSON responses

// SD Card Settings
#define SD_CS_PIN GPIO_NUM_15
#define SD_MOSI_PIN GPIO_NUM_23
#define SD_MISO_PIN GPIO_NUM_19
#define SD_SCK_PIN GPIO_NUM_18
#define AUDIO_FOLDER "/audios"

// Audio Recording Settings
#define WAV_SAMPLE_RATE 16000
#define WAV_BITS_PER_SAMPLE 16
#define WAV_CHANNELS 1

// are you using an I2S microphone - comment this if you want to use an analog mic and ADC input
// #define USE_I2S_MIC_INPUT

// I2S Microphone Settings

// Which channel is the I2S microphone on? I2S_CHANNEL_FMT_ONLY_LEFT or I2S_CHANNEL_FMT_ONLY_RIGHT
// Generally they will default to LEFT - but you may need to attach the L/R pin to GND
#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_LEFT
// #define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_RIGHT
#define I2S_MIC_SERIAL_CLOCK GPIO_NUM_18
#define I2S_MIC_LEFT_RIGHT_CLOCK GPIO_NUM_19
#define I2S_MIC_SERIAL_DATA GPIO_NUM_21

// Analog Microphone Settings - ADC1_CHANNEL_7 is GPIO35
#define ADC_MIC_CHANNEL ADC1_CHANNEL_7

// speaker settings
#define USE_I2S_SPEAKER_OUTPUT
#define I2S_SPEAKER_SERIAL_CLOCK GPIO_NUM_18
#define I2S_SPEAKER_LEFT_RIGHT_CLOCK GPIO_NUM_19
#define I2S_SPEAKER_SERIAL_DATA GPIO_NUM_5
// Shutdown line if you have this wired up or -1 if you don't
#define I2S_SPEAKER_SD_PIN GPIO_NUM_22

// transmit button
#define GPIO_TRANSMIT_BUTTON 23

// Which LED pin do you want to use? TinyPico LED or the builtin LED of a generic ESP32 board?
// Comment out this line to use the builtin LED of a generic ESP32 board
// #define USE_LED_GENERIC

// Which transport do you want to use? ESP_NOW or UDP?
// comment out this line to use UDP
// #define USE_ESP_NOW

// On which wifi channel (1-11) should ESP-Now transmit? The default ESP-Now channel on ESP32 is channel 1
#define ESP_NOW_WIFI_CHANNEL 1

// In case all transport packets need a header (to avoid interference with other applications or walkie talkie sets), 
// specify TRANSPORT_HEADER_SIZE (the length in bytes of the header) in the next line, and define the transport header in config.cpp
#define TRANSPORT_HEADER_SIZE 0
extern uint8_t transport_header[TRANSPORT_HEADER_SIZE];


// i2s config for using the internal ADC
extern i2s_config_t i2s_adc_config;
// i2s config for reading from of I2S
extern i2s_config_t i2s_mic_Config;
// i2s microphone pins
extern i2s_pin_config_t i2s_mic_pins;
// i2s speaker pins
extern i2s_pin_config_t i2s_speaker_pins;
