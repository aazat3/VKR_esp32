#include <WiFi.h>
#include <nvs_flash.h>
#include <esp_wifi_types.h>
#include <WebSocketsClient.h>
#include <Preferences.h>
#include <driver/i2s.h>
#include "HX711.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "FontsRus/TimesNRCyr6.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Конфигурация пинов
#define I2S_WS 15
#define I2S_SD 32
#define I2S_SCK 14
#define BUTTON_PIN 33
#define LOADCELL_DOUT_PIN 16
#define LOADCELL_SCK_PIN 4
#define RED_PIN 25
#define GREEN_PIN 26
#define BLUE_PIN 27

#define SAMPLE_RATE 16000
#define BUFFER_SIZE 600
#define VOX_THRESHOLD 40
#define VOX_SILENCE_TIME 1000
#define CALIBRATION_FACTOR 290.46
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCALE_CHANGE_TIME 500

const char *websocket_server = "aazatserver.ru";
const uint16_t websocket_port = 5000;
const char *websocket_path = "/scale_server";

WebSocketsClient webSocket;
QueueHandle_t audioQueue;
HX711 scale;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Глобальные переменные
unsigned long lastVoiceTime = 0;
unsigned long lastDisplayTime = 0;
bool isSpeaking = false;
bool allowReconnect = false;
bool eofSent = false;
volatile bool buttonState = HIGH;
unsigned long buttonPressStart = 0;
int scaleReading;
int scaleLastReading = 0;
int savedScaleReading = 0;
int lastSendScale;

unsigned long lastScaleTime = 0;
volatile bool scaleChanged = false;
String message = "";
String deviceID = "";

// BLE Provisioning
BLEServer *pServer = nullptr;
BLEService *pProvService = nullptr;
BLECharacteristic *pCredCharacteristic = nullptr;
bool bleProvisioningActive = false;
bool bleConnected = false;
bool credentialsReceived = false;
String wifiSSID = "";
String wifiPassword = "";
Preferences preferences;
// Прототипы функций
void setColor(uint8_t r, uint8_t g, uint8_t b);
void setupI2S();
void setupWiFi();
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);
bool detectSpeech(int16_t *samples, size_t sampleCount);
void displayWeight(int weight);
void eraseWiFiSettings();
bool isWiFiProvisioned();
void saveWiFiCredentials(const String &ssid, const String &password);
void loadWiFiCredentials(String &ssid, String &password);
void startBLEProvisioning();
void stopBLEProvisioning();

// BLE Callbacks
class ServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        bleConnected = true;
        Serial.println("BLE Client Connected");
        setColor(0, 0, 20); // Синий при подключении
    };

    void onDisconnect(BLEServer *pServer)
    {
        bleConnected = false;
        Serial.println("BLE Client Disconnected");
        if (!credentialsReceived)
        {
            setColor(20, 0, 0); // Красный при отключении до получения данных
        }
    }
};

class CredentialsCallback : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0)
        {
            String data = String(value.c_str());
            int separatorIndex = data.indexOf(':');
            if (separatorIndex != -1)
            {
                wifiSSID = data.substring(0, separatorIndex);
                wifiPassword = data.substring(separatorIndex + 1);

                Serial.println("Received WiFi Credentials via BLE:");
                Serial.println("SSID: " + wifiSSID);
                Serial.println("Password: " + wifiPassword);

                credentialsReceived = true;
                setColor(0, 20, 0); // Зеленый при успешном получении
            }
        }
    }
};

void setColor(uint8_t r, uint8_t g, uint8_t b)
{
    analogWrite(RED_PIN, r);
    analogWrite(GREEN_PIN, g);
    analogWrite(BLUE_PIN, b);
}

void setupI2S()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 10,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = false};

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_stop(I2S_NUM_0);
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD};
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

void setupWiFi()
{
    String ssid, password;
    loadWiFiCredentials(ssid, password);

    if (ssid.length() > 0)
    {
        Serial.println("Connecting to saved WiFi: " + ssid);
        WiFi.begin(ssid.c_str(), password.c_str());

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20)
        {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("\nConnected to WiFi");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            deviceID = WiFi.macAddress();
            deviceID.replace(":", "");
            setColor(0, 0, 0);
            return;
        }
    }

    Serial.println("Failed to connect to saved WiFi. Starting provisioning...");
    startBLEProvisioning();
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_CONNECTED:
        Serial.println("✅ Connected to WebSocket server");
        break;
    case WStype_DISCONNECTED:
        Serial.println("❌ Disconnected");
        allowReconnect = false;
        break;
    case WStype_TEXT:
        if (payload != nullptr && length > 0)
        {
            message = String((const char *)payload).substring(0, length);
            Serial.print("📨 Message: ");
            Serial.println(message);
            displayWeight(scaleReading);
            lastDisplayTime = millis();
            setColor(0, 0, 0);
        }
        break;
    }
}

bool detectSpeech(int16_t *samples, size_t sampleCount)
{
    int32_t totalAmplitude = 0;
    for (size_t i = 0; i < sampleCount; i++)
    {
        totalAmplitude += abs(samples[i]);
    }
    return (totalAmplitude / sampleCount) > VOX_THRESHOLD;
}

void displayWeight(int weight)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 50);
    display.println("Вес:");
    display.display();
    display.setCursor(35, 50);
    display.setTextSize(3);
    display.print(weight);
    display.print(" g");
    display.display();

    display.setTextSize(2);
    display.setCursor(0, 14);
    display.println(message);
    display.display();
}

void eraseWiFiSettings()
{
    Serial.println("Erasing Wi-Fi credentials...");
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    WiFi.disconnect(true);
    delay(1000);
    ESP.restart();
}

bool isWiFiProvisioned()
{
    preferences.begin("wifi", true);
    bool provisioned = preferences.getString("ssid", "").length() > 0;
    preferences.end();
    return provisioned;
}

void saveWiFiCredentials(const String &ssid, const String &password)
{
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    Serial.println("WiFi credentials saved");
}

void loadWiFiCredentials(String &ssid, String &password)
{
    preferences.begin("wifi", true);
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");
    preferences.end();
}

void startBLEProvisioning()
{
    Serial.println("Starting BLE Provisioning...");
    bleProvisioningActive = true;
    setColor(20, 0, 0); // Красный - ожидание данных

    BLEDevice::init("ESP32-Provisioning");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    pProvService = pServer->createService(BLEUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b"));
    pCredCharacteristic = pProvService->createCharacteristic(
        BLEUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8"),
        BLECharacteristic::PROPERTY_WRITE);
    pCredCharacteristic->setCallbacks(new CredentialsCallback());
    pProvService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pProvService->getUUID());
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("BLE Advertising Started");
    Serial.println("Use BLE client to send credentials in format: SSID:Password");
}

void stopBLEProvisioning()
{
    if (bleProvisioningActive)
    {
        bleProvisioningActive = false;
        BLEDevice::stopAdvertising();
        BLEDevice::deinit();
        Serial.println("BLE Provisioning Stopped");
    }
}

void i2sTask(void *parameter)
{
    int16_t samples[BUFFER_SIZE];
    size_t bytesRead;

    int8_t initDetect = 0;
    bool scaleActivating = false;
    bool firstVoiceFlag = false;

    while (true)
    {
        i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);

        if (scaleChanged && !scaleActivating)
        {
            scaleActivating = true;
            while (initDetect < 3)
            {
                i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
                if (!detectSpeech(samples, BUFFER_SIZE))
                    initDetect++;
            }
            setColor(0, 0, 20);
            initDetect = 0;
            lastVoiceTime = millis();
        }

        if (detectSpeech(samples, BUFFER_SIZE))
        {
            firstVoiceFlag = true;
            lastVoiceTime = millis();
            isSpeaking = true;
        }
        else if (millis() - lastVoiceTime > VOX_SILENCE_TIME)
        {
            isSpeaking = false;
            if (firstVoiceFlag)
            {
                firstVoiceFlag = false;
                scaleActivating = false;
                scaleChanged = false;
                i2s_stop(I2S_NUM_0);
                setColor(0, 0, 0);
            }
        }
        else if (abs(scaleReading) < 2)
        {
            firstVoiceFlag = false;
            scaleActivating = false;
            scaleChanged = false;
            i2s_stop(I2S_NUM_0);
            setColor(0, 0, 0);
        }

        if (isSpeaking)
        {
            setColor(0, 20, 0);
            int16_t *copy = (int16_t *)malloc(bytesRead);
            if (copy)
            {
                memcpy(copy, samples, bytesRead);
                xQueueSend(audioQueue, &copy, portMAX_DELAY);
            }
        }
        else if (scaleActivating)
        {
            setColor(0, 0, 20);
        }
    }
}

void websocketTask(void *parameter)
{
    int16_t *receivedSamples;
    eofSent = false;

    while (true)
    {
        if (webSocket.isConnected())
        {
            if (xQueueReceive(audioQueue, &receivedSamples, pdMS_TO_TICKS(500)))
            {
                webSocket.sendBIN((uint8_t *)receivedSamples, BUFFER_SIZE * sizeof(int16_t));
                free(receivedSamples);
                eofSent = false;
            }
            else if (!scaleChanged && !eofSent)
            {
                String id = "{\"id\":\"" + deviceID + "\",\"weight\":" + String(savedScaleReading - lastSendScale) + "}";
                webSocket.sendTXT(id);
                webSocket.sendTXT("{\"eof\" : 1}");
                lastSendScale = savedScaleReading;
                delay(1000);
                eofSent = true;
            }
        }
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
    setColor(20, 0, 0);

    // Инициализация NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Инициализация дисплея
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("SSD1306 allocation failed"));
        while (true)
            ;
    }
    display.setFont(&TimesNRCyr6pt8b);
    display.clearDisplay();
    display.setTextColor(WHITE);
    displayWeight(0);

    // Проверка и подключение WiFi
    if (isWiFiProvisioned())
    {
        setupWiFi();
    }
    else
    {
        startBLEProvisioning();
    }

    // Инициализация весов
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(CALIBRATION_FACTOR);
    scale.tare();

    // Настройка I2S и WebSocket
    setupI2S();
    webSocket.onEvent(webSocketEvent);

    // Создание очереди и задач
    audioQueue = xQueueCreate(100, sizeof(int16_t *));
    xTaskCreatePinnedToCore(i2sTask, "I2S Task", 8192, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(websocketTask, "WebSocket Task", 8192, NULL, 1, NULL, 1);

    Serial.println("Setup complete");
}

void loop()
{
    static bool lastButtonState = HIGH;
    buttonState = digitalRead(BUTTON_PIN);

    // Обработка кнопки
    if (buttonState == LOW && lastButtonState == HIGH)
    {
        buttonPressStart = millis();
    }
    else if (buttonState == LOW && millis() - buttonPressStart >= 5000)
    {
        eraseWiFiSettings();
    }
    else if (buttonState == HIGH && lastButtonState == LOW)
    {
        if (millis() - buttonPressStart < 5000)
        {
            scale.tare();
            Serial.println("Tare done");
        }
    }
    lastButtonState = buttonState;

    // Обработка BLE Provisioning
    if (credentialsReceived)
    {
        Serial.println("Attempting WiFi connection with BLE credentials...");
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20)
        {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("\nWiFi connected!");
            saveWiFiCredentials(wifiSSID, wifiPassword);
            deviceID = WiFi.macAddress();
            deviceID.replace(":", "");
            stopBLEProvisioning();
            setColor(0, 0, 0);
            credentialsReceived = false;
        }
        else
        {
            Serial.println("\nFailed to connect. Restarting BLE provisioning.");
            credentialsReceived = false;
            setColor(20, 0, 0);
            delay(1000);
            startBLEProvisioning();
        }
    }

    // Обновление показаний весов
    if (scale.wait_ready_timeout(5))
    {
        scaleReading = round(scale.get_units(3));

        if (abs(scaleReading) < 2 && millis() - lastDisplayTime >= 10000)
        {
            scaleReading = 0;
            display.ssd1306_command(SSD1306_DISPLAYOFF);
            message = "";
        }
        else
        {
            display.ssd1306_command(SSD1306_DISPLAYON);
        }

        if (abs(scaleReading - scaleLastReading) > 1)
        {
            displayWeight(scaleReading);
            lastScaleTime = millis();
        }

        if (scaleLastReading < 2 && eofSent)
        {
            lastSendScale = 0;
            savedScaleReading = 0;
        }
        scaleLastReading = scaleReading;
    }

    // Активация передачи данных при изменении веса
    if (scaleLastReading > 1 && scaleLastReading - savedScaleReading > 2 &&
        millis() - lastScaleTime > SCALE_CHANGE_TIME)
    {
        savedScaleReading = scaleLastReading;

        allowReconnect = true;
        scaleChanged = true;

        if (!webSocket.isConnected())
        {
            webSocket.begin(websocket_server, websocket_port);
            Serial.println("Connecting to WebSocket server...");
        }
        i2s_start(I2S_NUM_0);
    }

    // Обработка WebSocket
    if (allowReconnect)
    {
        webSocket.loop();
    }
}