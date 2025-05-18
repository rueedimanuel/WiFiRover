#include "html_page.h"

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
        <div id="distanceDisplay" style="font-size:1.1rem;font-weight:700;">Distanz: -- cm</div>
      </div>
    </div>

    <script>
      /* ---------------- WebSockets ---------------- */
      const camURL = `ws://${location.hostname}/Camera`;
      const ctrlURL = `ws://${location.hostname}/CarInput`;
      let wsCam, wsCtrl;
      let currentMoveDir = '0';  // last direction to send

      function initSockets() {
        wsCam = new WebSocket(camURL);
        wsCam.binaryType = 'blob';
        wsCam.onmessage = e => {
          document.getElementById('cameraImage').src = URL.createObjectURL(e.data);
        };
        wsCam.onclose = () => setTimeout(initSockets, 2000);

        wsCtrl = new WebSocket(ctrlURL);
        wsCtrl.onopen = () => {
          // send initial speed
          sendButtonInput('Speed', document.getElementById('Speed').value);
        };
        wsCtrl.onmessage = e => {
          const [key, val] = e.data.split(',');
          if (key === 'Distance') {
            document.getElementById('distanceDisplay').textContent = `Distanz: ${val} cm`;
          }
        };
        wsCtrl.onclose = () => setTimeout(initSockets, 2000);
      }

      function sendButtonInput(key, val) {
        if (wsCtrl?.readyState === WebSocket.OPEN) {
          wsCtrl.send(`${key},${val}`);
        }
      }

      // send MoveCar,<dir> every 200 ms
      function periodicMoveSend() {
        if (wsCtrl?.readyState === WebSocket.OPEN) {
          wsCtrl.send(`MoveCar,${currentMoveDir}`);
        }
      }
      setInterval(periodicMoveSend, 200);

      /* ---------------- Joystick OR Arrow Pad ---------------- */
      const joystickDiv = document.getElementById('joystickContainer');
      const arrowPad   = document.getElementById('arrowPad');

      function enableArrowPad() {
        joystickDiv.classList.add('hidden');
        arrowPad.classList.remove('hidden');
      }

      function setupArrowPadEvents() {
        arrowPad.querySelectorAll('.arrow-button').forEach(btn => {
          btn.addEventListener('touchstart', () => {
            currentMoveDir = btn.dataset.dir;
          });
          btn.addEventListener('touchend', () => {
            currentMoveDir = '0';
          });
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
          }, 2500);

          manager.on('start', () => joystickDiv.classList.add('active'));
          manager.on('end', () => {
            joystickDiv.classList.remove('active');
            currentMoveDir = '0';
          });
          manager.on('dir', (_, data) => {
            joystickMoved = true;
            const map = { up: '1', down: '2', left: '3', right: '4' };
            currentMoveDir = map[data.direction.angle] ?? '0';
          });
        } catch (e) {
          console.error('Joystick failed, using arrow pad', e);
          enableArrowPad();
          setupArrowPadEvents();
        }
      }

      window.addEventListener('DOMContentLoaded', () => {
        initSockets();
        setupJoystick();

        // keyboard fallback
        window.addEventListener('keydown', e => {
          const m = { ArrowUp: '1', ArrowDown: '2', ArrowLeft: '3', ArrowRight: '4' };
          if (m[e.key]) currentMoveDir = m[e.key];
        });
        window.addEventListener('keyup', e => {
          if (['ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.key)) {
            currentMoveDir = '0';
          }
        });
      });
    </script>

  </body>
</html>
)HTMLHOMEPAGE";