#include <WiFi.h>
#include <WebSocketsClient.h>
#include <driver/i2s.h>
#include "HX711.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
#define VOX_SILENCE_TIME 500  // Время (мс), через которое считаем, что говорящий замолчал
#define CALIBRATION_FACTOR 290.46 // Калибровка тензодатчика
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)


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
volatile bool lastButtonStateFori2sTask = HIGH;
int scaleReading;
int sclaeLastReading;


void displayWeight(int weight){
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 10);
    // Display static text
    display.println("Weight:");
    display.display();
    display.setCursor(0, 30);
    display.setTextSize(2);
    display.print(weight);
    display.print(" ");
    display.print("g");
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
        delay(500);
        Serial.print(".");
    }
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
            // if (!allowReconnect) {
            //     webSocket.setReconnectInterval(0);  // Отключаем авто-переподключение
            // }
            break;
        case WStype_TEXT:
            // Serial.printf("📨 Message: %s\n", payload);
            break;
        case WStype_BIN:
            // Serial.printf("📨 Binary data received (%d bytes)\n", length);
            break;
    }
}


// void setupWebSocket() {
//     webSocket.begin(websocket_server, websocket_port);
//     webSocket.onEvent(webSocketEvent);
// }


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
    // if (isdetected){
    //     Serial.println("Speaking");
    // } else{
    //     Serial.println("Not speaking");
    // }
    return isdetected;
}


void i2sTask(void* parameter) {
    int16_t samples[BUFFER_SIZE];
    size_t bytesRead;
    // bool lastButton = HIGH;
    int8_t initDetect = 0;

    while (true) {
        i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
        // if (millis() - lastVoiceTime > 2000){
        //     lastButtonStateFori2sTask = HIGH;
        // }
        if (lastButtonStateFori2sTask == HIGH and buttonState == LOW){
            lastButtonStateFori2sTask = LOW;
            // for (int i = 0; i < 40; i++){
            //     i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
            // }
            while(initDetect < 3){
                i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
                if (!detectSpeech(samples, BUFFER_SIZE)){
                    initDetect++;
                }
            }
            initDetect = 0;
            lastVoiceTime = millis();
        } 
        if (detectSpeech(samples, BUFFER_SIZE)) {
            lastVoiceTime = millis();
            isSpeaking = true;
            Serial.println("Speaking");
        } else if (millis() - lastVoiceTime > VOX_SILENCE_TIME) {
            isSpeaking = false;
            Serial.println("Not speaking");
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

    // const TickType_t xDelay = pdMS_TO_TICKS(1);
    while (true) {
        if (webSocket.isConnected()) {
            if (xQueueReceive(audioQueue, &receivedSamples, pdMS_TO_TICKS(500)) == pdTRUE){
                webSocket.sendBIN((uint8_t*)receivedSamples, BUFFER_SIZE * sizeof(int16_t));
                free(receivedSamples);
                eofSent = false;
            } else {
                if (buttonState == HIGH && !eofSent) {
                    webSocket.sendTXT("{\"eof\" : 1}");
                    allowReconnect = false;
                    delay(500);
                    webSocket.disconnect();
                    // webSocket.setReconnectInterval(0);
                    Serial.println("🛑 Отключение от WebSocket сервера.");
                } 
            } 
        // vTaskDelay(xDelay);
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
    setupWiFi();
    webSocket.onEvent(webSocketEvent);
    setupI2S();

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }
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
}


void loop() {
    static bool lastButtonState = HIGH;

    if (allowReconnect){
        webSocket.loop();
    }

    buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW && lastButtonState == HIGH) {
        allowReconnect = true;
        // webSocket.setReconnectInterval(5000);
        if (!webSocket.isConnected()) {
            webSocket.begin(websocket_server, websocket_port);
            Serial.println("🎙 Подключение к WebSocket серверу..");
        }
        i2s_start(I2S_NUM_0);
        Serial.println("🎙 Начало записи...");
        setColor(0,0,20);
    } else if (buttonState == HIGH && lastButtonState == LOW) {
        i2s_stop(I2S_NUM_0);
        lastButtonStateFori2sTask = HIGH;
        Serial.println("🛑 Остановлена запись.");
        setColor(0,0,0);
    }
    lastButtonState = buttonState;

    if (scale.wait_ready_timeout(200)) {
        scaleReading = round(scale.get_units());
        if (scaleReading != sclaeLastReading){
          displayWeight(scaleReading); 
        }
        sclaeLastReading = scaleReading;
      }
      else {
        Serial.println("HX711 not found.");
      }
}
