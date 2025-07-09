#include <Arduino.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <SD.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include "Application.h"
#include "I2SMEMSSampler.h"
#include "ADCSampler.h"
#include "I2SOutput.h"
#include "DACOutput.h"
#include "UdpTransport.h"
#include "EspNowTransport.h"
#include "OutputBuffer.h"
#include "config.h"

#ifdef ARDUINO_TINYPICO
#include "TinyPICOIndicatorLed.h"
#else
#include "GenericDevBoardIndicatorLed.h"
#endif

static void application_task(void *param)
{
  // delegate onto the application
  Application *application = reinterpret_cast<Application *>(param);
  application->loop();
}

Application::Application()
{
  m_output_buffer = new OutputBuffer(300 * 16);
#ifdef USE_I2S_MIC_INPUT
  m_input = new I2SMEMSSampler(I2S_NUM_0, i2s_mic_pins, i2s_mic_Config,128);
#else
  m_input = new ADCSampler(ADC_UNIT_1, ADC1_CHANNEL_7, i2s_adc_config);
#endif

#ifdef USE_I2S_SPEAKER_OUTPUT
  m_output = new I2SOutput(I2S_NUM_0, i2s_speaker_pins);
#else
  m_output = new DACOutput(I2S_NUM_0);
#endif

#ifdef USE_ESP_NOW
  m_transport = new EspNowTransport(m_output_buffer,ESP_NOW_WIFI_CHANNEL);
#else
  m_transport = new UdpTransport(m_output_buffer);
#endif

  m_transport->set_header(TRANSPORT_HEADER_SIZE,transport_header);

#ifdef ARDUINO_TINYPICO
  m_indicator_led = new TinyPICOIndicatorLed();
#else
  m_indicator_led = new GenericDevBoardIndicatorLed();
#endif

  if (I2S_SPEAKER_SD_PIN != -1)
  {
    pinMode(I2S_SPEAKER_SD_PIN, OUTPUT);
  }
}

void Application::begin()
{
  // show a flashing indicator that we are trying to connect
  m_indicator_led->set_default_color(0);
  m_indicator_led->set_is_flashing(true, 0xff0000);
  m_indicator_led->begin();

  Serial.print("My IDF Version is: ");
  Serial.println(esp_get_idf_version());

  // Initialize WiFi
  m_wifi_connected = initWiFi();
  if (!m_wifi_connected) {
      Serial.println("WiFi connection failed!");
      return;
  }

  // Initialize SD Card
  m_sd_initialized = initSDCard();
  if (!m_sd_initialized) {
      Serial.println("SD Card initialization failed!");
      return;
  }

  // Initialize Telegram bot
  if (!initTelegramBot()) {
      Serial.println("Telegram bot initialization failed!");
      return;
  }

  // bring up WiFi
  WiFi.mode(WIFI_STA);
#ifndef USE_ESP_NOW
  WiFi.begin(WIFI_SSID, WIFI_PSWD);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  // this has a dramatic effect on packet RTT
  WiFi.setSleep(WIFI_PS_NONE);
  Serial.print("My IP Address is: ");
  Serial.println(WiFi.localIP());
#else
  // but don't connect if we're using ESP NOW
  WiFi.disconnect();
#endif
  Serial.print("My MAC Address is: ");
  Serial.println(WiFi.macAddress());
  // do any setup of the transport
  m_transport->begin();
  // connected so show a solid green light
  m_indicator_led->set_default_color(0x00ff00);
  m_indicator_led->set_is_flashing(false, 0x00ff00);
  // setup the transmit button
  pinMode(GPIO_TRANSMIT_BUTTON, INPUT_PULLDOWN);
  // start off with i2S output running
  m_output->start(SAMPLE_RATE);
  // flush all samples received during startup
  m_output_buffer->flush();
  // start the main task for the application
  TaskHandle_t task_handle;
  xTaskCreate(application_task, "application_task", 8192, this, 1, &task_handle);
}

// application task - coordinates everything
void Application::loop()
{
  int16_t *samples = reinterpret_cast<int16_t *>(malloc(sizeof(int16_t) * 128));
  
  // continue forever
  while (true)
  {
    // Check for new audio files to process
    if (millis() - m_last_bot_check > m_bot_check_interval) {
      checkAndProcessNewAudioFile();
      m_last_bot_check = millis();
    }

    // do we need to start transmitting?
    if (digitalRead(GPIO_TRANSMIT_BUTTON))
    {
      Serial.println("Started transmitting");
      m_indicator_led->set_is_flashing(true, 0xff0000);
      // stop the output as we're switching into transmit mode
      m_output->stop();
      // start the input to get samples from the microphone
      m_input->start();
      // transmit for at least 1 second or while the button is pushed
      unsigned long start_time = millis();
      while (millis() - start_time < 1000 || digitalRead(GPIO_TRANSMIT_BUTTON))
      {
        // read samples from the microphone
        int samples_read = m_input->read(samples, 128);
        if (samples_read > 0) {
          // Save audio to SD card
          if (m_sd_initialized) {
            saveAudioToSD(samples, samples_read);
          }
          
          // Send audio through transport
          for (int i = 0; i < samples_read; i++) {
            m_transport->add_sample(samples[i]);
          }
        }
      }
      // send all packets still in the transport buffer
      m_transport->flush();
      // finished transmitting stop the input and start the output
      Serial.println("Finished transmitting");
      m_indicator_led->set_is_flashing(false, 0xff0000);
      m_input->stop();
      m_output->start(SAMPLE_RATE);
    }
    // while the transmit button is not pushed and 1 second has not elapsed
    Serial.println("Started Receiving");
    if (I2S_SPEAKER_SD_PIN != -1)
    {
      digitalWrite(I2S_SPEAKER_SD_PIN, HIGH);
    }
    unsigned long start_time = millis();
    while (millis() - start_time < 1000 || !digitalRead(GPIO_TRANSMIT_BUTTON))
    {
      // read from the output buffer (which should be getting filled by the transport)
      m_output_buffer->remove_samples(samples, 128);
      // and send the samples to the speaker
      m_output->write(samples, 128);
    }
    if (I2S_SPEAKER_SD_PIN != -1)
    {
      digitalWrite(I2S_SPEAKER_SD_PIN, LOW);
    }
    Serial.println("Finished Receiving");
  }
}

bool Application::initSDCard() {
    SPIClass spiSD(VSPI);
    spiSD.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    
    if(!SD.begin(SD_CS_PIN, spiSD)) {
        Serial.println("Card Mount Failed");
        return false;
    }

    if(!SD.exists("/")) {
        Serial.println("Failed to read root directory");
        return false;
    }

    createAudioDirectory();
    return true;
}

void Application::createAudioDirectory() {
    if(!SD.exists(AUDIO_FOLDER)) {
        if(SD.mkdir(AUDIO_FOLDER)) {
            Serial.println("Audio directory created");
        } else {
            Serial.println("Audio directory creation failed");
        }
    }
}

String Application::getTimestampFilename() {
    return String(AUDIO_FOLDER) + "/audio_" + String(millis()) + ".wav";
}

void Application::writeWAVHeader(File* file, size_t length) {
    unsigned char header[44];
    unsigned long fileSize = length * sizeof(int16_t) + 36;

    // RIFF chunk descriptor
    header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
    header[4] = (fileSize & 0xFF);
    header[5] = ((fileSize >> 8) & 0xFF);
    header[6] = ((fileSize >> 16) & 0xFF);
    header[7] = ((fileSize >> 24) & 0xFF);
    header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';

    // fmt sub-chunk
    header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
    header[16] = 16; // SubChunk1Size is 16
    header[17] = 0;
    header[18] = 0;
    header[19] = 0;
    header[20] = 1; // PCM = 1
    header[21] = 0;
    header[22] = WAV_CHANNELS;
    header[23] = 0;
    
    // Sample rate
    header[24] = (WAV_SAMPLE_RATE & 0xFF);
    header[25] = ((WAV_SAMPLE_RATE >> 8) & 0xFF);
    header[26] = ((WAV_SAMPLE_RATE >> 16) & 0xFF);
    header[27] = ((WAV_SAMPLE_RATE >> 24) & 0xFF);
    
    // Byte rate
    unsigned long byteRate = WAV_SAMPLE_RATE * WAV_CHANNELS * (WAV_BITS_PER_SAMPLE / 8);
    header[28] = (byteRate & 0xFF);
    header[29] = ((byteRate >> 8) & 0xFF);
    header[30] = ((byteRate >> 16) & 0xFF);
    header[31] = ((byteRate >> 24) & 0xFF);
    
    // Block align
    header[32] = WAV_CHANNELS * (WAV_BITS_PER_SAMPLE / 8);
    header[33] = 0;
    
    // Bits per sample
    header[34] = WAV_BITS_PER_SAMPLE;
    header[35] = 0;

    // data sub-chunk
    header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
    header[40] = (length & 0xFF);
    header[41] = ((length >> 8) & 0xFF);
    header[42] = ((length >> 16) & 0xFF);
    header[43] = ((length >> 24) & 0xFF);

    file->write(header, sizeof(header));
}

bool Application::saveAudioToSD(const int16_t* samples, size_t length) {
    if (!m_sd_initialized) {
        return false;
    }

    // If we don't have a current file, create one
    if (m_current_audio_file.length() == 0) {
        m_current_audio_file = getTimestampFilename();
        File file = SD.open(m_current_audio_file.c_str(), FILE_WRITE);
        if (!file) {
            Serial.println("Failed to create new audio file");
            m_current_audio_file = "";
            return false;
        }
        writeWAVHeader(&file, length);
        file.close();
    }

    // Append the samples to the current file
    File file = SD.open(m_current_audio_file.c_str(), FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open audio file for appending");
        return false;
    }

    size_t written = file.write((const uint8_t*)samples, length * sizeof(int16_t));
    file.close();

    return written == length * sizeof(int16_t);
}

bool Application::initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PSWD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    m_wifi_connected = (WiFi.status() == WL_CONNECTED);
    return m_wifi_connected;
}

String Application::base64Encode(const uint8_t* data, size_t length) {
    const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    String encoded;
    encoded.reserve(((length + 2) / 3) * 4);

    for (size_t i = 0; i < length; i += 3) {
        uint32_t octet_a = i < length ? data[i] : 0;
        uint32_t octet_b = i + 1 < length ? data[i + 1] : 0;
        uint32_t octet_c = i + 2 < length ? data[i + 2] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded += base64_chars[(triple >> 18) & 0x3F];
        encoded += base64_chars[(triple >> 12) & 0x3F];
        encoded += base64_chars[(triple >> 6) & 0x3F];
        encoded += base64_chars[triple & 0x3F];
    }

    switch (length % 3) {
        case 1:
            encoded[encoded.length() - 2] = '=';
            encoded[encoded.length() - 1] = '=';
            break;
        case 2:
            encoded[encoded.length() - 1] = '=';
            break;
    }

    return encoded;
}

String Application::readWAVFile(const char* filepath) {
    if (!m_sd_initialized) {
        Serial.println("SD card not initialized");
        return "";
    }
    
    File audioFile = SD.open(filepath);
    if (!audioFile) {
        Serial.println("Failed to open audio file for reading");
        return "";
    }
    
    size_t fileSize = audioFile.size();
    if (fileSize > MAX_AUDIO_SIZE) {
        Serial.println("Audio file too large");
        audioFile.close();
        return "";
    }
    
    // Alocar buffer para o arquivo inteiro (incluindo cabeçalho WAV)
    uint8_t* buffer = (uint8_t*)malloc(fileSize);
    if (!buffer) {
        Serial.println("Failed to allocate memory for audio data");
        audioFile.close();
        return "";
    }
    
    size_t bytesRead = audioFile.read(buffer, fileSize);
    if (bytesRead != fileSize) {
        Serial.println("Failed to read complete file");
        free(buffer);
        audioFile.close();
        return "";
    }
    
    String base64Audio = base64Encode(buffer, bytesRead);
    
    free(buffer);
    audioFile.close();
    
    if (base64Audio.length() == 0) {
        Serial.println("Failed to encode audio to base64");
        return "";
    }
    
    return base64Audio;
}

String Application::transcribeAudio(const String& audioBase64) {
    if (!m_wifi_connected) {
        Serial.println("WiFi not connected");
        return "";
    }
    
    HTTPClient http;
    String url = String(GEMINI_API_URL) + "?key=" + String(GEMINI_API_KEY);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    // Criar o JSON para a API do Gemini
    DynamicJsonDocument doc(MAX_JSON_SIZE);
    JsonArray contents = doc.createNestedArray("contents");
    JsonObject content = contents.createNestedObject();
    JsonArray parts = content.createNestedArray("parts");
    
    // Adicionar o texto do prompt
    JsonObject textPart = parts.createNestedObject();
    textPart["text"] = "Por favor, transcreva esse áudio em texto.";
    
    // Adicionar o áudio
    JsonObject audioPart = parts.createNestedObject();
    JsonObject inlineData = audioPart.createNestedObject("inlineData");
    inlineData["mimeType"] = "audio/wav";
    inlineData["data"] = audioBase64;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    int httpCode = http.POST(jsonString);
    String transcription;
    
    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        
        DynamicJsonDocument responseDoc(4096);
        deserializeJson(responseDoc, response);
        
        transcription = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    } else {
        Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return transcription;
}

void Application::processAudioFile(const char* filepath) {
    if (!m_sd_initialized || !m_wifi_connected) return;
    
    String audioBase64 = readWAVFile(filepath);
    if (audioBase64.length() > 0) {
        m_last_transcription = transcribeAudio(audioBase64);
        Serial.println("Transcription: " + m_last_transcription);
    }
}

bool Application::initTelegramBot()
{
    m_client.setInsecure(); // Required for HTTPS but skips certificate verification
    m_bot = new UniversalTelegramBot(BOT_TOKEN, m_client);
    m_last_bot_check = 0;
    return true;
}

bool Application::sendAudioFileToTelegram(const char* filepath)
{
    if (!m_bot) {
        Serial.println("Telegram bot not initialized");
        return false;
    }

    File audioFile = SD.open(filepath);
    if (!audioFile) {
        Serial.println("Failed to open audio file");
        return false;
    }

    String fileName = String(filepath);
    fileName = fileName.substring(fileName.lastIndexOf('/') + 1);

    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendAudio";

    http.begin(url);
    
    // Use URLEncoded form instead of multipart for simplicity
    String data = "chat_id=" + String(CHAT_ID);
    data += "&caption=" + String("Audio file: ") + fileName;
    data += "&audio=" + String("attach://audio.wav");
    
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    // Send the request
    int httpCode = http.sendRequest("POST", data);
    String response = http.getString();
    http.end();
    audioFile.close();

    if (httpCode == HTTP_CODE_OK) {
        Serial.println("Audio sent to Telegram successfully");
        Serial.println("Response: " + response);
        return true;
    } else {
        Serial.printf("Failed to send audio to Telegram. HTTP Code: %d\n", httpCode);
        Serial.println("Response: " + response);
        return false;
    }
}

bool Application::sendMessageToTelegram(const String& message)
{
    if (!m_bot) {
        Serial.println("Telegram bot not initialized");
        return false;
    }

    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    String data = "chat_id=" + String(CHAT_ID);
    data += "&text=" + message;
    data += "&parse_mode=HTML";
    
    int httpCode = http.POST(data);
    String response = http.getString();
    http.end();

    if (httpCode == HTTP_CODE_OK) {
        Serial.println("Message sent to Telegram successfully");
        return true;
    } else {
        Serial.printf("Failed to send message to Telegram. HTTP Code: %d\n", httpCode);
        Serial.println("Response: " + response);
        return false;
    }
}

void Application::handleCurrentAudioFile()
{
    if (m_current_audio_file.length() > 0) {
        Serial.printf("Processing audio file: %s\n", m_current_audio_file.c_str());
        
        // First, send the audio file to Telegram
        if (sendAudioFileToTelegram(m_current_audio_file.c_str())) {
            // Then process it with Gemini API
            processAudioFile(m_current_audio_file.c_str());
            
            // If we got a transcription, send it to Telegram
            if (m_last_transcription.length() > 0) {
                String message = "<b>📝 Transcription:</b>\n\n" + m_last_transcription;
                sendMessageToTelegram(message);
            }
        }
        
        m_current_audio_file = ""; // Clear current file
    }
}

void Application::checkAndProcessNewAudioFile()
{
    handleCurrentAudioFile(); // Process any pending file first
}

Application::~Application()
{
    delete m_bot;
    delete m_output_buffer;
    delete m_input;
    delete m_output;
    delete m_transport;
    delete m_indicator_led;
}
