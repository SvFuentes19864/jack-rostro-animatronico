#include <Bluepad32.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <math.h>

// =======================
// 🌈 Configuración Neopixel - Dos aros
// =======================
#define PIN1           1
#define PIN2           2
#define NUMPIXELS      12
#define BRIGHTNESS     60

Adafruit_NeoPixel strip1(NUMPIXELS, PIN1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUMPIXELS, PIN2, NEO_GRB + NEO_KHZ800);

// =======================
// ⚙️ PCA9685 - Controlador de servos
// =======================
Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);  // Dirección 0x40

// =======================
// 🔢 Canales (Derecha primero, luego Izquierda)
// =======================
// --- Derecha ---
#define SERVO_CHANNEL_RIGHT          0   // Ojo derecho (horizontal)
#define SERVO_CHANNEL_VERTICAL       1   // Ojo vertical (mecanismo del lado derecho)
#define SERVO_CHANNEL_EXTRA_1        2   // Párpado derecho
#define SERVO_CHANNEL_EYEBROW_RIGHT  3   // Ceja derecha
#define SERVO_CHANNEL_JAW_RIGHT      4   // Mandíbula derecha
// --- Izquierda ---
#define SERVO_CHANNEL_LEFT           5   // Ojo izquierdo (horizontal)
#define SERVO_CHANNEL_EXTRA_2        6   // Párpado izquierdo
#define SERVO_CHANNEL_EYEBROW_LEFT   7   // Ceja izquierda
#define SERVO_CHANNEL_JAW_LEFT       8   // Mandíbula izquierda

// =======================
// 🔩 Límites base
// =======================
#define SERVO_MID           90
#define SERVO_MIN_X         45
#define SERVO_MAX_X         125
#define SERVO_MIN_Y         45
#define SERVO_MAX_Y         135

// Párpados
#define SERVO_EXTRA1_PRESSED 45    // Párpado derecho cerrado
#define SERVO_EXTRA2_PRESSED 155   // Párpado izquierdo cerrado
#define SERVO_EXTRA1_HALF    65    // derecho — entrecerrado
#define SERVO_EXTRA2_HALF    120   // izquierdo — entrecerrado

#define EYEBROW_CENTER       90
#define EYEBROW_RANGE        20
#define EYEBROW_MIN          (EYEBROW_CENTER - EYEBROW_RANGE)
#define EYEBROW_MAX          (EYEBROW_CENTER + EYEBROW_RANGE)

// Helpers para cejas individuales
const int EYEBROW_UP   = EYEBROW_CENTER + EYEBROW_RANGE;
const int EYEBROW_DOWN = EYEBROW_CENTER - EYEBROW_RANGE;

// --- 🦷 Mandíbula ---
#define JAW_CLOSE     75
#define JAW_OPEN      120   // apertura grande (AUTO “habla”)
#define JAW_SOFT_OPEN 95    // apertura suave (DPAD↓ en NORMAL)

// 👉 Si tu mecánica está invertida, pon 1 (tu caso)
#define JAW_SERVO_INVERT 1  // 0 = como antes, 1 = invertido

// =======================
// 🎮 Bluepad32
// =======================
#define CLEAN_PAIRING 0
ControllerPtr myControllers[BP32_MAX_GAMEPADS];

bool prevA[BP32_MAX_GAMEPADS]  = {false};
bool prevB[BP32_MAX_GAMEPADS]  = {false};
bool prevX[BP32_MAX_GAMEPADS]  = {false};
bool prevY[BP32_MAX_GAMEPADS]  = {false};
bool prevL1[BP32_MAX_GAMEPADS] = {false};
bool prevR1[BP32_MAX_GAMEPADS] = {false};
bool prevL2[BP32_MAX_GAMEPADS] = {false};
bool prevR2[BP32_MAX_GAMEPADS] = {false};
bool prevShare[BP32_MAX_GAMEPADS] = {false};
uint8_t prevDpad[BP32_MAX_GAMEPADS] = {0};

// 🔽 D-Pad enums fallback
#ifndef DPAD_UP
#define DPAD_UP    0x01
#endif
#ifndef DPAD_DOWN
#define DPAD_DOWN  0x02
#endif
#ifndef DPAD_LEFT
#define DPAD_LEFT  0x04
#endif
#ifndef DPAD_RIGHT
#define DPAD_RIGHT 0x08
#endif

// =======================
// 🎨 Colores base
// =======================
uint32_t COLOR_CIAN      = strip1.Color(0, 255, 255);
uint32_t COLOR_ROJO      = strip1.Color(255, 0, 0);
uint32_t COLOR_VERDE     = strip1.Color(0, 255, 0);
uint32_t COLOR_CAFE_MIEL  = strip1.Color(186, 140, 99); // 🍯 café miel
uint32_t COLOR_BLANCO    = strip1.Color(255, 255, 255);

// ➕ Colores extra
uint32_t COLOR_AMARILLO   = strip1.Color(255, 255, 0);
uint32_t COLOR_ROSA       = strip1.Color(255, 0, 128);
uint32_t COLOR_NARANJA    = strip1.Color(255, 128, 0);
uint32_t COLOR_AZUL       = strip1.Color(0, 0, 255);

// 🎯 Color persistente entre modos (inicia en cian)
uint32_t currentEyeColor = 0;   // se setea en setup

// 🎡 Carrusel de colores para MODE_COLOR con DPAD L/R
uint32_t colorPalette[] = {
  // orden variado y útil en escena
  // (puedes reordenar al gusto)
  // Nota: dejar todos aquí asegura que DPAD recorra todo.
  0, // placeholder para inicialización en setup
};
int colorIndex = 0;

void initColorPalette() {
  // Rellenamos el arreglo con los valores reales
  colorPalette[0] = COLOR_CIAN;
  // Si quieres agregar más colores, aumenta el tamaño y rellena aquí.
}

// =======================
// Helpers de color
// =======================
void setNeopixelColor(uint32_t color) {
  for (int i = 0; i < NUMPIXELS; i++) {
    strip1.setPixelColor(i, color);
    strip2.setPixelColor(i, color);
  }
  strip1.show();
  strip2.show();
}
uint32_t scaleColor(uint32_t c, float s) {
  uint8_t r = (c >> 16) & 0xFF;
  uint8_t g = (c >>  8) & 0xFF;
  uint8_t b = (c      ) & 0xFF;
  r = (uint8_t)constrain((int)(r * s), 0, 255);
  g = (uint8_t)constrain((int)(g * s), 0, 255);
  b = (uint8_t)constrain((int)(b * s), 0, 255);
  return strip1.Color(r,g,b);
}

// =======================
// 👁️ Suavizado + Calibración de OJOS (EMA)
// =======================
float EYE_RIGHT_OFFSET = 0.0f;
float EYE_LEFT_OFFSET  = 0.0f;
float EYE_RIGHT_GAIN   = 1.00f;
float EYE_LEFT_GAIN    = 0.95f;

int RIGHT_MIN_X = 75, RIGHT_MAX_X = 105;
int LEFT_MIN_X  = 75, LEFT_MAX_X  = 105;

float EYE_VERT_OFFSET = 0.0f;
float EYE_VERT_GAIN   = 1.10f;
int   VERT_MIN_Y      = 70;
int   VERT_MAX_Y      = 110;

float eyeH_cur_f = SERVO_MID;  // estado suavizado (horizontal)
float eyeV_cur_f = SERVO_MID;  // estado suavizado (vertical)

float EYE_ALPHA = 0.14f;
const int16_t JOY_DEADZONE = 70;

int16_t applyDeadzoneSmooth(int16_t raw, int16_t dz) {
  if (raw > -dz && raw < dz) return 0;
  if (raw > 0)  return map(raw, dz, 512, 0, 512);
  else          return map(raw, -dz, -512, 0, -512);
}
float cubicEase(float xNorm) { return xNorm * xNorm * xNorm; }

// =======================
// 🔧 Servos
// =======================
void setServoAngle(uint8_t channel, uint8_t angle) {
  uint16_t pulse = map(angle, 0, 180, 500, 2500);
  pca.writeMicroseconds(channel, pulse);
}
void setBothExtrasTo(uint8_t angle) {
  setServoAngle(SERVO_CHANNEL_EXTRA_1, angle);
  setServoAngle(SERVO_CHANNEL_EXTRA_2, angle);
}

// Mandíbula: servos opuestos (con opción de invertir mapeo)
void moverMandibula(uint8_t baseAngle) {
#if JAW_SERVO_INVERT
  uint8_t leftAngle  = 180 - baseAngle;
  uint8_t rightAngle = baseAngle;
#else
  uint8_t leftAngle  = baseAngle;
  uint8_t rightAngle = 180 - baseAngle;
#endif
  setServoAngle(SERVO_CHANNEL_JAW_LEFT, leftAngle);
  setServoAngle(SERVO_CHANNEL_JAW_RIGHT, rightAngle);
}
void moverMandibulaSuave(uint8_t inicio, uint8_t fin, uint8_t stepDelay = 10) {
  int dir = (fin > inicio) ? 1 : -1;
  for (int angle = inicio; angle != fin; angle += dir) {
#if JAW_SERVO_INVERT
    uint8_t leftAngle  = 180 - angle;
    uint8_t rightAngle = angle;
#else
    uint8_t leftAngle  = angle;
    uint8_t rightAngle = 180 - angle;
#endif
    setServoAngle(SERVO_CHANNEL_JAW_LEFT, leftAngle);
    setServoAngle(SERVO_CHANNEL_JAW_RIGHT, rightAngle);
    delay(stepDelay);
  }
  moverMandibula(fin);
}

// =======================
// 🪄 Cejas
// =======================
// Enfadado “∧”: derecha baja (CENTER - delta), izquierda baja opuesto (CENTER + delta)
void setBrowsMirroredAngry(int delta) {
  int right = constrain(EYEBROW_CENTER - delta, EYEBROW_MIN, EYEBROW_MAX);
  int left  = constrain(EYEBROW_CENTER + delta, EYEBROW_MIN, EYEBROW_MAX);
  setServoAngle(SERVO_CHANNEL_EYEBROW_RIGHT, right);
  setServoAngle(SERVO_CHANNEL_EYEBROW_LEFT,  left);
}

// =======================
// 🔥 Estados (modos de usuario)
// =======================
enum Mode : uint8_t { MODE_NORMAL = 0, MODE_DEVIL = 1, MODE_AUTO = 2, MODE_COLOR = 3 };
Mode currentMode = MODE_NORMAL;

// =======================
// 🎬 AUTO MODE (capas con “stingers” diabólicos)
// =======================
struct AutoLayers {
  // Habla
  bool speaking = false;
  unsigned long speakSegEnd = 0;
  float speechAmp = 0.0f;
  // Mirada
  int gazeTargetX = SERVO_MID;
  int gazeTargetY = SERVO_MID;
  unsigned long nextGazeChange = 0;
  // Micro-saccades
  unsigned long lastMicroMs = 0;
  // Blinks
  bool blinkActive = false;
  unsigned long blinkStart = 0;
  unsigned long nextBlink = 0;
  // Diablo “stinger”
  bool devilCue = false;
  unsigned long devilStart = 0;
  unsigned long devilEnd = 0;
  unsigned long nextDevil = 0;
  // Para transición suave a rojo / entrecerrado
  float devilMix = 0.0f; // 0=neutral, 1=full diablo
} autoL;

float lerp(float a, float b, float t) { return a + (b - a) * t; }

void scheduleNextBlink(unsigned long now) {
  autoL.nextBlink = now + (unsigned long)(1500 + random(0, 3500)); // 1.5–5s
}
void scheduleNextGaze(unsigned long now) {
  autoL.nextGazeChange = now + (unsigned long)(800 + random(0, 1000)); // 0.8–1.8s
}
void scheduleNextDevil(unsigned long now) {
  autoL.nextDevil = now + (unsigned long)(12000 + random(0, 12000)); // 12–24s
}
void scheduleSpeechSegment(unsigned long now) {
  autoL.speaking = !autoL.speaking ? true : (random(0,100) < 70);
  unsigned long dur = autoL.speaking ? (unsigned long)(2000 + random(0, 4000))  // 2–6s
                                     : (unsigned long)(1200 + random(0, 1800)); // 1.2–3s
  autoL.speakSegEnd = now + dur;
}
// 0..1 “amplitud de voz”
float speechEnvelope(unsigned long now, bool speaking, float prev) {
  float t = now / 1000.0f;
  float base = 0.0f;
  if (speaking) {
    base = 0.55f * fabsf(sinf(t * 5.3f))
         + 0.35f * fabsf(sinf(t * 2.7f))
         + 0.20f * fabsf(sinf(t * 7.1f));
    base = constrain(base, 0.0f, 1.2f);
  }
  float out = prev + 0.25f * (base - prev);
  if (!speaking) out *= 0.92f;
  return constrain(out, 0.0f, 1.0f);
}

void enterAutoMode() {
  currentMode = MODE_AUTO;
  unsigned long now = millis();

  // Estado base neutral
  setNeopixelColor(COLOR_CIAN);
  setBothExtrasTo(SERVO_MID);
  moverMandibula(JAW_CLOSE);
  setServoAngle(SERVO_CHANNEL_EYEBROW_RIGHT, EYEBROW_CENTER);
  setServoAngle(SERVO_CHANNEL_EYEBROW_LEFT,  EYEBROW_CENTER);

  // Timers
  autoL.speaking = true;
  autoL.speechAmp = 0.0f;
  autoL.gazeTargetX = SERVO_MID;
  autoL.gazeTargetY = SERVO_MID;
  scheduleNextGaze(now);
  scheduleNextBlink(now);

  autoL.devilCue = false;
  autoL.devilMix = 0.0f;
  autoL.devilStart = 0;
  autoL.devilEnd = 0;
  scheduleNextDevil(now);

  autoL.lastMicroMs = now;

  Serial.println("▶️ AUTO: neutral con mirada/parpadeo/habla + stingers DIABLO (ojos rojos, cejas ∧, párpados entrecerrados).");
}

void enterDevilModeManual() { // botón X
  currentMode = MODE_DEVIL;
  setNeopixelColor(COLOR_ROJO);
  setBrowsMirroredAngry((EYEBROW_RANGE * 4) / 5);
  setServoAngle(SERVO_CHANNEL_EXTRA_1, SERVO_EXTRA1_HALF);
  setServoAngle(SERVO_CHANNEL_EXTRA_2, SERVO_EXTRA2_HALF);
  Serial.println("💀 MODO DIABLO (manual): cejas espejo (∧). Solo ojos.");
}

void exitToNormal() { // botón Y
  currentMode = MODE_NORMAL;
  // ✅ Mantener color elegido en MODE_COLOR
  setNeopixelColor(currentEyeColor);
  setServoAngle(SERVO_CHANNEL_EYEBROW_RIGHT, EYEBROW_CENTER);
  setServoAngle(SERVO_CHANNEL_EYEBROW_LEFT,  EYEBROW_CENTER);
  setBothExtrasTo(SERVO_MID);
  moverMandibula(JAW_CLOSE);
  Serial.println("😌 MODO NORMAL.");
}

// ======= MODE_COLOR =======
void syncColorIndexToCurrent() {
  const int n = (int)(sizeof(colorPalette)/sizeof(colorPalette[0]));
  for (int i = 0; i < n; ++i) {
    if (colorPalette[i] == currentEyeColor) { colorIndex = i; return; }
  }
  colorIndex = 0;
}
void enterColorMode() {
  currentMode = MODE_COLOR;
  setNeopixelColor(currentEyeColor);
  syncColorIndexToCurrent();
  Serial.println("🎨 MODO COLOR: usa botones o DPAD ←/→. Y para volver a NORMAL.");
}

// Tick del modo AUTO (capas + stinger)
void autoTick() {
  unsigned long now = millis();

  // Habla on/off
  if (now > autoL.speakSegEnd) scheduleSpeechSegment(now);
  autoL.speechAmp = speechEnvelope(now, autoL.speaking, autoL.speechAmp);

  // Stinger Diablo
  if (!autoL.devilCue && now > autoL.nextDevil) {
    autoL.devilCue = true;
    autoL.devilStart = now;
    autoL.devilEnd = now + (unsigned long)(2000 + random(0, 1800)); // 2–3.8s
    scheduleNextDevil(now);
  }
  // Mezcla diablo
  if (autoL.devilCue) {
    float t = (now - autoL.devilStart) / 250.0f; // ~250ms para subir
    autoL.devilMix = constrain(autoL.devilMix + 0.06f + 0.02f * t, 0.0f, 1.0f);
    if (now > autoL.devilEnd) autoL.devilCue = false;
  } else {
    autoL.devilMix = max(0.0f, autoL.devilMix - 0.05f); // baja suave
  }

  // Blink
  if (!autoL.blinkActive && now > autoL.nextBlink) {
    autoL.blinkActive = true;
    autoL.blinkStart = now;
  }
  if (autoL.blinkActive) {
    if (now - autoL.blinkStart > 200) { // 120ms cierre + 80ms abrir
      autoL.blinkActive = false;
      scheduleNextBlink(now);
    }
  }

  // Cambios de mirada
  if (now > autoL.nextGazeChange) {
    int tx, ty;
    int pick = random(0, 5);
    switch (pick) {
      case 0: tx = map(random(-512,-128), -512, 512, SERVO_MIN_X+6, SERVO_MAX_X-6);
              ty = map(random(-64,192),   -512, 512, SERVO_MIN_Y+8,  SERVO_MAX_Y-8); break;
      case 1: tx = map(random(-256, 64),  -512, 512, SERVO_MIN_X+6, SERVO_MAX_X-6);
              ty = map(random(-192,128),  -512, 512, SERVO_MIN_Y+8,  SERVO_MAX_Y-8); break;
      case 2: tx = SERVO_MID + random(-6,6);
              ty = SERVO_MID + random(-4,6); break;
      case 3: tx = map(random(-64,256),   -512, 512, SERVO_MIN_X+6, SERVO_MAX_X-6);
              ty = map(random(-192,128),  -512, 512, SERVO_MIN_Y+8,  SERVO_MAX_Y-8); break;
      default:tx = map(random(128,512),   -512, 512, SERVO_MIN_X+6, SERVO_MAX_X-6);
              ty = map(random(-64,192),   -512, 512, SERVO_MIN_Y+8,  SERVO_MAX_Y-8); break;
    }
    autoL.gazeTargetX = tx;
    autoL.gazeTargetY = ty;
    scheduleNextGaze(now);
  }

  // Micro-saccades
  static float microPhaseX = 0.0f, microPhaseY = 0.0f;
  if (now - autoL.lastMicroMs > 20) {
    autoL.lastMicroMs = now;
    microPhaseX += 0.18f + (random(-5,5) * 0.005f);
    microPhaseY += 0.15f + (random(-5,5) * 0.005f);
  }
  int microX = (int)(sin(microPhaseX) * 2.5f);
  int microY = (int)(sin(microPhaseY) * 2.0f);

  // Ojos + micro “nod” con la voz
  float nod = autoL.speechAmp * 6.0f * sinf(now / 420.0f);
  int eyeTargetX = autoL.gazeTargetX + microX;
  int eyeTargetY = autoL.gazeTargetY + microY + (int)nod;

  // Suavizados
  eyeH_cur_f = eyeH_cur_f + 0.22f * (eyeTargetX - eyeH_cur_f);
  eyeV_cur_f = eyeV_cur_f + 0.22f * (eyeTargetY - eyeV_cur_f);

  int cmdRight = constrain((int)round((eyeH_cur_f - SERVO_MID) * EYE_RIGHT_GAIN + SERVO_MID + EYE_RIGHT_OFFSET), RIGHT_MIN_X, RIGHT_MAX_X);
  int cmdLeft  = constrain((int)round((eyeH_cur_f - SERVO_MID) * EYE_LEFT_GAIN  + SERVO_MID + EYE_LEFT_OFFSET),  LEFT_MIN_X,  LEFT_MAX_X);
  int cmdVert  = constrain((int)round((eyeV_cur_f - SERVO_MID) * EYE_VERT_GAIN  + SERVO_MID + EYE_VERT_OFFSET),  VERT_MIN_Y,  VERT_MAX_Y);
  setServoAngle(SERVO_CHANNEL_RIGHT, cmdRight);
  setServoAngle(SERVO_CHANNEL_LEFT,  cmdLeft);
  setServoAngle(SERVO_CHANNEL_VERTICAL, cmdVert);

  // Mandíbula con la “voz”
  uint8_t jaw = (uint8_t)round(lerp(JAW_CLOSE, JAW_OPEN, 0.10f + 0.90f*autoL.speechAmp));
  moverMandibula(jaw);

  // Párpados (voz/blink) + transición diabólica
  int lidR = SERVO_MID;
  int lidL = SERVO_MID;
  if (autoL.blinkActive) {
    unsigned long dt = now - autoL.blinkStart;
    if (dt < 120) {
      float t = dt / 120.0f;
      lidR = (int)round(lerp(SERVO_MID, SERVO_EXTRA1_PRESSED, t));
      lidL = (int)round(lerp(SERVO_MID, SERVO_EXTRA2_PRESSED, t));
    } else {
      float t = (dt - 120) / 80.0f;
      lidR = (int)round(lerp(SERVO_EXTRA1_PRESSED, SERVO_MID, t));
      lidL = (int)round(lerp(SERVO_EXTRA2_PRESSED, SERVO_MID, t));
    }
  } else {
    float clos = 0.18f * autoL.speechAmp;
    float devilExtra = 0.50f * autoL.devilMix;  // entrecerrado adicional
    float mix = min(1.0f, clos + devilExtra);
    lidR = (int)round(lerp(SERVO_MID, SERVO_EXTRA1_HALF, mix));
    lidL = (int)round(lerp(SERVO_MID, SERVO_EXTRA2_HALF, mix));
  }
  setServoAngle(SERVO_CHANNEL_EXTRA_1, lidR);
  setServoAngle(SERVO_CHANNEL_EXTRA_2, lidL);

  // Cejas
  if (autoL.devilMix > 0.02f) {
    int angryDelta = (int)round(((EYEBROW_RANGE * 4) / 5) * autoL.devilMix);
    setBrowsMirroredAngry(angryDelta);
  } else {
    static float browR = EYEBROW_CENTER, browL = EYEBROW_CENTER;
    browR = browR + 0.15f * (EYEBROW_CENTER - browR);
    browL = browL + 0.15f * (EYEBROW_CENTER - browL);
    setServoAngle(SERVO_CHANNEL_EYEBROW_RIGHT, (int)round(browR));
    setServoAngle(SERVO_CHANNEL_EYEBROW_LEFT,  (int)round(browL));
  }

  // Luces base + mezcla a rojo (diablo)
  float breathe = 0.35f + 0.60f * (0.25f + 0.75f * autoL.speechAmp);
  uint32_t base = scaleColor(COLOR_CIAN, breathe);
  uint32_t redPulse = scaleColor(COLOR_ROJO, 0.6f + 0.4f * autoL.speechAmp);
  if (autoL.devilMix > 0.0f) {
    uint8_t br = (base >> 16) & 0xFF, bg = (base >> 8) & 0xFF, bb = base & 0xFF;
    uint8_t rr = (redPulse >> 16) & 0xFF, rg = (redPulse >> 8) & 0xFF, rb = redPulse & 0xFF;
    float m = autoL.devilMix;
    uint8_t r = (uint8_t)round(lerp(br, rr, m));
    uint8_t g = (uint8_t)round(lerp(bg, rg, m));
    uint8_t b = (uint8_t)round(lerp(bb, rb, m));
    setNeopixelColor(strip1.Color(r,g,b));
  } else {
    setNeopixelColor(base);
  }
}

// =======================
// 🔌 Callbacks del mando
// =======================
void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      myControllers[i] = ctl;
      Console.printf("✅ Control conectado en índice %d\n", i);
      setNeopixelColor(scaleColor(COLOR_CIAN, 0.16f));

      setServoAngle(SERVO_CHANNEL_LEFT, SERVO_MID);
      setServoAngle(SERVO_CHANNEL_RIGHT, SERVO_MID);
      setServoAngle(SERVO_CHANNEL_VERTICAL, SERVO_MID);
      setBothExtrasTo(SERVO_MID);
      setServoAngle(SERVO_CHANNEL_EYEBROW_RIGHT, EYEBROW_CENTER);
      setServoAngle(SERVO_CHANNEL_EYEBROW_LEFT,  EYEBROW_CENTER);
      moverMandibula(JAW_CLOSE);

      eyeH_cur_f = SERVO_MID;
      eyeV_cur_f = SERVO_MID;

      currentMode = MODE_NORMAL;

      Console.printf("🎭 Listo. B=AUTO (stingers), X=DIABLO manual, Y=NORMAL, A=COLOR (DPAD←/→ carrusel).\n");
      return;
    }
  }
}
void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      myControllers[i] = nullptr;
      setNeopixelColor(0);
      Console.printf("🔌 Control desconectado (%d)\n", i);
      return;
    }
  }
}

// =======================
// 🕹️ Lógica principal de control
// =======================
void processGamepad(ControllerPtr ctl) {
  const int idx = ctl->index();

  bool a  = ctl->a();     // ahora: entra a MODE_COLOR
  bool b  = ctl->b();     // AUTO
  bool x  = ctl->x();     // DIABLO (manual)
  bool y  = ctl->y();     // NORMAL

  bool l1 = ctl->l1();    // L1 levanta ceja IZQUIERDA (prioridad)
  bool r1 = ctl->r1();    // R1 levanta ceja DERECHA  (prioridad)
  bool l2 = ctl->l2();    // párpado derecho (NORMAL)
  bool r2 = ctl->r2();    // párpado izquierdo (NORMAL)
  bool share = ctl->miscCapture();

  uint8_t dpad = ctl->dpad();
  uint8_t dpadPrev = prevDpad[idx];
  bool dpadDownNow  = (dpad & DPAD_DOWN);
  bool dpadDownPrev = (dpadPrev & DPAD_DOWN);

  // Cambios de estado
  if (b && !prevB[idx]) enterAutoMode();
  if (x && !prevX[idx]) enterDevilModeManual();
  if (y && !prevY[idx]) exitToNormal();
  if (a && !prevA[idx]) enterColorMode();

  if (currentMode == MODE_AUTO) {
    autoTick();  // ignora demás controles
  }
  else if (currentMode == MODE_COLOR) {
    // ---- Botones directos y sincronización del carrusel ----
    auto syncIndex = [&](){
      const int n = (int)(sizeof(colorPalette)/sizeof(colorPalette[0]));
      for (int i = 0; i < n; ++i) if (colorPalette[i] == currentEyeColor) { colorIndex = i; break; }
    };

    if (ctl->a())  { currentEyeColor = COLOR_CIAN;     setNeopixelColor(currentEyeColor); syncIndex(); }
    if (ctl->b())  { currentEyeColor = COLOR_ROJO;     setNeopixelColor(currentEyeColor); syncIndex(); }
    if (ctl->x())  { currentEyeColor = COLOR_VERDE;    setNeopixelColor(currentEyeColor); syncIndex(); }
    if (ctl->l1()) { currentEyeColor = COLOR_BLANCO;   setNeopixelColor(currentEyeColor); syncIndex(); }
    if (ctl->r1()) { currentEyeColor = COLOR_AMARILLO; setNeopixelColor(currentEyeColor); syncIndex(); }
    if (ctl->l2()) { currentEyeColor = COLOR_ROSA;     setNeopixelColor(currentEyeColor); syncIndex(); }
    if (ctl->r2()) { currentEyeColor = COLOR_NARANJA;  setNeopixelColor(currentEyeColor); syncIndex(); }
    // DPAD_UP no se usa porque tu mando no lo reporta; mantenemos RIGHT/LEFT

    // ---- Carrusel con DPAD RIGHT/LEFT ----
    const int paletteLen = (int)(sizeof(colorPalette)/sizeof(colorPalette[0]));
    bool rightNow = (dpad & DPAD_RIGHT);
    bool rightPrev = (dpadPrev & DPAD_RIGHT);
    bool leftNow  = (dpad & DPAD_LEFT);
    bool leftPrev = (dpadPrev & DPAD_LEFT);

    if (rightNow && !rightPrev) {
      colorIndex = (colorIndex + 1) % paletteLen;
      currentEyeColor = colorPalette[colorIndex];
      setNeopixelColor(currentEyeColor);
    }
    if (leftNow && !leftPrev) {
      colorIndex = (colorIndex - 1 + paletteLen) % paletteLen;
      currentEyeColor = colorPalette[colorIndex];
      setNeopixelColor(currentEyeColor);
    }
    // Salir con Y ya está arriba (exitToNormal)
  }
  else if (currentMode == MODE_NORMAL) {
    // Share mantiene cian tenue si quieres un "reset" rápido
    if (share && !prevShare[idx]) setNeopixelColor(COLOR_CIAN);

    // Párpados por gatillos
    if (l2 != prevL2[idx]) setServoAngle(SERVO_CHANNEL_EXTRA_1, l2 ? SERVO_EXTRA1_PRESSED : SERVO_MID);
    if (r2 != prevR2[idx]) setServoAngle(SERVO_CHANNEL_EXTRA_2, r2 ? SERVO_EXTRA2_PRESSED : SERVO_MID);

    // ✨ DPAD ABAJO: abrir mandíbula SUAVE y POCO mientras esté presionado
    if (dpadDownNow && !dpadDownPrev) {
      moverMandibulaSuave(JAW_CLOSE, JAW_SOFT_OPEN, 12);
    } else if (!dpadDownNow && dpadDownPrev) {
      moverMandibulaSuave(JAW_SOFT_OPEN, JAW_CLOSE, 12);
    }

    // Ojos manuales (EMA)
    int16_t rawX = ctl->axisX();
    int16_t rawY = ctl->axisY();
    int16_t joyX = applyDeadzoneSmooth(rawX, JOY_DEADZONE);
    int16_t joyY = applyDeadzoneSmooth(rawY, JOY_DEADZONE);
    float nx = cubicEase(joyX / 512.0f);
    float ny = cubicEase(joyY / 512.0f);

    float targetX = map(nx * 512.0f, -512, 512, SERVO_MIN_X, SERVO_MAX_X);
    float targetY = map(-ny * 512.0f, -512, 512, SERVO_MIN_Y, SERVO_MAX_Y);
    targetX = constrain((int)targetX, SERVO_MIN_X, SERVO_MAX_X);
    targetY = constrain((int)targetY, SERVO_MIN_Y, SERVO_MAX_Y);

    eyeH_cur_f = eyeH_cur_f + EYE_ALPHA * (targetX - eyeH_cur_f);
    eyeV_cur_f = eyeV_cur_f + EYE_ALPHA * (targetY - eyeV_cur_f);

    int cmdRight = constrain((int)round((eyeH_cur_f - SERVO_MID) * EYE_RIGHT_GAIN + SERVO_MID + EYE_RIGHT_OFFSET), RIGHT_MIN_X, RIGHT_MAX_X);
    int cmdLeft  = constrain((int)round((eyeH_cur_f - SERVO_MID) * EYE_LEFT_GAIN  + SERVO_MID + EYE_LEFT_OFFSET),  LEFT_MIN_X,  LEFT_MAX_X);
    int cmdVert  = constrain((int)round((eyeV_cur_f - SERVO_MID) * EYE_VERT_GAIN  + SERVO_MID + EYE_VERT_OFFSET),  VERT_MIN_Y,  VERT_MAX_Y);
    setServoAngle(SERVO_CHANNEL_RIGHT, cmdRight);
    setServoAngle(SERVO_CHANNEL_LEFT,  cmdLeft);
    setServoAngle(SERVO_CHANNEL_VERTICAL, cmdVert);

    // Cejas: espejo con stick derecho, PERO L1/R1 tienen prioridad para cada lado
    int16_t rx = ctl->axisRX();
    int delta = map(rx, -512, 512, -EYEBROW_RANGE, EYEBROW_RANGE);

    // Espejo: derecha = -delta, izquierda = +delta (ajustado a tu mecánica)
    int eyebrowRight  = constrain(EYEBROW_CENTER - delta, EYEBROW_MIN, EYEBROW_MAX);
    int eyebrowLeft   = constrain(EYEBROW_CENTER + delta, EYEBROW_MIN, EYEBROW_MAX);

    // Prioridad: L1 IZQ arriba, R1 DER arriba (si lo quieres invertido, cambia EYEBROW_UP/EYEBROW_DOWN)
    if (l1) eyebrowLeft  = EYEBROW_UP;
    if (r1) eyebrowRight = EYEBROW_UP;

    setServoAngle(SERVO_CHANNEL_EYEBROW_RIGHT, eyebrowRight);
    setServoAngle(SERVO_CHANNEL_EYEBROW_LEFT,  eyebrowLeft);
  }
  else { // MODE_DEVIL (manual)
    // Sólo ojos manuales
    int16_t rawX = ctl->axisX();
    int16_t rawY = ctl->axisY();
    int16_t joyX = applyDeadzoneSmooth(rawX, JOY_DEADZONE);
    int16_t joyY = applyDeadzoneSmooth(rawY, JOY_DEADZONE);
    float nx = cubicEase(joyX / 512.0f);
    float ny = cubicEase(joyY / 512.0f);

    float targetX = map(nx * 512.0f, -512, 512, SERVO_MIN_X, SERVO_MAX_X);
    float targetY = map(-ny * 512.0f, -512, 512, SERVO_MIN_Y, SERVO_MAX_Y);
    targetX = constrain((int)targetX, SERVO_MIN_X, SERVO_MAX_X);
    targetY = constrain((int)targetY, SERVO_MIN_Y, SERVO_MAX_Y);

    eyeH_cur_f = eyeH_cur_f + EYE_ALPHA * (targetX - eyeH_cur_f);
    eyeV_cur_f = eyeV_cur_f + EYE_ALPHA * (targetY - eyeV_cur_f);

    int cmdRight = constrain((int)round((eyeH_cur_f - SERVO_MID) * EYE_RIGHT_GAIN + SERVO_MID + EYE_RIGHT_OFFSET), RIGHT_MIN_X, RIGHT_MAX_X);
    int cmdLeft  = constrain((int)round((eyeH_cur_f - SERVO_MID) * EYE_LEFT_GAIN  + SERVO_MID + EYE_LEFT_OFFSET),  LEFT_MIN_X,  LEFT_MAX_X);
    int cmdVert  = constrain((int)round((eyeV_cur_f - SERVO_MID) * EYE_VERT_GAIN  + SERVO_MID + EYE_VERT_OFFSET),  VERT_MIN_Y,  VERT_MAX_Y);
    setServoAngle(SERVO_CHANNEL_RIGHT, cmdRight);   // ✅ correcto
    setServoAngle(SERVO_CHANNEL_LEFT,  cmdLeft);
    setServoAngle(SERVO_CHANNEL_VERTICAL, cmdVert);
  }

  // Guardar estado
  prevA[idx]  = a;
  prevB[idx]  = b;
  prevX[idx]  = x;
  prevY[idx]  = y;
  prevL1[idx] = l1;
  prevR1[idx] = r1;
  prevL2[idx] = l2;
  prevR2[idx] = r2;
  prevShare[idx] = share;
  prevDpad[idx] = dpad;
}

void processControllers() {
  for (auto ctl : myControllers) {
    if (ctl && ctl->isConnected() && ctl->hasData()) {
      if (ctl->isGamepad()) processGamepad(ctl);
    }
  }
}

// =======================
// 🚀 Setup principal
// =======================
void setup() {
  Serial.begin(115200);
  delay(1000);

  strip1.begin();
  strip2.begin();
  strip1.setBrightness(BRIGHTNESS);
  strip2.setBrightness(BRIGHTNESS);
  setNeopixelColor(0);

  Wire.begin();
  pca.begin();
  pca.setPWMFreq(50);  // Hz estándar para servos

  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);

  randomSeed(analogRead(0)); // semilla aleatoria

  // Inicializa color persistente y carrusel
  currentEyeColor = COLOR_CIAN;

  // Construimos el carrusel con todos los colores (incluye café miel)
  // Si quieres añadir/quitar, modifica aquí y sincroniza colorIndex:
  static uint32_t paletteLocal[] = {
    COLOR_CIAN, COLOR_ROJO, COLOR_VERDE, COLOR_BLANCO,
    COLOR_AMARILLO, COLOR_ROSA, COLOR_NARANJA, COLOR_AZUL, COLOR_CAFE_MIEL
  };
  // Copiar al arreglo global colorPalette (por si el compilador no permite asignación directa):
  memcpy(colorPalette, paletteLocal, sizeof(paletteLocal));
  // Ajusta colorIndex al color actual
  syncColorIndexToCurrent();

  Console.printf("🤖 Sistema Animatrónico listo.\n");
  Console.printf("🕹️ B=AUTO (stingers diablo), X=DIABLO manual, Y=NORMAL, A=COLOR (DPAD←/→ carrusel; incluye café miel).\n");
}

// =======================
// 🔁 Loop principal
// =======================
void loop() {
  bool dataUpdated = BP32.update();
  if (dataUpdated) {
    processControllers();
  } else {
    if (currentMode == MODE_AUTO) autoTick();
  }
  delay(15);
}