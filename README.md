# ESP32 Walkie-Talkie com Transcrição Automática

Projeto de um Walkie-Talkie em ESP32 usando UDP broadcast ou ESP-NOW para a disciplina Microcontroladores do curso Ciências da Computação 7º semestre do IFCE Maracanaú.

O projeto adiciona recursos de armazenamento de áudio em cartão SD, integração com Telegram e transcrição automática via Google Gemini.

## Funcionalidades

- Comunicação bidirecional de áudio via ESP-NOW ou UDP broadcast
- Armazenamento automático de áudio em cartão SD (formato WAV)
- Integração com Telegram para envio de áudios
- Transcrição automática dos áudios usando Google Gemini API
- Suporte a múltiplos idiomas na transcrição
- Interface com botão PTT (Push-to-Talk) e LEDs indicadores
- Funciona sem necessidade de rede WiFi (no modo ESP-NOW)

## Pré-requisitos

### Hardware

- 2x ESP32 DevKit ou similar
- 2x Microfone I2S INMP441
- 2x Amplificador de áudio (PAM8403 ou similar)
- 2x Speaker 8Ω 0,5W
- 2x Módulo SD Card (interface SPI)
- 2x Botão PTT
- LEDs e resistores para indicadores
- Jumpers e protoboard para montagem

### Software

- VS Code com extensão PlatformIO
- Bibliotecas necessárias (configuradas no platformio.ini):
  - ArduinoJson
  - HTTPClient
  - WiFiClientSecure
  - Bibliotecas ESP32 nativas (WiFi, SD, SPI)

### APIs

- Bot do Telegram (token e chat_id)
- Google Gemini API (chave de API)

## Configuração

1. **Configuração das credenciais**:
   - Crie um arquivo `credentials.h` na pasta `data/`:

   ```cpp
   #pragma once

   // WiFi credentials
   #define WIFI_SSID "your-ssid"
   #define WIFI_PSWD "your-password"

   // Gemini API credentials
   #define GEMINI_API_KEY "your-gemini-api-key"

   // Telegram Bot credentials
   #define BOT_TOKEN "your-bot-token"
   #define CHAT_ID "your-chat-id"
   ```

2. **Configuração dos pinos**:
   No arquivo `src/config.h`, configure os pinos de acordo com sua montagem:

   ```cpp
   // I2S Microphone Pins
   #define I2S_MIC_SCK_PIN GPIO_NUM_18
   #define I2S_MIC_WS_PIN  GPIO_NUM_19
   #define I2S_MIC_SD_PIN  GPIO_NUM_21

   // SD Card Pins (SPI)
   #define SD_CS_PIN   GPIO_NUM_15
   #define SD_MOSI_PIN GPIO_NUM_23
   #define SD_MISO_PIN GPIO_NUM_19
   #define SD_SCK_PIN  GPIO_NUM_18

   // Push to Talk Button
   #define GPIO_TRANSMIT_BUTTON GPIO_NUM_23
   ```

3. **Modo de comunicação**:
   Para usar UDP Broadcast em vez de ESP-NOW, comente a linha em `config.h`:

   ```cpp
   // #define USE_ESP_NOW
   ```

## Compilação e Upload

1. Abra o projeto no VS Code com PlatformIO
2. Selecione o ambiente correto (tinypico ou lolin32)
3. Conecte o ESP32 via USB
4. Compile e faça upload:
   - Use o botão "Build" para compilar
   - Use o botão "Upload" para enviar para o ESP32

## Uso

1. **Comunicação básica**:
   - Pressione e segure o botão PTT para falar
   - Solte o botão para ouvir
   - O LED vermelho indica transmissão
   - O LED verde indica recepção

2. **Recursos avançados**:
   - Cada transmissão é automaticamente salva no cartão SD
   - O áudio é enviado para o chat do Telegram configurado
   - A transcrição é gerada e enviada como mensagem de texto

## Estrutura do Projeto

```plaintext
esp32-walkie-talkie/
├── data/
│   └── credentials.h        # Credenciais (WiFi, API, etc)
├── include/
├── lib/
│   ├── audio_input/        # Biblioteca para entrada de áudio
│   ├── audio_output/       # Biblioteca para saída de áudio
│   └── transport/          # Biblioteca para comunicação
├── src/
│   ├── Application.cpp     # Implementação principal
│   ├── Application.h       # Definições da classe principal
│   ├── config.cpp          # Configurações
│   ├── config.h           # Definições de pinos e configurações
│   └── main.cpp           # Ponto de entrada
└── platformio.ini         # Configuração do PlatformIO
```

## Referências

- [ESP32 Audio Series](https://www.youtube.com/playlist?list=PL5vDt5AALlRfGVUv2x7riDMIOX34udtKD)
- [ESP-NOW Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html)
- [Google Gemini API](https://ai.google.dev/gemini-api/docs?hl=pt-br)
- [Telegram Bot API](https://core.telegram.org/bots/api)

- Baseado no trabalho original de [atomic14](https://github.com/atomic14/esp32-walkie-talkie)
