#pragma once

#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

using fs::File;  // Resolvendo ambiguidade do tipo File

class Output;
class I2SSampler;
class Transport;
class OutputBuffer;
class IndicatorLed;

class Application
{
private:
    Output *m_output;
    I2SSampler *m_input;
    Transport *m_transport;
    IndicatorLed *m_indicator_led;
    OutputBuffer *m_output_buffer;
    bool m_sd_initialized;
    bool m_wifi_connected;
    String m_last_transcription;
    String m_current_audio_file;
    
    // Telegram bot components
    WiFiClientSecure m_client;
    UniversalTelegramBot* m_bot;
    unsigned long m_last_bot_check;
    const unsigned long m_bot_check_interval = 1000; // Check every second
    
    // SD Card functions
    bool initSDCard();
    void createAudioDirectory();
    void writeWAVHeader(File* file, size_t length);
    bool saveAudioToSD(const int16_t* samples, size_t length);
    String getTimestampFilename();

    // Transcription functions
    bool initWiFi();
    String readWAVFile(const char* filepath);
    String transcribeAudio(const String& audioBase64);
    String base64Encode(const uint8_t* data, size_t length);
    void processAudioFile(const char* filepath);
    
    // Telegram functions
    bool initTelegramBot();
    bool sendAudioFileToTelegram(const char* filepath);
    bool sendMessageToTelegram(const String& message);
    void checkAndProcessNewAudioFile();
    void handleCurrentAudioFile();

public:
    Application();
    ~Application();
    void begin();
    void loop();
    String getLastTranscription() { return m_last_transcription; }
};
