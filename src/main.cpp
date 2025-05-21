#include <WiFi.h>
#include <WebSocketsClient.h>
#include <driver/i2s.h>
#include "HX711.h"
// #include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "FontsRus/TimesNRCyr6.h"

// INMP441 pin
#define I2S_WS  15   // L/R Word Select
#define I2S_SD  32   // Serial Data
#define I2S_SCK 14   // Serial Clock
// Button pin
#define BUTTON_PIN 33       // Пин кнопки
// HX711 pin
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 4;
// RGB led pin
#define RED_PIN    25
#define GREEN_PIN  26
#define BLUE_PIN   27

#define SAMPLE_RATE 16000
#define BUFFER_SIZE 600  // Размер буфера для передачи
#define VOX_THRESHOLD 40    // Порог громкости (подбирается экспериментально)
#define VOX_SILENCE_TIME 1000  // Время (мс), через которое считаем, что говорящий замолчал
#define CALIBRATION_FACTOR 290.46 // Калибровка тензодатчика
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCALE_CHANGE_TIME 500 


const char* ssid = "POCO X3 NFC";
const char* password = "12345678";
const char* websocket_server = "aazatserver.ru";  
const uint16_t websocket_port = 5000;
const char* websocket_path = "/";

WebSocketsClient webSocket;
QueueHandle_t audioQueue;
HX711 scale;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastVoiceTime = 0;
bool isSpeaking = false;
bool allowReconnect = false;
volatile bool buttonState = HIGH;
int scaleReading;
int scaleLastReading;
int savedScaleReading = 0;
unsigned long lastScaleTime = 0;
volatile bool scaleChanged = false;
String message = "";

String deviceID = "";



void displayWeight(int weight){
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 50);
    // Display static text
    display.println("Вес:");
    display.display();
    display.setCursor(35, 50);
    display.setTextSize(3);
    display.print(weight);
    display.print(" ");
    display.print("g");
    display.display();  

    // Проверка, если второй аргумент не пустой
    display.setTextSize(2);
    display.setCursor(0, 14);  // Местоположение для второго текста
    display.println(message);
    display.display();
}


// Настройка I2S
void setupI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 10,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = false
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_stop(I2S_NUM_0);
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };
    i2s_set_pin(I2S_NUM_0, &pin_config);
}


// WiFi
void setupWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print(".");
    }
    deviceID = WiFi.macAddress();  // например, "24:6F:28:A3:B2:10"
    deviceID.replace(":", "");     // сделать ID компактнее: "246F28A3B210"
    Serial.println("\nConnected to WiFi");
}


// WebSocket
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.println("✅ Connected to WebSocket server");
            break;
        case WStype_DISCONNECTED:
            Serial.println("❌ Disconnected");
            allowReconnect = false;
            // if (!allowReconnect) {
            //     webSocket.setReconnectInterval(0);  // Отключаем авто-переподключение
            // }
            break;
        case WStype_TEXT:
            if (payload != nullptr && length > 0) {
                // Создаем строку из полезной части payload (без мусора после \0)
                message = String((const char*)payload).substring(0, length);
                Serial.print("📨 Message: ");
                Serial.println(message);
                displayWeight(scaleReading);
            } else {
                Serial.println("📨 Empty or invalid message received");
            }
            break;
        case WStype_BIN:
            // Serial.printf("📨 Binary data received (%d bytes)\n", length);
            break;
    }
}


void setColor(uint8_t r, uint8_t g, uint8_t b) {
    analogWrite(RED_PIN, r);
    analogWrite(GREEN_PIN, g);
    analogWrite(BLUE_PIN, b);
}


bool detectSpeech(int16_t* samples, size_t sampleCount) {
    int32_t totalAmplitude = 0;
    for (size_t i = 0; i < sampleCount; i++) {
        totalAmplitude += abs(samples[i]);
    }
    int32_t averageAmplitude = totalAmplitude / sampleCount;
    bool isdetected = averageAmplitude > VOX_THRESHOLD;
    return isdetected;
}


void i2sTask(void* parameter) {
    int16_t samples[BUFFER_SIZE];
    size_t bytesRead;

    int8_t initDetect = 0;
    bool scaleActivating = false;
    bool firstVoiceFlag = false;

    while (true) {
        i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
        if (scaleChanged == true && scaleActivating == false){
            scaleActivating = true;
            // Не учитывать начальные шумы микрофона
            while(initDetect < 3){
                i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
                if (!detectSpeech(samples, BUFFER_SIZE)){
                    initDetect++;
                }
            }
            setColor(0,0,20);
            initDetect = 0;
            lastVoiceTime = millis();
        } 
        if (detectSpeech(samples, BUFFER_SIZE)) {
            firstVoiceFlag = true;
            lastVoiceTime = millis();
            isSpeaking = true;
            Serial.println("Speaking");
        } else if (millis() - lastVoiceTime > VOX_SILENCE_TIME) {
            isSpeaking = false;
            Serial.println("Not speaking");
            if (firstVoiceFlag){
                firstVoiceFlag = false;
                scaleActivating = false;
                scaleChanged = false;
                i2s_stop(I2S_NUM_0);
                Serial.println("🛑 Остановлена запись.");
                setColor(0,0,0);
                continue;
            }
        } else if (abs(scaleReading) < 2){
            firstVoiceFlag = false;
                scaleActivating = false;
                scaleChanged = false;
                i2s_stop(I2S_NUM_0);
                Serial.println("Весы пустые.");
                setColor(0,0,0);
                continue;
        }

        if (isSpeaking) {
            setColor(0,20,0);
            int16_t* copy = (int16_t*)malloc(bytesRead);
            if (copy != NULL) {
                memcpy(copy, samples, bytesRead);
                xQueueSend(audioQueue, &copy, portMAX_DELAY);
            } else {
                Serial.println("❌ malloc failed");
            }
        } else {
            setColor(0,0,20);
        }
    }
}


void websocketTask(void* parameter) {
    int16_t* receivedSamples;
    bool eofSent = false;

    while (true) {
        if (webSocket.isConnected()) {
            if (xQueueReceive(audioQueue, &receivedSamples, pdMS_TO_TICKS(500)) == pdTRUE){
                webSocket.sendBIN((uint8_t*)receivedSamples, BUFFER_SIZE * sizeof(int16_t));
                free(receivedSamples);
                eofSent = false;
            } else {
                if (scaleChanged == false && !eofSent) {

                    String id = "{\"id\":\"" + deviceID + "\",\"weight\":" + String(savedScaleReading) + "}";
                    webSocket.sendTXT(id);
                    webSocket.sendTXT("{\"eof\" : 1}");
                    
                    delay(1000);
                    eofSent = true;
                    // webSocket.disconnect();
                    // webSocket.setReconnectInterval(0);
                    Serial.println("🛑 Отключение от WebSocket сервера.");
                } 
            } 
        }
    }
}


// SETUP
void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
    setColor(20,0,0);
    setupWiFi();
    webSocket.onEvent(webSocketEvent);
    setupI2S();

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }

    display.setFont(&TimesNRCyr6pt8b);
    display.clearDisplay();
    display.setTextColor(WHITE);
    
    audioQueue = xQueueCreate(100, sizeof(int16_t*));

    Serial.println("Initializing the scale");
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(CALIBRATION_FACTOR);   // this value is obtained by calibrating the scale with known weights; see the README for details
    scale.tare();               // reset the scale to 0

    // Запуск задачи для чтения с I2S (ядро 1)
    xTaskCreatePinnedToCore(
        i2sTask,             // функция задачи
        "I2S Task",          // имя
        8192,                // стек
        NULL,                // аргумент (не используется)
        1,                   // приоритет
        NULL,                // хендл (не используется)
        0                    // ядро (1 = APP)
    );

    // Запуск задачи для отправки по WebSocket (ядро 0)
    xTaskCreatePinnedToCore(
        websocketTask,       // функция задачи
        "WebSocket Task",    // имя
        8192,                // стек
        NULL,                // аргумент (не используется)
        1,                   // приоритет
        NULL,                // хендл (не используется)
        1                    // ядро (0 = PRO)
    );

    Serial.println("setup выполнен");
    setColor(0,0,0);
}


void loop() {
    static bool lastButtonState = HIGH;
    buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW && lastButtonState == HIGH) {
        Serial.print("tare...");
        scale.tare();
    }

    if (allowReconnect){
        webSocket.loop();
    }

    if (scale.wait_ready_timeout(200)) {
        scaleReading = round(scale.get_units());
        // убрать начальные шаги в граммах
        if (abs(scaleReading) < 2){
            scaleReading = 0;
            display.ssd1306_command(SSD1306_DISPLAYOFF);
        } else {
            display.ssd1306_command(SSD1306_DISPLAYON);
        }
        if (abs(scaleReading - scaleLastReading) > 1){
          displayWeight(scaleReading); 
          lastScaleTime = millis();
        }
        if (scaleLastReading < 2){
            savedScaleReading = 0;
        }
        scaleLastReading = scaleReading;
      }
      else {
        Serial.println("HX711 not found.");
      }

    if (scaleLastReading > 1 && scaleLastReading - savedScaleReading > 2 && millis() - lastScaleTime > SCALE_CHANGE_TIME) {
        savedScaleReading = scaleLastReading;
        allowReconnect = true;
        scaleChanged = true;
        // webSocket.setReconnectInterval(5000);
        if (!webSocket.isConnected()) {
            webSocket.begin(websocket_server, websocket_port);
            Serial.println("🎙 Подключение к WebSocket серверу..");
        }
        i2s_start(I2S_NUM_0);
        Serial.println("🎙 Начало записи...");
    } 
    lastButtonState = buttonState;
}
