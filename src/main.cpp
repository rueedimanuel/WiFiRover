#include "esp_camera.h"
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <iostream>
#include <sstream>
#include "DFRobot_AXP313A.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_struct.h"
#include "soc/mcpwm_reg.h"
#include "html_page.h"

DFRobot_AXP313A axp;

/*--- Konstanten ---*/
#define MAX_RANG        (520)      // maximale Messreichweite des Moduls = 520 cm (etwas größer als der effektive Bereich)
#define ADC_SOLUTION    (4095.0)   // 12-Bit-ADC-Auflösung des ESP32 (0…4095)

#define M1_EN 12
#define M1_PN 13
#define M2_EN 14
#define M2_PN 21
#define M3_EN  9
#define M3_PN 10
#define M4_EN 47
#define M4_PN 11

#define UP    1
#define DOWN  2
#define LEFT  3
#define RIGHT 4
#define STOP  0

#define FORWARD   1
#define BACKWARD -1

/* Kamera-bezogene Konstanten */
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   45
#define SIOD_GPIO_NUM    1
#define SIOC_GPIO_NUM    2

#define Y9_GPIO_NUM     48
#define Y8_GPIO_NUM     46
#define Y7_GPIO_NUM      8
#define Y6_GPIO_NUM      7
#define Y5_GPIO_NUM      4
#define Y4_GPIO_NUM     41
#define Y3_GPIO_NUM     40
#define Y2_GPIO_NUM     39
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM   42
#define PCLK_GPIO_NUM    5

const char* ssid     = "WiFiRover";
const char* password = "12345678";

int   sensityPin = 15;            // Pin des Abstandssensors
float dist_t, sensity_t;
volatile uint8_t currentDir = STOP;
uint32_t lastDistTx = 0;

AsyncWebServer    server(80);
AsyncWebSocket    wsCamera("/Camera");
AsyncWebSocket    wsCarInput("/CarInput");
uint32_t          cameraClientId = 0;

uint8_t Speed = 30;

/* Lineare Rampe: 20 cm → 0 %, 100 cm → 100 % */
static uint8_t limitSpeedByDistance(float cm, uint8_t maxSpeed)
{
    if (cm <= 20)   return 0;                       // Stopp – Hindernis sehr nah
    if (cm >= 100)  return maxSpeed;                // frei – volle Geschwindigkeit
    return (uint8_t)((cm - 20) * maxSpeed / 80.0);  // stufenweise Reduktion
}

/*-------------------------------------------------------
  Motortreiber-Hilfsfunktionen
-------------------------------------------------------*/
void advance(uint8_t motorNumber, uint8_t speed)
{
    switch (motorNumber) {
        case 1:
            mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B);  // PH-Pin dauerhaft HIGH
            mcpwm_set_duty_type   (MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, MCPWM_DUTY_MODE_0);
            mcpwm_set_duty        (MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, speed); // EN-Pin gibt PWM mit Tastverhältnis „speed“ aus
            break;

        case 2:
            mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_B);  // PH-Pin dauerhaft HIGH
            mcpwm_set_duty_type   (MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_A, MCPWM_DUTY_MODE_0);
            mcpwm_set_duty        (MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_A, speed);
            break;

        case 3:
            mcpwm_set_signal_low (MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM_GEN_B);  // PH-Pin dauerhaft HIGH (invertiert)
            mcpwm_set_duty_type   (MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM_GEN_A, MCPWM_DUTY_MODE_0);
            mcpwm_set_duty        (MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM_GEN_A, speed);
            break;

        case 4:
            mcpwm_set_signal_low (MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_GEN_B);  // PH-Pin dauerhaft HIGH (invertiert)
            mcpwm_set_duty_type   (MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_GEN_A, MCPWM_DUTY_MODE_0);
            mcpwm_set_duty        (MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_GEN_A, speed);
            break;
    }
}

void retreat(uint8_t motorNumber, uint8_t speed)
{
    switch (motorNumber) {
        case 1:
            mcpwm_set_signal_low (MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B);  // PH-Pin dauerhaft LOW
            mcpwm_set_duty_type   (MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, MCPWM_DUTY_MODE_0);
            mcpwm_set_duty        (MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, speed);
            break;

        case 2:
            mcpwm_set_signal_low (MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_B);  // PH-Pin dauerhaft LOW
            mcpwm_set_duty_type   (MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_A, MCPWM_DUTY_MODE_0);
            mcpwm_set_duty        (MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_A, speed);
            break;

        case 3:
            mcpwm_set_signal_high(MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM_GEN_B);  // PH-Pin dauerhaft LOW (invertiert)
            mcpwm_set_duty_type   (MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM_GEN_A, MCPWM_DUTY_MODE_0);
            mcpwm_set_duty        (MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM_GEN_A, speed);
            break;

        case 4:
            mcpwm_set_signal_high(MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_GEN_B);  // PH-Pin dauerhaft LOW (invertiert)
            mcpwm_set_duty_type   (MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_GEN_A, MCPWM_DUTY_MODE_0);
            mcpwm_set_duty        (MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_GEN_A, speed);
            break;
    }
}

void breake(uint8_t motorNumber)
{
    switch (motorNumber) {
        case 1: mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A); break;
        case 2: mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_A); break;
        case 3: mcpwm_set_signal_low(MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM_GEN_A); break;
        case 4: mcpwm_set_signal_low(MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_GEN_A); break;
    }
}

void moveCar(uint8_t dir, uint8_t speed)
{
    switch (dir) {
        case UP:
            advance(1, speed); advance(2, speed); advance(3, speed); advance(4, speed);
            break;
        case DOWN:
            retreat(1, speed); retreat(2, speed); retreat(3, speed); retreat(4, speed);
            break;
        case LEFT:
            advance(3, speed); advance(4, speed); retreat(1, speed); retreat(2, speed);
            break;
        case RIGHT:
            advance(1, speed); advance(2, speed); retreat(3, speed); retreat(4, speed);
            break;
        default:  // STOP
            breake(1); breake(2); breake(3); breake(4);
    }
}

/*-------------------------------------------------------
  HTTP-/WebSocket-Callbacks
-------------------------------------------------------*/
void handleRoot(AsyncWebServerRequest* request)
{
    request->send_P(200, "text/html", htmlHomePage);
}

void handleNotFound(AsyncWebServerRequest* request)
{
    request->send(404, "text/plain", "File Not Found");
}

/* Ereignisse für Steuer-WebSocket */
void onCarInputWebSocketEvent(AsyncWebSocket* server,
                              AsyncWebSocketClient* client,
                              AwsEventType type,
                              void* arg, uint8_t* data, size_t len)
{
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n",
                          client->id(), client->remoteIP().toString().c_str());
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            moveCar(STOP, 0);
            // ledcWrite(PWMLightChannel, 0);
            // panServo.write(90);
            // tiltServo.write(90);
            break;

        case WS_EVT_DATA: {
            auto* info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                std::string myData((char*)data, len);
                std::istringstream ss(myData);
                std::string key, value;
                std::getline(ss, key, ',');
                std::getline(ss, value, ',');

                int valueInt = atoi(value.c_str());
                if (key == "MoveCar") {
                    uint8_t dir = valueInt;         // 1 = UP, …
                    uint8_t driveSpeed = Speed;     // Geschwindigkeit aus Slider

                    if (dir == UP)                  // Begrenzung nur bei Vorwärtsfahrt
                        driveSpeed = limitSpeedByDistance(dist_t, Speed);

                    moveCar(dir, driveSpeed);
                }
                else if (key == "Speed") {
                    Speed = valueInt;
                }
                /* else if (key == "Light") {
                       ledcWrite(PWMLightChannel, valueInt);
                   } … */
            }
            break;
        }

        default:
            break;
    }
}

/* Ereignisse für Kamera-WebSocket */
void onCameraWebSocketEvent(AsyncWebSocket* server,
                            AsyncWebSocketClient* client,
                            AwsEventType type,
                            void* arg, uint8_t* data, size_t len)
{
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n",
                          client->id(), client->remoteIP().toString().c_str());
            cameraClientId = client->id();
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            cameraClientId = 0;
            break;
        default:
            break;
    }
}

/*-------------------------------------------------------
  Kamera-Initialisierung
-------------------------------------------------------*/
void setupCamera()
{
    camera_config_t config;
    config.ledc_channel   = LEDC_CHANNEL_0;
    config.ledc_timer     = LEDC_TIMER_0;
    config.pin_d0         = Y2_GPIO_NUM;
    config.pin_d1         = Y3_GPIO_NUM;
    config.pin_d2         = Y4_GPIO_NUM;
    config.pin_d3         = Y5_GPIO_NUM;
    config.pin_d4         = Y6_GPIO_NUM;
    config.pin_d5         = Y7_GPIO_NUM;
    config.pin_d6         = Y8_GPIO_NUM;
    config.pin_d7         = Y9_GPIO_NUM;
    config.pin_xclk       = XCLK_GPIO_NUM;
    config.pin_pclk       = PCLK_GPIO_NUM;
    config.pin_vsync      = VSYNC_GPIO_NUM;
    config.pin_href       = HREF_GPIO_NUM;
    config.pin_sccb_sda   = SIOD_GPIO_NUM;
    config.pin_sccb_scl   = SIOC_GPIO_NUM;
    config.pin_pwdn       = PWDN_GPIO_NUM;
    config.pin_reset      = RESET_GPIO_NUM;
    config.xclk_freq_hz   = 20000000;
    config.frame_size     = FRAMESIZE_UXGA;
    config.pixel_format   = PIXFORMAT_JPEG;                     // für Streaming
    // config.pixel_format = PIXFORMAT_RGB565;                   // für Gesichts­erkennung
    config.grab_mode      = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location    = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality   = 8;
    config.fb_count       = 1;

    // Wenn PSRAM vorhanden ist → UXGA-Auflösung & höhere JPEG-Qualität
    //                            dank größerem vor­allo­kierten Frame-Puffer
    if (config.pixel_format == PIXFORMAT_JPEG) {
        if (psramFound()) {
            config.jpeg_quality = 10;
            config.fb_count     = 2;
            config.grab_mode    = CAMERA_GRAB_LATEST;
        } else {
            // Ohne PSRAM Bildgröße begrenzen
            config.frame_size  = FRAMESIZE_SVGA;
            config.fb_location = CAMERA_FB_IN_DRAM;
        }
    } else {
        // Beste Option für Gesichts­erkennung
        config.frame_size = FRAMESIZE_240X240;
    #if CONFIG_IDF_TARGET_ESP32S3
        config.fb_count   = 2;
    #endif
    }

    /* Kamera initialisieren */
    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Camera init failed");
        return;
    }

    if (psramFound()) {
        heap_caps_malloc_extmem_enable(20000);
        Serial.println("PSRAM initialized, large malloc → PSRAM");
    }
}

/* Einzelnes JPEG-Frame an Browser schicken */
void sendCameraPicture()
{
    if (cameraClientId == 0) return;

    // Schnelltest: Ist der Client aktiv und bereit?
    AsyncWebSocketClient* client = wsCamera.client(cameraClientId);
    if (!client || client->queueIsFull()) {
        // Bild verwerfen statt zu blockieren
        return;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return;

    wsCamera.binary(cameraClientId, fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

/*-------------------------------------------------------
  PWM-Treiber konfigurieren
-------------------------------------------------------*/
void mcpwm_init(void)
{
    // MCPWM-Konfiguration
    mcpwm_config_t pwm_config;
    pwm_config.frequency     = 500;
    pwm_config.cmpr_a        = 0;
    pwm_config.cmpr_b        = 0;
    pwm_config.counter_mode  = MCPWM_UP_COUNTER;
    pwm_config.duty_mode     = MCPWM_DUTY_MODE_0;

    // Kanal A → Geschwindigkeit, Kanal B → Richtung
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, M1_EN);  // Motor-1-GPIO
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, M1_PN);
    mcpwm_init      (MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);  // Einheit initialisieren & Takt binden

    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, M2_EN);  // Motor-2-GPIO
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1B, M2_PN);
    mcpwm_init      (MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);

    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM0A, M3_EN);  // Motor-3-GPIO
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM0B, M3_PN);
    mcpwm_init      (MCPWM_UNIT_1, MCPWM_TIMER_0, &pwm_config);

    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM1A, M4_EN);  // Motor-4-GPIO
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM1B, M4_PN);
    mcpwm_init      (MCPWM_UNIT_1, MCPWM_TIMER_1, &pwm_config);
}

/*-------------------------------------------------------
  Arduino-Setup
-------------------------------------------------------*/
void setup(void)
{
    Serial.begin(115200);
    while (axp.begin() != 0) {
        Serial.println("AXP init error");
        delay(1000);
    }

    axp.enableCameraPower(axp.eOV2640);  // Kamera-Versorgung einschalten
    mcpwm_init();

    WiFi.softAP(ssid, password);
    Serial.print("AP-IP-Adresse: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, handleRoot);
    server.onNotFound(handleNotFound);

    wsCamera.onEvent(onCameraWebSocketEvent);
    server.addHandler(&wsCamera);

    wsCarInput.onEvent(onCarInputWebSocketEvent);
    server.addHandler(&wsCarInput);

    server.begin();
    Serial.println("HTTP-Server gestartet");

    setupCamera();
}

/*-------------------------------------------------------
  Arduino-Loop
-------------------------------------------------------*/
void loop()
{
    sendCameraPicture();
    wsCamera.cleanupClients();

    /* Sensor auslesen */
    sensity_t = analogRead(sensityPin);
    dist_t    = sensity_t * MAX_RANG / ADC_SOLUTION;

    /* Distanz alle 200 ms senden (non-blocking) */
    static uint32_t lastSend = 0;
    if (millis() - lastSend > 200) {
        lastSend = millis();

        // Senden nur, wenn Steuer-WebSocket nicht überlastet ist
        AsyncWebSocketClient* ctrl = wsCarInput.client(0);  // 0 = alle Clients
        if (!ctrl || !ctrl->queueIsFull()) {
            char msg[24];
            snprintf(msg, sizeof(msg), "Distance,%d", (int)dist_t);
            wsCarInput.textAll(msg);
        }
    }

    wsCarInput.cleanupClients();
}
