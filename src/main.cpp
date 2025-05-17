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


DFRobot_AXP313A axp;

#define M1_EN 12
#define M1_PN 13
#define M2_EN 14
#define M2_PN 21
#define M3_EN 9
#define M3_PN 10
#define M4_EN 47
#define M4_PN 11


#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4
#define STOP 0

#define FORWARD 1
#define BACKWARD -1

//Camera related constants
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     45
#define SIOD_GPIO_NUM     1
#define SIOC_GPIO_NUM     2

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       46
#define Y7_GPIO_NUM       8
#define Y6_GPIO_NUM       7
#define Y5_GPIO_NUM       4
#define Y4_GPIO_NUM       41
#define Y3_GPIO_NUM       40
#define Y2_GPIO_NUM       39
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     42
#define PCLK_GPIO_NUM     5

const char* ssid     = "WiFiRover";
const char* password = "12345678";

AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsCarInput("/CarInput");
uint32_t cameraClientId = 0;

uint8_t Speed = 30;

const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html lang="de">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no" />
    <title>Remote Car Control</title>
    <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;700&display=swap" rel="stylesheet" />
    <!-- NippleJS: virtual joystick -->
    <script defer src="https://cdn.jsdelivr.net/npm/nipplejs@0.9.0/dist/nipplejs.min.js"></script>

    <style>
      :root {
        --primary: #ff3b30;
        --bg: #111;
        --surface: #1f1f1f;
        --text: #fafafa;
      }

      * {
        box-sizing: border-box;
        -webkit-tap-highlight-color: transparent;
      }

      html,
      body {
        margin: 0;
        padding: 0;
        font-family: "Roboto", sans-serif;
        background: var(--bg);
        color: var(--text);
        height: 100%;
        overscroll-behavior: contain; /* stop pull‑to‑refresh killing controls */
      }

      .wrapper {
        width: 92vw;
        max-width: 560px;
        margin: 0 auto;
        padding-top: 2vh;
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: 2.2rem;
      }

      .camera-feed {
        width: 100%;
        aspect-ratio: 16/9;
        max-height: 48vh;
        object-fit: cover;
        border-radius: 16px;
        box-shadow: 0 12px 24px rgba(0, 0, 0, 0.6);
        background: #000;
      }

      .controls {
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: 2.4rem;
        width: 100%;
      }

      /* ---------------- Joystick ---------------- */
      #joystickContainer {
        width: 180px;
        height: 180px;
        border-radius: 50%;
        background: radial-gradient(circle at 30% 30%, #2d2d2d 0%, #1a1a1a 70%);
        box-shadow: 0 6px 12px rgba(0, 0, 0, 0.4);
        position: relative;
        touch-action: none;
        user-select: none;
      }
      #joystickContainer.hidden { display: none; }

      #joystickContainer.active::after {
        content: "";
        position: absolute;
        inset: -8px;
        border-radius: 50%;
        border: 4px solid var(--primary);
        animation: pulse 1s ease-out infinite;
      }
      @keyframes pulse {
        0% { opacity: 0.8; transform: scale(1); }
        100% { opacity: 0; transform: scale(1.4); }
      }

      /* ---------------- Arrow Pad ---------------- */
      #arrowPad {
        display: grid;
        grid-template-columns: repeat(3, 72px);
        grid-template-rows: repeat(3, 72px);
        gap: 14px;
        touch-action: none;
        user-select: none;
      }
      #arrowPad.hidden { display: none; }

      .arrow-button {
        display: flex;
        justify-content: center;
        align-items: center;
        font-size: 30px;
        font-weight: 700;
        border-radius: 50%;
        background: var(--surface);
        box-shadow: 0 4px 8px rgba(0, 0, 0, 0.3);
        transition: transform 0.1s, box-shadow 0.1s;
      }
      .arrow-button:active {
        transform: translateY(2px);
        box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
      }

      /* ---------------- Slider ---------------- */
      .slider-container {
        width: 100%;
        max-width: 420px;
      }
      .slider-container label {
        display: block;
        margin-bottom: 8px;
        font-weight: 700;
        font-size: 1.1rem;
      }
      input[type="range"] {
        -webkit-appearance: none;
        width: 100%;
        height: 14px;
        border-radius: 7px;
        background: #444;
        outline: none;
      }
      input[type="range"]::-webkit-slider-thumb {
        -webkit-appearance: none;
        appearance: none;
        width: 32px;
        height: 32px;
        border-radius: 50%;
        background: var(--primary);
        cursor: pointer;
        box-shadow: 0 0 12px var(--primary);
        transition: transform 0.15s ease-out;
      }
      input[type="range"]::-webkit-slider-thumb:active { transform: scale(0.9); }
      input[type="range"]::-moz-range-thumb {
        width: 32px; height: 32px; border-radius: 50%; background: var(--primary); cursor: pointer; border: none; box-shadow: 0 0 12px var(--primary);
      }
    </style>
  </head>
  <body>
    <div class="wrapper">
      <img id="cameraImage" class="camera-feed" src="" alt="Live Camera" />

      <div class="controls">
        <div id="joystickContainer"></div>

        <div id="arrowPad" class="hidden">
          <div></div>
          <div class="arrow-button" data-dir="1">▲</div>
          <div></div>
          <div class="arrow-button" data-dir="3">◀</div>
          <div></div>
          <div class="arrow-button" data-dir="4">▶</div>
          <div></div>
          <div class="arrow-button" data-dir="2">▼</div>
          <div></div>
        </div>

        <div class="slider-container">
          <label for="Speed">Geschwindigkeit</label>
          <input type="range" min="0" max="100" value="50" id="Speed" oninput="sendButtonInput('Speed', this.value)" />
        </div>
      </div>
    </div>

    <script>
      /* ---------------- WebSockets ---------------- */
      const camURL = `ws://${location.hostname}/Camera`;
      const ctrlURL = `ws://${location.hostname}/CarInput`;
      let wsCam, wsCtrl;

      function initSockets() {
        wsCam = new WebSocket(camURL);
        wsCam.binaryType = 'blob';
        wsCam.onmessage = e => (document.getElementById('cameraImage').src = URL.createObjectURL(e.data));
        wsCam.onclose = () => setTimeout(initSockets, 2000);

        wsCtrl = new WebSocket(ctrlURL);
        wsCtrl.onopen = () => sendButtonInput('Speed', document.getElementById('Speed').value);
        wsCtrl.onclose = () => setTimeout(initSockets, 2000);
      }

      function sendButtonInput(key, val) {
        if (wsCtrl?.readyState === WebSocket.OPEN) wsCtrl.send(`${key},${val}`);
      }

      /* ---------------- Joystick OR Arrow Pad ---------------- */
      const joystickDiv = document.getElementById('joystickContainer');
      const arrowPad = document.getElementById('arrowPad');

      function enableArrowPad() {
        joystickDiv.classList.add('hidden');
        arrowPad.classList.remove('hidden');
      }

      function setupArrowPadEvents() {
        arrowPad.querySelectorAll('.arrow-button').forEach(btn => {
          btn.addEventListener('touchstart', () => sendButtonInput('MoveCar', btn.dataset.dir));
          btn.addEventListener('touchend',   () => sendButtonInput('MoveCar', '0'));
        });
      }

      function setupJoystick() {
        try {
          const manager = nipplejs.create({
            zone: joystickDiv,
            mode: 'static',
            color: 'var(--primary)',
            size: 160,
            position: { left: '50%', top: '50%' },
            restJoystick: true,
            multitouch: false,
          });

          let joystickMoved = false;
          const fallbackTimer = setTimeout(() => {
            if (!joystickMoved) {
              manager.destroy();
              enableArrowPad();
              setupArrowPadEvents();
            }
          }, 2500); // 2.5 s to prove working

          manager.on('start', () => joystickDiv.classList.add('active'));
          manager.on('end', () => {
            joystickDiv.classList.remove('active');
            sendButtonInput('MoveCar', '0');
          });

          manager.on('dir', (_, data) => {
            joystickMoved = true; // joystick works
            const angle = data.direction.angle;
            const map = { up: '1', down: '2', left: '3', right: '4' };
            sendButtonInput('MoveCar', map[angle] ?? '0');
          });
        } catch (e) {
          console.error('Joystick failed, using arrows', e);
          enableArrowPad();
          setupArrowPadEvents();
        }
      }

      window.addEventListener('DOMContentLoaded', () => {
        initSockets();
        setupJoystick();

        // Keyboard fallback (desktop testing)
        window.addEventListener('keydown', e => {
          const m = { ArrowUp: '1', ArrowDown: '2', ArrowLeft: '3', ArrowRight: '4' };
          if (m[e.key]) sendButtonInput('MoveCar', m[e.key]);
        });
        window.addEventListener('keyup', e => {
          if (['ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.key)) sendButtonInput('MoveCar', '0');
        });
      });
    </script>
  </body>
</html>
)HTMLHOMEPAGE";


void advance(uint8_t motorNumber,uint8_t speed)
{
  switch(motorNumber)
  {
    case 1:
      mcpwm_set_signal_high(MCPWM_UNIT_0,MCPWM_TIMER_0,MCPWM_GEN_B);//Give PH a consistently high level
      mcpwm_set_duty_type(MCPWM_UNIT_0,MCPWM_TIMER_0,MCPWM_GEN_A,MCPWM_DUTY_MODE_0);
      mcpwm_set_duty(MCPWM_UNIT_0,MCPWM_TIMER_0,MCPWM_GEN_A,speed);//The EN pin outputs a PWM wave with a duty cycle of "speed"
      break;

    case 2:
      mcpwm_set_signal_high(MCPWM_UNIT_0,MCPWM_TIMER_1,MCPWM_GEN_B);//Give PH a consistently high level
      mcpwm_set_duty_type(MCPWM_UNIT_0,MCPWM_TIMER_1,MCPWM_GEN_A,MCPWM_DUTY_MODE_0);
      mcpwm_set_duty(MCPWM_UNIT_0,MCPWM_TIMER_1,MCPWM_GEN_A,speed);//The EN pin outputs a PWM wave with a duty cycle of "speed"
      break;

    case 3:
      mcpwm_set_signal_low(MCPWM_UNIT_1,MCPWM_TIMER_0,MCPWM_GEN_B);//Give PH a consistently high level
      mcpwm_set_duty_type(MCPWM_UNIT_1,MCPWM_TIMER_0,MCPWM_GEN_A,MCPWM_DUTY_MODE_0);
      mcpwm_set_duty(MCPWM_UNIT_1,MCPWM_TIMER_0,MCPWM_GEN_A,speed);//The EN pin outputs a PWM wave with a duty cycle of "speed"
      break;

    case 4:
      mcpwm_set_signal_low(MCPWM_UNIT_1,MCPWM_TIMER_1,MCPWM_GEN_B);//Give PH a consistently high level
      mcpwm_set_duty_type(MCPWM_UNIT_1,MCPWM_TIMER_1,MCPWM_GEN_A,MCPWM_DUTY_MODE_0);
      mcpwm_set_duty(MCPWM_UNIT_1,MCPWM_TIMER_1,MCPWM_GEN_A,speed);//The EN pin outputs a PWM wave with a duty cycle of "speed"
      break;
  }
}

void retreat(uint8_t motorNumber,uint8_t speed)
{
  switch(motorNumber)
  {
    case 1:
      mcpwm_set_signal_low(MCPWM_UNIT_0,MCPWM_TIMER_0,MCPWM_GEN_B);//Give PH a constant low level
      mcpwm_set_duty_type(MCPWM_UNIT_0,MCPWM_TIMER_0,MCPWM_GEN_A,MCPWM_DUTY_MODE_0);
      mcpwm_set_duty(MCPWM_UNIT_0,MCPWM_TIMER_0,MCPWM_GEN_A,speed);//The EN pin outputs a PWM wave with a duty cycle of "speed"
      break;

    case 2:
      mcpwm_set_signal_low(MCPWM_UNIT_0,MCPWM_TIMER_1,MCPWM_GEN_B);//Give PH a constant low level
      mcpwm_set_duty_type(MCPWM_UNIT_0,MCPWM_TIMER_1,MCPWM_GEN_A,MCPWM_DUTY_MODE_0);
      mcpwm_set_duty(MCPWM_UNIT_0,MCPWM_TIMER_1,MCPWM_GEN_A,speed);//The EN pin outputs a PWM wave with a duty cycle of "speed"
      break;

    case 3:
      mcpwm_set_signal_high(MCPWM_UNIT_1,MCPWM_TIMER_0,MCPWM_GEN_B);//Give PH a constant low level
      mcpwm_set_duty_type(MCPWM_UNIT_1,MCPWM_TIMER_0,MCPWM_GEN_A,MCPWM_DUTY_MODE_0);
      mcpwm_set_duty(MCPWM_UNIT_1,MCPWM_TIMER_0,MCPWM_GEN_A,speed);//The EN pin outputs a PWM wave with a duty cycle of "speed"
      break;

    case 4:
      mcpwm_set_signal_high(MCPWM_UNIT_1,MCPWM_TIMER_1,MCPWM_GEN_B);//Give PH a constant low level
      mcpwm_set_duty_type(MCPWM_UNIT_1,MCPWM_TIMER_1,MCPWM_GEN_A,MCPWM_DUTY_MODE_0);
      mcpwm_set_duty(MCPWM_UNIT_1,MCPWM_TIMER_1,MCPWM_GEN_A,speed);//The EN pin outputs a PWM wave with a duty cycle of "speed"
      break;
  }
}

void breake(uint8_t motorNumber)
{
  switch(motorNumber)
  {
    case 1:
      mcpwm_set_signal_low(MCPWM_UNIT_0,MCPWM_TIMER_0,MCPWM_GEN_A);
      break;

    case 2:
      mcpwm_set_signal_low(MCPWM_UNIT_0,MCPWM_TIMER_1,MCPWM_GEN_A);
      break;

    case 3:
      mcpwm_set_signal_low(MCPWM_UNIT_1,MCPWM_TIMER_0,MCPWM_GEN_A);
      break;

    case 4:
      mcpwm_set_signal_low(MCPWM_UNIT_1,MCPWM_TIMER_1,MCPWM_GEN_A);
      break;
  }
}

void moveCar(uint8_t inputValue,uint8_t speed)
{
  switch(inputValue)
  {
    case UP:
      advance(1,speed);
      advance(2,speed);
      advance(3,speed);
      advance(4,speed);
      break;

    case DOWN:
      retreat(1,speed);
      retreat(2,speed);
      retreat(3,speed);
      retreat(4,speed);
      break;

    case LEFT:
      advance(3,speed);
      advance(4,speed);
      retreat(1,speed);
      retreat(2,speed);
      break;

    case RIGHT:
      advance(1,speed);
      advance(2,speed);
      retreat(3,speed);
      retreat(4,speed);
      break;

    case STOP:
      breake(1);
      breake(2);
      breake(3);
      breake(4);
      break;
  }
}

void handleRoot(AsyncWebServerRequest *request) 
{
  request->send_P(200, "text/html", htmlHomePage);
}

void handleNotFound(AsyncWebServerRequest *request) 
{
    request->send(404, "text/plain", "File Not Found");
}

void onCarInputWebSocketEvent(AsyncWebSocket *server, 
                      AsyncWebSocketClient *client, 
                      AwsEventType type,
                      void *arg, 
                      uint8_t *data, 
                      size_t len) 
{                      
  switch (type) 
  {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      moveCar(STOP,0);
      //ledcWrite(PWMLightChannel, 0); 
      //panServo.write(90);
      //tiltServo.write(90);       
      break;
    case WS_EVT_DATA:
      AwsFrameInfo *info;
      info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
      {
        std::string myData = "";
        myData.assign((char *)data, len);
        std::istringstream ss(myData);
        std::string key, value;
        std::getline(ss, key, ',');
        std::getline(ss, value, ',');
        Serial.printf("Key [%s] Value[%s]\n", key.c_str(), value.c_str()); 
        int valueInt = atoi(value.c_str());     
        if (key == "MoveCar")
        {
          moveCar(valueInt,Speed);        
        }
        else if (key == "Speed")
        {
          Speed = valueInt;
        }
        /*else if (key == "Light")
        {
          //ledcWrite(PWMLightChannel, valueInt);         
        }
        else if (key == "Pan")
        {
          //panServo.write(valueInt);
        }
        else if (key == "Tilt")
        {
          //tiltServo.write(valueInt);   
        }  */           
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;  
  }
}

void onCameraWebSocketEvent(AsyncWebSocket *server, 
                      AsyncWebSocketClient *client, 
                      AwsEventType type,
                      void *arg, 
                      uint8_t *data, 
                      size_t len) 
{                      
  switch (type) 
  {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      cameraClientId = client->id();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      cameraClientId = 0;
      break;
    case WS_EVT_DATA:
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;  
  }
}

void setupCamera()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;   
  config.pin_sccb_scl = SIOC_GPIO_NUM; 
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 8;
  config.fb_count = 1;

    // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }


  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) 
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }  

  if (psramFound())
  {
    heap_caps_malloc_extmem_enable(20000);  
    Serial.printf("PSRAM initialized. malloc to take memory from psram above this size");    
  }  
}

void sendCameraPicture()
{
  if (cameraClientId == 0)
  {
    return;
  }
  unsigned long  startTime1 = millis();
  //capture a frame
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) 
  {
      Serial.println("Frame buffer could not be acquired");
      return;
  }

  unsigned long  startTime2 = millis();
  wsCamera.binary(cameraClientId, fb->buf, fb->len);
  esp_camera_fb_return(fb);

  //Wait for message to be delivered
  while (true)
  {
    AsyncWebSocketClient * clientPointer = wsCamera.client(cameraClientId);
    if (!clientPointer || !(clientPointer->queueIsFull()))
    {
      break;
    }
    delay(1);
  }

  unsigned long  startTime3 = millis();  
  Serial.printf("Time taken Total: %d|%d|%d\n",startTime3 - startTime1, startTime2 - startTime1, startTime3-startTime2 );
}

//Initialize the pins needed to generate the PWM signal
void mcpwm_init(void)
{
  //Configure mcpwm information
  mcpwm_config_t pwm_config;
  pwm_config.frequency = 500;
  pwm_config.cmpr_a = 0;
  pwm_config.cmpr_b = 0;
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

  //A speed B direction
  mcpwm_gpio_init(MCPWM_UNIT_0,MCPWM0A,M1_EN);//Motor 1 GPIO
  mcpwm_gpio_init(MCPWM_UNIT_0,MCPWM0B,M1_PN);
  mcpwm_init(MCPWM_UNIT_0,MCPWM_TIMER_0,&pwm_config);//Initializes a unit of mcpwm and binds the clock

  mcpwm_gpio_init(MCPWM_UNIT_0,MCPWM1A,M2_EN);//Motor 2 GPIO
  mcpwm_gpio_init(MCPWM_UNIT_0,MCPWM1B,M2_PN);
  mcpwm_init(MCPWM_UNIT_0,MCPWM_TIMER_1,&pwm_config);//Initializes a unit of mcpwm and binds the clock

  mcpwm_gpio_init(MCPWM_UNIT_1,MCPWM0A,M3_EN);//Motor 3 GPIO
  mcpwm_gpio_init(MCPWM_UNIT_1,MCPWM0B,M3_PN);
  mcpwm_init(MCPWM_UNIT_1,MCPWM_TIMER_0,&pwm_config);//Initializes a unit of mcpwm and binds the clock

  mcpwm_gpio_init(MCPWM_UNIT_1,MCPWM1A,M4_EN);//Motor 4 GPIO
  mcpwm_gpio_init(MCPWM_UNIT_1,MCPWM1B,M4_PN);
  mcpwm_init(MCPWM_UNIT_1,MCPWM_TIMER_1,&pwm_config);//Initializes a unit of mcpwm and binds the clock
}


void setup(void) 
{
  Serial.begin(115200);
  while(axp.begin() != 0){
    Serial.println("init error");
    delay(1000);
  }

  axp.enableCameraPower(axp.eOV2640);//Set up camera power supply
  mcpwm_init();

  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);

  wsCamera.onEvent(onCameraWebSocketEvent);
  server.addHandler(&wsCamera);

  wsCarInput.onEvent(onCarInputWebSocketEvent);
  server.addHandler(&wsCarInput);

  server.begin();
  Serial.println("HTTP server started");

  setupCamera();
}


void loop() 
{
  /*  Kamera-Frame versenden  */
  sendCameraPicture();

  /*  WebSocket-Clients abräumen  */
  wsCamera.cleanupClients();
  wsCarInput.cleanupClients();
}