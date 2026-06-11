/*
 * VitalGuard IoT — ESP32-S3 + MAX30102
 *
 * Combina el "cerebro sensor" del proyecto VitalGuard (medición de BPM, SpO2
 * y onda PPG, con dashboard web local) con la conectividad a la nube del
 * proyecto Integrador (WiFi STA + MQTT hacia el broker EMQX + OTA desde S3).
 *
 * Reparto de núcleos (FreeRTOS):
 *   Core 0  -> sensorTask(): lee el MAX30102 y calcula BPM/SpO2/PPG.
 *   Core 1  -> loop():       red (WiFi, WebSocket del dashboard y MQTT).
 *
 * Las variables global* son el puente entre ambos núcleos.
 */
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "webpage.h"

#include "secrets.h"
#include "net_wifi.h"
#include "net_mqtt.h"
#include "net_ota.h"

// ==========================================
// HARDWARE CONFIG
// ==========================================
#define I2C_SDA        8
#define I2C_SCL        9
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

// ==========================================
// OBJETOS GLOBALES
// ==========================================
WebServer        server(80);
WebSocketsServer webSocket(81);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MAX30105         particleSensor;

SemaphoreHandle_t i2cMutex;

// ==========================================
// TELEMETRÍA COMPARTIDA (volátil = acceso desde ambos cores)
// ==========================================
volatile float globalBPM           = 0;
volatile float globalSpO2          = 0;
volatile bool  globalFingerPresent = false;
volatile bool  globalBeatDetected  = false;
volatile long  globalRawIR         = 0;
volatile float globalPPGValue      = 0;

// Buffer OLED scrolling
#define GRAPH_WIDTH  128
#define GRAPH_HEIGHT 44
#define GRAPH_Y_OFF  18
int oledGraph[GRAPH_WIDTH] = {0};

// Promedio móvil BPM
#define RATE_SIZE 5
byte   rateBuffer[RATE_SIZE] = {0};
byte   rateIdx               = 0;
float  avgBPM                = 0;

// Promedio móvil SpO2
#define SPO2_SIZE 5
float  spo2Buffer[SPO2_SIZE] = {0};
int    spo2Idx               = 0;
float  avgSpO2               = 0;

// Intervalo de publicación MQTT (segundos)
#define MQTT_PUBLISH_INTERVAL 2

// ==========================================
// PROTOTIPOS
// ==========================================
void handleRoot();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
void sensorTask(void* pvParameters);
void drawOled(float bpm, float spo2, bool finger, bool beat, long rawIR);
void oledSplash(const char* line1, const char* line2, const char* line3);

// ==========================================
// SETUP — Core 1
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n====== VitalGuard IoT BootUp ======");
    Serial.printf("Firmware: %s\n", FIRMWARE_VERSION);

    i2cMutex = xSemaphoreCreateMutex();

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    Serial.println("[I2C] Bus OK  SDA=8  SCL=9");

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("[OLED] ERROR: No encontrada");
    } else {
        Serial.println("[OLED] OK");
        oledSplash("VITALGUARD IoT", "Conectando WiFi...", "");
    }

    // --- WiFi en modo estación (conecta a una red real con internet) ---
    wifiConnect();

    // --- Hora por NTP (para el timestamp de las publicaciones) ---
    configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    // --- MQTT hacia el broker EMQX ---
    mqttSetup();

    // --- Servidor Web + WebSocket (dashboard local, ahora sobre la IP STA) ---
    server.on("/", handleRoot);
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("[HTTP] Servidor en puerto 80, WebSocket en 81");

    // Mostrar IP del dashboard en OLED
    char l3[24];
    snprintf(l3, sizeof(l3), "http://%s", wifiGetIp().c_str());
    oledSplash("VitalGuard IoT", wifiIsConnected() ? "WiFi OK" : "WiFi --", l3);
    delay(2000);

    // --- Tarea del sensor en Core 0 ---
    xTaskCreatePinnedToCore(sensorTask, "SensorTask", 16384, NULL, 2, NULL, 0);
    Serial.println("[TASK] Tarea de sensor creada en Core 0");
    Serial.println("===================================\n");
}

// ==========================================
// LOOP — Core 1: red, WebSocket y MQTT
// ==========================================
void loop() {
    server.handleClient();
    webSocket.loop();
    wifiEnsureConnected();
    mqttLoop();

    unsigned long now = millis();

    // --- WebSocket para el dashboard local (igual que VitalGuard) ---
    static unsigned long lastTx = 0;
    if (now - lastTx >= 40) {  // 25Hz
        lastTx = now;

        if (globalFingerPresent) {
            String msg = "{\"type\":\"ppg\",\"val\":" + String(globalPPGValue, 2) + "}";
            webSocket.broadcastTXT(msg);
        }

        static unsigned long lastStats = 0;
        if (now - lastStats >= 500) {
            lastStats = now;
            String msg = "{\"type\":\"stats\","
                         "\"bpm\":"    + String(globalBPM, 1)  + ","
                         "\"spo2\":"   + String(globalSpO2, 1) + ","
                         "\"finger\":" + String(globalFingerPresent ? "true" : "false") + ","
                         "\"beat\":"   + String(globalBeatDetected  ? "true" : "false") + "}";
            webSocket.broadcastTXT(msg);
        }
    }

    // --- Publicación a la nube por MQTT (cada MQTT_PUBLISH_INTERVAL s) ---
    static unsigned long lastPublish = 0;
    if (now - lastPublish >= (unsigned long)MQTT_PUBLISH_INTERVAL * 1000) {
        lastPublish = now;
        if (mqttIsConnected()) {
            mqttPublishBiometrics(globalBPM, globalSpO2, globalFingerPresent,
                                  globalBeatDetected, globalRawIR);
        }
        globalBeatDetected = false;  // se reinicia tras cada ciclo de envío
    }
}

// ==========================================
// HANDLERS WEB
// ==========================================
void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (type == WStype_CONNECTED) {
        Serial.printf("[WS] Cliente #%u conectado\n", num);
    }
}

// ==========================================
// TAREA SENSOR — Core 0  (lógica original de VitalGuard)
// ==========================================
void sensorTask(void* pvParameters) {
    Serial.println("[SENSOR] Inicializando MAX30102...");

    bool sensorOK = false;
    for (int attempt = 0; attempt < 5 && !sensorOK; attempt++) {
        if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {
            sensorOK = true;
        } else {
            Serial.printf("[SENSOR] Intento %d fallido. Reintentando...\n", attempt + 1);
            delay(500);
        }
    }

    if (!sensorOK) {
        Serial.println("[SENSOR] ERROR FATAL: MAX30102 no encontrado. Revisa SDA/SCL/VIN.");
        while (true) {
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                display.clearDisplay();
                display.setTextSize(1);
                display.setTextColor(SSD1306_WHITE);
                display.setCursor(5, 20);
                display.println("ERROR: MAX30102");
                display.setCursor(5, 35);
                display.println("Revisa conexion SDA");
                display.setCursor(5, 45);
                display.println("SCL y VIN al ESP32");
                display.display();
                xSemaphoreGive(i2cMutex);
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    particleSensor.setup(200, 4, 2, 100, 411, 16384);
    particleSensor.setPulseAmplitudeRed(200);
    particleSensor.setPulseAmplitudeIR(200);
    Serial.println("[SENSOR] MAX30102 listo. LED=200, ADC=16384, Rate=100Hz");

    const float DC_ALPHA = 0.95f;
    float dcIR  = 0;
    float dcRed = 0;
    bool  dcInit = false;

    float irAC_max = -1e9f, irAC_min = 1e9f;
    float rdAC_max = -1e9f, rdAC_min = 1e9f;
    unsigned long lastBeatMs = 0;
    unsigned long lastDisplayMs = 0;

    Serial.println("[SENSOR] Esperando dedo...");

    while (true) {
        particleSensor.check();

        while (particleSensor.available()) {
            long rawIR  = particleSensor.getFIFOIR();
            long rawRed = particleSensor.getFIFORed();
            globalRawIR = rawIR;

            // ---- DETECCIÓN DE DEDO ----
            if (rawIR < 20000) {
                globalFingerPresent = false;
                globalBPM   = 0;
                globalSpO2  = 0;
                dcInit = false;

                for (int i = 0; i < RATE_SIZE; i++) rateBuffer[i] = 0;
                for (int i = 0; i < SPO2_SIZE;  i++) spo2Buffer[i] = 0;
                avgBPM = 0; avgSpO2 = 0; rateIdx = 0; spo2Idx = 0;
                irAC_max = -1e9f; irAC_min = 1e9f;
                rdAC_max = -1e9f; rdAC_min = 1e9f;

                particleSensor.nextSample();
                continue;
            }

            globalFingerPresent = true;

            if (!dcInit) {
                dcIR  = (float)rawIR;
                dcRed = (float)rawRed;
                dcInit = true;
                Serial.printf("[SENSOR] Dedo detectado! IR=%ld  Red=%ld\n", rawIR, rawRed);
            }

            // ---- FILTRO DC (IIR High-Pass) ----
            dcIR  = DC_ALPHA * dcIR  + (1.0f - DC_ALPHA) * (float)rawIR;
            dcRed = DC_ALPHA * dcRed + (1.0f - DC_ALPHA) * (float)rawRed;
            float acIR  = (float)rawIR  - dcIR;
            float acRed = (float)rawRed - dcRed;

            globalPPGValue = acIR;

            if (acIR  > irAC_max) irAC_max = acIR;
            if (acIR  < irAC_min) irAC_min = acIR;
            if (acRed > rdAC_max) rdAC_max = acRed;
            if (acRed < rdAC_min) rdAC_min = acRed;

            // ---- DETECCIÓN DE LATIDO (algoritmo SparkFun) ----
            if (checkForBeat(rawIR)) {
                unsigned long nowMs = millis();
                long delta = nowMs - lastBeatMs;
                lastBeatMs = nowMs;

                if (delta > 300 && delta < 1700) {
                    int bpm = (int)(60000L / delta);

                    rateBuffer[rateIdx++] = (byte)bpm;
                    if (rateIdx >= RATE_SIZE) rateIdx = 0;

                    long sum = 0;
                    for (int i = 0; i < RATE_SIZE; i++) sum += rateBuffer[i];
                    avgBPM = (float)sum / RATE_SIZE;
                    globalBPM = avgBPM;
                    globalBeatDetected = true;

                    // ---- SpO2 por ratio-of-ratios ----
                    float irRange = irAC_max - irAC_min;
                    float rdRange = rdAC_max - rdAC_min;

                    if (dcIR > 5000.0f && dcRed > 5000.0f && irRange > 5.0f && rdRange > 5.0f) {
                        float R    = (rdRange / dcRed) / (irRange / dcIR);
                        float spo2 = 104.0f - 17.0f * R;

                        if (spo2 >= 80.0f && spo2 <= 100.0f) {
                            spo2Buffer[spo2Idx++] = spo2;
                            if (spo2Idx >= SPO2_SIZE) spo2Idx = 0;

                            float s = 0;
                            for (int i = 0; i < SPO2_SIZE; i++) s += spo2Buffer[i];
                            avgSpO2 = s / SPO2_SIZE;
                            globalSpO2 = avgSpO2;
                        }
                    }

                    irAC_max = -1e9f; irAC_min = 1e9f;
                    rdAC_max = -1e9f; rdAC_min = 1e9f;
                }
            }

            particleSensor.nextSample();
        }

        // ---- ACTUALIZAR OLED A 20Hz ----
        unsigned long now = millis();
        if (now - lastDisplayMs >= 50) {
            lastDisplayMs = now;
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                drawOled(globalBPM, globalSpO2, globalFingerPresent, globalBeatDetected, globalRawIR);
                xSemaphoreGive(i2cMutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ==========================================
// RENDER OLED
// ==========================================
void drawOled(float bpm, float spo2, bool finger, bool beat, long rawIR) {
    display.clearDisplay();

    // Indicadores de conexión en la esquina superior derecha (W=WiFi, M=MQTT)
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(104, 56);
    display.print(wifiIsConnected() ? "W" : "-");
    display.print(mqttIsConnected() ? "M" : "-");

    if (!finger) {
        display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
        display.setCursor(20, 10);
        display.println("VITALGUARD IoT");
        display.drawFastHLine(4, 20, 120, SSD1306_WHITE);
        display.setCursor(10, 28);
        display.println("Coloque el dedo");
        display.setCursor(10, 40);
        display.println("en el sensor...");
        display.setCursor(2, 54);
        display.printf("IR:%ld", rawIR);
        display.display();
        return;
    }

    // ---- Header: BPM y SpO2 ----
    display.setCursor(0, 2);
    display.print(beat ? "<3" : "o ");
    display.printf(" %3d BPM", (int)bpm);

    display.setCursor(72, 2);
    display.printf("SpO2 %3d%%", (int)spo2);

    display.drawFastHLine(0, 13, 128, SSD1306_WHITE);

    // ---- Gráfico scrolling PPG ----
    static float ppgMin = -500, ppgMax = 500;
    float val = globalPPGValue;

    if (val < ppgMin) ppgMin = val;
    if (val > ppgMax) ppgMax = val;
    ppgMin *= 0.999f;
    ppgMax *= 0.999f;

    float range = ppgMax - ppgMin;
    if (range < 1.0f) range = 1.0f;

    int y = GRAPH_Y_OFF + GRAPH_HEIGHT - (int)(((val - ppgMin) / range) * GRAPH_HEIGHT);
    y = constrain(y, GRAPH_Y_OFF, GRAPH_Y_OFF + GRAPH_HEIGHT - 1);

    memmove(oledGraph, oledGraph + 1, (GRAPH_WIDTH - 1) * sizeof(int));
    oledGraph[GRAPH_WIDTH - 1] = y;

    for (int i = 1; i < GRAPH_WIDTH; i++) {
        display.drawLine(i - 1, oledGraph[i - 1], i, oledGraph[i], SSD1306_WHITE);
    }

    display.display();
}

// ==========================================
// SPLASH SCREEN OLED
// ==========================================
void oledSplash(const char* line1, const char* line2, const char* line3) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(10, 12);
    display.println(line1);
    display.drawFastHLine(4, 22, 120, SSD1306_WHITE);
    display.setCursor(5, 30);
    display.println(line2);
    display.setCursor(5, 44);
    display.println(line3);
    display.display();
}
