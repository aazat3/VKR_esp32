// #include <WiFi.h>
// #include <WebSocketsClient.h>
// #include <driver/i2s.h>

// // Конфигурация I2S
// #define I2S_WS 15
// #define I2S_SD 32
// #define I2S_SCK 14
// #define I2S_PIN_NO_CHANGE -1

// // Настройки Wi-Fi
// const char* ssid = "POCO X3 NFC";
// const char* password = "12345678";

// // WebSocket сервер
// const char* wsServer = "192.168.232.243";
// const int wsPort = 5000;
// const char* wsPath = "/ws";

// WebSocketsClient webSocket;
// const int SAMPLE_RATE = 16000;
// const int BUFFER_SIZE = 1024; // Уменьшенный буфер для реального времени
// const int TARGET_BITRATE = SAMPLE_RATE * 2 * 1; // 44.1kHz, 16bit, mono (байт/сек)
// unsigned long lastSendTime = 0;
// int16_t i2sBuffer[BUFFER_SIZE];

// void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
//   switch(type) {
//     case WStype_DISCONNECTED:
//       Serial.println("WebSocket disconnected");
//       break;
//     case WStype_CONNECTED:
//       Serial.println("WebSocket connected");
//       break;
//     case WStype_TEXT:
//       Serial.printf("Received text: %s\n", payload);
//       break;
//   }
// }

// void setup() {
//   Serial.begin(115200);
  
//   // Инициализация I2S
//   i2s_config_t i2s_config = {
//     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
//     .sample_rate = SAMPLE_RATE,
//     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
//     .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
//     .communication_format = I2S_COMM_FORMAT_STAND_I2S,
//     .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
//     .dma_buf_count = 4,
//     .dma_buf_len = BUFFER_SIZE
//   };

//   i2s_pin_config_t pin_config = {
//     .bck_io_num = I2S_SCK,
//     .ws_io_num = I2S_WS,
//     .data_out_num = I2S_PIN_NO_CHANGE,
//     .data_in_num = I2S_SD
//   };

//   i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
//   i2s_set_pin(I2S_NUM_0, &pin_config);

//   // Подключение к Wi-Fi
//   WiFi.begin(ssid, password);
//   while(WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("\nConnected to WiFi");

//   // Инициализация WebSocket
//   webSocket.begin(wsServer, wsPort, wsPath);
//   webSocket.onEvent(webSocketEvent);
//   webSocket.setReconnectInterval(5000);
// }

// void loop() {
//   webSocket.loop();
  
//   if(webSocket.isConnected()) {
//     size_t bytesRead = 0;
//     i2s_read(I2S_NUM_0, &i2sBuffer, BUFFER_SIZE * sizeof(int16_t), &bytesRead, portMAX_DELAY);
    
//     if(bytesRead > 0) {
//       // Контроль скорости отправки
//       unsigned long now = millis();
//       if(now - lastSendTime >= (bytesRead * 1000) / TARGET_BITRATE) {
//         webSocket.sendBIN((uint8_t*)i2sBuffer, bytesRead);
//         lastSendTime = now;
//       }
//     }
//   }
//   delay(0); // Небольшая задержка для стабильности
// }





// #include <driver/i2s.h>
// #include <WiFi.h>
// #include <ArduinoWebsockets.h>

// #define I2S_SD 32
// #define I2S_WS 15
// #define I2S_SCK 14
// #define I2S_PORT I2S_NUM_0

// #define bufferCnt 10
// #define bufferLen 1024
// int16_t sBuffer[bufferLen];

// const char* ssid = "POCO X3 NFC";
// const char* password = "12345678";

// const char* websocket_server_host = "192.168.232.243";
// const uint16_t websocket_server_port = 5000;  // <WEBSOCKET_SERVER_PORT>

// using namespace websockets;
// WebsocketsClient client;
// bool isWebSocketConnected;

// void onEventsCallback(WebsocketsEvent event, String data) {
//   if (event == WebsocketsEvent::ConnectionOpened) {
//     Serial.println("Connnection Opened");
//     isWebSocketConnected = true;
//   } else if (event == WebsocketsEvent::ConnectionClosed) {
//     Serial.println("Connnection Closed");
//     isWebSocketConnected = false;
//   } else if (event == WebsocketsEvent::GotPing) {
//     Serial.println("Got a Ping!");
//   } else if (event == WebsocketsEvent::GotPong) {
//     Serial.println("Got a Pong!");
//   }
// }

// void i2s_install() {
//   // Set up I2S Processor configuration
//   const i2s_config_t i2s_config = {
//     .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
//     // .sample_rate = 44100,
//     .sample_rate = 16000,
//     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
//     .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
//     .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
//     .intr_alloc_flags = 0,
//     .dma_buf_count = bufferCnt,
//     .dma_buf_len = bufferLen,
//     .use_apll = false
//   };

//   i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
// }

// void i2s_setpin() {
//   // Set I2S pin configuration
//   const i2s_pin_config_t pin_config = {
//     .bck_io_num = I2S_SCK,
//     .ws_io_num = I2S_WS,
//     .data_out_num = -1,
//     .data_in_num = I2S_SD
//   };

//   i2s_set_pin(I2S_PORT, &pin_config);
// }

// void connectWiFi() {
//   WiFi.begin(ssid, password);

//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("");
//   Serial.println("WiFi connected");
// }

// void connectWSServer() {
//   client.onEvent(onEventsCallback);
//   while (!client.connect(websocket_server_host, websocket_server_port, "/ws")) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("Websocket Connected!");
// }


// void micTask(void* parameter) {

//   i2s_install();
//   i2s_setpin();
//   i2s_start(I2S_PORT);

//   size_t bytesIn = 0;
//   while (1) {
//     esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);
//     if (result == ESP_OK && isWebSocketConnected) {
//       client.sendBinary((const char*)sBuffer, bytesIn);
//     }
//   }
// }

// void setup() {
//   Serial.begin(115200);

//   connectWiFi();
//   connectWSServer();
//   xTaskCreatePinnedToCore(micTask, "micTask", 10000, NULL, 1, NULL, 1);
// }

// void loop() {
// }




// #include <WiFi.h>
// #include <WebSocketsClient.h>
// #include <driver/i2s.h>

// // Конфигурация пинов
// #define I2S_WS  15   // Word Select (L/R)
// #define I2S_SD  32   // Serial Data In
// #define I2S_SCK 14   // Serial Clock

// // Настройки Wi-Fi
// const char* ssid = "POCO X3 NFC";
// const char* password = "12345678";

// // Настройки WebSocket сервера
// const char* websocketServer = "192.168.232.243"; // IP сервера
// const int websocketPort = 5000; // Порт FastAPI
// const char* websocketPath = "/ws"; // Путь WebSocket

// // Настройки аудио
// const int SAMPLE_RATE = 16000;
// const int BUFFER_SIZE = 1024;
// const int CHANNELS = 1;
// const int BITS_PER_SAMPLE = 16;

// WebSocketsClient webSocket;

// // Конфигурация I2S
// i2s_pin_config_t pin_config = {
//     .bck_io_num = I2S_SCK,
//     .ws_io_num = I2S_WS,
//     .data_out_num = I2S_PIN_NO_CHANGE,
//     .data_in_num = I2S_SD
// };

// void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
//   switch(type) {
//       case WStype_DISCONNECTED:
//           Serial.printf("[WSc] Disconnected!\n");
//           break;
//       case WStype_CONNECTED:
//           Serial.printf("[WSc] Connected to url: %s\n", payload);
//           break;
//       case WStype_TEXT:
//           Serial.printf("[WSc] Received text: %s\n", payload);
//           break;
//       case WStype_BIN:
//           case WStype_ERROR:
//           case WStype_FRAGMENT_TEXT_START:
//           case WStype_FRAGMENT_BIN_START:
//           case WStype_FRAGMENT:
//           case WStype_FRAGMENT_FIN:
//           break;
//   }
// }

// void setup() {
//     Serial.begin(115200);
    
//     // Подключение к Wi-Fi
//     WiFi.begin(ssid, password);
//     while (WiFi.status() != WL_CONNECTED) {
//         delay(500);
//         Serial.print(".");
//     }
//     Serial.println("WiFi connected");
    
//     // Инициализация I2S
//     i2s_config_t i2s_config = {
//         .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
//         .sample_rate = SAMPLE_RATE,
//         .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
//         .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
//         .communication_format = I2S_COMM_FORMAT_I2S,
//         .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
//         .dma_buf_count = 8,
//         .dma_buf_len = BUFFER_SIZE,
//         .use_apll = false,
//         .tx_desc_auto_clear = false,
//         .fixed_mclk = 0
//     };
    
//     i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
//     i2s_set_pin(I2S_NUM_0, &pin_config);
    
//     // Подключение к WebSocket серверу
//     webSocket.begin(websocketServer, websocketPort, websocketPath);
//     webSocket.onEvent(webSocketEvent);
//     webSocket.setReconnectInterval(5000);
// }

// void loop() {
//     webSocket.loop();
    
//     static int16_t i2s_buffer[BUFFER_SIZE];
//     size_t bytes_read;
    
//     // Чтение данных с I2S
//     i2s_read(I2S_NUM_0, &i2s_buffer, BUFFER_SIZE * sizeof(int16_t), &bytes_read, portMAX_DELAY);
    
//     if (bytes_read > 0 && webSocket.isConnected()) {
//         // Отправка аудиоданных через WebSocket
//         webSocket.sendBIN((uint8_t*)i2s_buffer, bytes_read);
//     }
// }



// #include <WiFi.h>
// #include <PubSubClient.h>
// #include <Wire.h>
// #include <driver/i2s.h>

// const char* ssid = "POCO X3 NFC";              // WiFi SSID
// const char* password = "12345678";     // WiFi пароль
// const char* mqtt_server = "aazatserver.ru";  // IP адрес MQTT брокера (ваш сервер)
// const char* mqtt_topic = "iot/32452345/audio";  // Топик MQTT для отправки аудио
// const int mqtt_port = 1883;
// const char* mqtt_user = "admin";
// const char* mqtt_password = "admin";

// WiFiClient espClient;
// PubSubClient client(espClient);

// #define I2S_WS  15   // Word Select (L/R)
// #define I2S_SD  32   // Serial Data In
// #define I2S_SCK 14   // Serial Clock

// #define SAMPLE_RATE 16000  // Частота дискретизации (16 кГц)
// #define BUFFER_SIZE 1024    // Размер пакета аудиоданных

// // Настройка пинов I2S
// i2s_config_t i2s_config = {
//     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
//     .sample_rate = SAMPLE_RATE,
//     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
//     .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
//     .communication_format = I2S_COMM_FORMAT_I2S,
//     .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
//     .dma_buf_count = 4,
//     .dma_buf_len = BUFFER_SIZE,
//     .use_apll = false
// };

// i2s_pin_config_t pin_config = {
//     .bck_io_num = I2S_SCK,
//     .ws_io_num = I2S_WS,
//     .data_out_num = I2S_PIN_NO_CHANGE,
//     .data_in_num = I2S_SD
// };

// // Подключение к WiFi
// void setupWiFi() {
//     WiFi.begin(ssid, password);
//     while (WiFi.status() != WL_CONNECTED) {
//         delay(500);
//         Serial.print(".");
//     }
//     Serial.println("\nConnected to WiFi");
// }

// // Подключение к MQTT
// void reconnectMQTT() {
//     while (!client.connected()) {
//         Serial.print("Connecting to MQTT...");
//         if (client.connect("ESP32-Audio", mqtt_user, mqtt_password)) {
//             Serial.println("Connected!");
//         } else {
//             Serial.print("Failed, rc=");
//             Serial.print(client.state());
//             Serial.println(" retrying in 5 seconds");
//             delay(5000);
//         }
//     }
// }

// // Отправка аудиоданных в MQTT
// void sendAudioData() {
//     int16_t samples[BUFFER_SIZE];
//     size_t bytesRead;

//     i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);

//     if (bytesRead > 0) {
//         client.publish(mqtt_topic, (uint8_t*)samples, bytesRead, 2);
//         // client.publish(mqtt_topic, "sdsd");
//     }
// }

// void setup() {
//     Serial.begin(115200);
    
//     setupWiFi();
//     client.setBufferSize(2200);
//     client.setServer(mqtt_server, 1883);
    
//     i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
//     i2s_set_pin(I2S_NUM_0, &pin_config);
// }

// void loop() {
//     if (!client.connected()) {
//         reconnectMQTT();
//     }
//     client.loop();
//     sendAudioData();
// }




// #include <WiFi.h>
// #include <driver/i2s.h>

// #define I2S_WS  15   // L/R Word Select
// #define I2S_SD  32   // Serial Data
// #define I2S_SCK 14   // Serial Clock
// #define SAMPLE_RATE 16000
// #define BUFFER_SIZE 1024  // Размер буфера передачи

// const char* ssid = "POCO X3 NFC";
// const char* password = "12345678";
// const char* server_ip = "192.168.232.243";  // IP сервера
// const uint16_t server_port = 5000;

// WiFiClient client;

// // Настройка I2S
// void setupI2S() {
//     i2s_config_t i2s_config = {
//         .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
//         .sample_rate = SAMPLE_RATE,
//         .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
//         .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
//         .communication_format = I2S_COMM_FORMAT_I2S,
//         .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
//         .dma_buf_count = 8,
//         .dma_buf_len = BUFFER_SIZE,
//         .use_apll = false
//     };

//     i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
//     i2s_pin_config_t pin_config = {
//         .bck_io_num = I2S_SCK,
//         .ws_io_num = I2S_WS,
//         .data_out_num = I2S_PIN_NO_CHANGE,
//         .data_in_num = I2S_SD
//     };
//     i2s_set_pin(I2S_NUM_0, &pin_config);
// }

// // Подключение к WiFi
// void setupWiFi() {
//     WiFi.begin(ssid, password);
//     while (WiFi.status() != WL_CONNECTED) {
//         delay(500);
//         Serial.print(".");
//     }
//     Serial.println("\nConnected to WiFi");
// }

// // Подключение к TCP-серверу
// bool connectToServer() {
//     if (client.connect(server_ip, server_port)) {
//         Serial.println("Connected to TCP server");
//         return true;
//     } else {
//         Serial.println("Connection to server failed");
//         return false;
//     }
// }

// // Отправка аудиоданных
// void sendAudioData() {
//     int16_t samples[BUFFER_SIZE];
//     size_t bytesRead;

//     i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);

//     if (bytesRead > 0) {
//         if (client.connected()) {
//             client.write((uint8_t*)samples, bytesRead);
//         } else {
//             Serial.println("Disconnected! Reconnecting...");
//             connectToServer();
//         }
//     }
// }

// void setup() {
//     Serial.begin(115200);
//     setupWiFi();
//     setupI2S();
//     connectToServer();
// }

// void loop() {
//     sendAudioData();
// }



#include <WiFi.h>
#include <WebSocketsClient.h>
#include <driver/i2s.h>

#define I2S_WS  15   // L/R Word Select
#define I2S_SD  32   // Serial Data
#define I2S_SCK 14   // Serial Clock
#define SAMPLE_RATE 16000
#define BUFFER_SIZE 256  // Размер буфера для передачи

#define BUTTON_PIN 33       // Пин кнопки

const char* ssid = "POCO X3 NFC";
const char* password = "12345678";
const char* websocket_server = "aazatserver.ru";  
const uint16_t websocket_port = 5000;
const char* websocket_path = "/";

WebSocketsClient webSocket;
QueueHandle_t audioQueue;


// Настройка I2S
void setupI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 100,
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
    if (type == WStype_CONNECTED) {
        Serial.println("Connected to WebSocket server");
    } else if (type == WStype_DISCONNECTED) {
        Serial.println("Disconnected from WebSocket server");
    }
}

void setupWebSocket() {
    webSocket.begin(websocket_server, websocket_port);
    webSocket.onEvent(webSocketEvent);
}

void i2sTask(void* parameter) {
    int16_t samples[BUFFER_SIZE];
    size_t bytesRead;

    while (true) {
        // if (isStreaming) {
            i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
            // if (bytesRead > 0) {
                // Копируем в heap и отправляем в очередь
                int16_t* copy = (int16_t*)malloc(bytesRead);
                // if (copy) {
                    memcpy(copy, samples, bytesRead);
                    xQueueSend(audioQueue, &copy, portMAX_DELAY);
                // }
            // }
        // } else {
            // vTaskDelay(10);  // чтобы не грузить ядро, если не стримим
        // }
    }
}

void websocketTask(void* parameter) {
    int16_t* receivedSamples;
    const TickType_t xDelay = pdMS_TO_TICKS(15);
    while (true) {
        // Блокируется, пока нет данных
        if (webSocket.isConnected()) {
            xQueueReceive(audioQueue, &receivedSamples, portMAX_DELAY) == pdTRUE;
            webSocket.sendBIN((uint8_t*)receivedSamples, BUFFER_SIZE * sizeof(int16_t));
            free(receivedSamples);
            
            
        } 
        vTaskDelay(xDelay);
    }
}

// SETUP
void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    setupWiFi();
    setupI2S();
    setupWebSocket();

    // Создаём очередь для аудиоданных
    audioQueue = xQueueCreate(10, sizeof(int16_t*));

    // Запуск задачи для чтения с I2S (ядро 1)
    xTaskCreatePinnedToCore(
        i2sTask,             // функция задачи
        "I2S Task",          // имя
        8192,                // стек
        NULL,                // аргумент (не используется)
        1,                   // приоритет
        NULL,                // хендл (не используется)
        1                    // ядро (1 = APP)
    );

    // Запуск задачи для отправки по WebSocket (ядро 0)
    xTaskCreatePinnedToCore(
        websocketTask,       // функция задачи
        "WebSocket Task",    // имя
        8192,                // стек
        NULL,                // аргумент (не используется)
        1,                   // приоритет
        NULL,                // хендл (не используется)
        0                    // ядро (0 = PRO)
    );
}

// loop
void loop() {
    webSocket.loop();

    // Проверка кнопки
    static bool lastButtonState = HIGH;
    bool buttonState = digitalRead(BUTTON_PIN);

    if (buttonState == LOW && lastButtonState == HIGH) {
        // isStreaming = true;
        i2s_start(I2S_NUM_0);
        Serial.println("🎙 Начало записи...");
    } else if (buttonState == HIGH && lastButtonState == LOW) {
        // isStreaming = false;
        i2s_stop(I2S_NUM_0);
        Serial.println("🛑 Остановлена запись.");
    }

    lastButtonState = buttonState;

    // delay(10);
}




// #include <WiFi.h>
// #include <WebSocketsClient.h>
// #include <driver/i2s.h>

// #define I2S_WS  15   // L/R Word Select
// #define I2S_SD  32   // Serial Data
// #define I2S_SCK 14   // Serial Clock
// #define SAMPLE_RATE 16000
// #define BUFFER_SIZE 256  // Размер буфера для передачи
// #define BUTTON_PIN 33       // Пин кнопки

// const char* ssid = "POCO X3 NFC";
// const char* password = "12345678";
// const char* websocket_server = "aazatserver.ru";  // IP сервера
// const uint16_t websocket_port = 5000;
// const char* websocket_path = "/";

// WebSocketsClient webSocket;
// TaskHandle_t audioTaskHandle;
// volatile bool isStreaming = false;  // Флаг стриминга


// // Настройка I2S
// void setupI2S() {
//     i2s_config_t i2s_config = {
//         .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
//         .sample_rate = SAMPLE_RATE,
//         .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
//         .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
//         .communication_format = I2S_COMM_FORMAT_I2S,
//         .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
//         .dma_buf_count = 80,
//         .dma_buf_len = BUFFER_SIZE,
//         .use_apll = false
//     };

//     i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
//     i2s_pin_config_t pin_config = {
//         .bck_io_num = I2S_SCK,
//         .ws_io_num = I2S_WS,
//         .data_out_num = I2S_PIN_NO_CHANGE,
//         .data_in_num = I2S_SD
//     };
//     i2s_set_pin(I2S_NUM_0, &pin_config);
// }

// void setupWiFi() {
//     WiFi.begin(ssid, password);
//     while (WiFi.status() != WL_CONNECTED) {
//         delay(500);
//         Serial.print(".");
//     }
//     Serial.println("\nConnected to WiFi");
// }

// void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
//     if (type == WStype_CONNECTED) {
//         Serial.println("Connected to WebSocket server");
//     } else if (type == WStype_DISCONNECTED) {
//         Serial.println("Disconnected from WebSocket server");
//     }
// }

// void setupWebSocket() {
//     webSocket.begin(websocket_server, websocket_port);
//     webSocket.onEvent(webSocketEvent);
// }

// void audioTask(void* parameter) {
//     int16_t samples[BUFFER_SIZE];
//     size_t bytesRead;
//     const TickType_t xDelay = pdMS_TO_TICKS(20); // FreeRTOS-совместимая задержка

//     while (true) {
//         if(isStreaming){
//             esp_err_t result = i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
//             if (result == ESP_OK && webSocket.isConnected()) {
//                 webSocket.sendBIN((uint8_t*)samples, bytesRead);
//             }
//         }
//         vTaskDelay(xDelay); // Более точная задержка в FreeRTOS
//     }
// }

// void setup() {
//     Serial.begin(115200);
//     pinMode(BUTTON_PIN, INPUT_PULLUP);
//     setupWiFi();
//     setupI2S();
//     setupWebSocket();
//     xTaskCreatePinnedToCore(
//         audioTask,             // функция задачи
//         "Audio Task",          // имя
//         8192,                  // стек
//         NULL,                  // аргумент (не используется)
//         1,                     // приоритет
//         &audioTaskHandle,      // хендл (если нужно потом управлять)
//         1                      // ядро (1 = APP)
//     );
// }

// void loop() {
//     webSocket.loop();

//     // Проверка кнопки
//     static bool lastButtonState = HIGH;
//     bool buttonState = digitalRead(BUTTON_PIN);

//     if (buttonState == LOW && lastButtonState == HIGH) {
//         isStreaming = true;
//         Serial.println("🎙 Начало записи...");
//     } else if (buttonState == HIGH && lastButtonState == LOW) {
//         isStreaming = false;
//         Serial.println("🛑 Остановлена запись.");
//     }

//     lastButtonState = buttonState;
// }
