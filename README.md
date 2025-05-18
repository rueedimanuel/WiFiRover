# ESP32 WiFi Rover 

Dieses Projekt implementiert eine Web-basierte Fernsteuerung für einen Rover mit einer ESP32-Plattform. Der Rover verfügt über vier Gleichstrommotoren, einen Ultraschallsensor zur Hinderniserkennung und einen Kamera-Stream, der über WebSockets aufgerufen wird.

### Funktionen

* **WiFi-Access-Point**: Der ESP32 betreibt einen WLAN-Access-Point (`WiFiRover`), über den sich Clients verbinden können.
* **Webserver & WebSockets**:

  * HTTP-Server liefert die HTML-Oberfläche.
  * WebSocket `/Camera` streamt JPEG-Bilder der Kamera.
  * WebSocket `/CarInput` empfängt Steuerbefehle (Geschwindigkeit, Richtungsbefehle) und sendet Sensordaten (Abstand).
* **Motorsteuerung**:

  * Vier Motoren werden über MCPWM (Motor Control PWM) des ESP32 gesteuert.
  * Vorwärts, Rückwärts, Links, Rechts und Stopp.
  * Geschwindigkeitsbegrenzung basierend auf Ultraschall-Abstandsmessung (20 cm–100 cm linear).
* **Kamera**:

  * Initialisierung der ESP32-Kamera mit JPEG-Streaming.
  * PSRAM-Erkennung für höhere Bildqualität.
* **Ultraschallsensor**:

  * 12‑Bit ADC-Auslesung (0–4095) umgerechnet auf 0–520 cm.
  * Abstand wird alle 200 ms über WebSocket gesendet.


### Voraussetzungen

* ESP32-CAM-Modul (z. B. ESP32-S3-CAM)
* Ultraschallsensor (analog, z. B. HC-SR04 mit Spannungsteiler)
* Gleichstrommotoren (4×) mit Treiberschaltung
* DFRobot AXP313A Stromversorgungsmodul
* Entwicklungsumgebung: Arduino IDE oder PlatformIO

### Installation

1. Projekt in der Arduino IDE oder PlatformIO öffnen.
2. Bibliotheken installieren:

   * `esp_camera`
   * `ESPAsyncWebServer`
   * `AsyncTCP`
   * `DFRobot_AXP313A`
3. Pinbelegung prüfen und ggf. anpassen (`html_page.cpp`, `main.cpp`).
4. Kompilieren und auf das ESP32-CAM-Modul hochladen.

### Nutzung

1. Mit dem WLAN-Netzwerk `WiFiRover` verbinden (Kennwort: `12345678`).
2. Im Browser mit der IP-Adresse: `192.168.4.1` verbinden.
3. Kamera-Stream und Steuerung über virtuelle Joystick-/Pfeiltasten und Geschwindigkeits-Slider.

### Limitation

Bei schlechter Verbindung kann es zu Latenzen im Video-Stream kommen – den Controller nahe beim Rover halten.
