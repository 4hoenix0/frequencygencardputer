#include <Arduino.h>
/*
  Cardputer ADV Frequency Generator
  ---------------------------------
  Library: M5Cardputer (install via Arduino Library Manager; pulls in M5Unified + M5GFX)
  Board:   M5Stack -> M5Cardputer  (ESP32-S3)

  Keys
    Tab ........ switch Presets / Custom
    Enter ...... play / stop
    - and = .... volume down / up
    w .......... waveform: sine / square
    t .......... auto-stop timer: off / 5 / 10 / 15 / 30 / 60 min

  Presets mode
    ;  (up arrow)   previous preset
    .  (down arrow) next preset

  Custom mode
    0-9 and .   type a frequency in Hz
    Del         backspace
    ;           frequency + step
    ,           frequency - step
    /           cycle step size: 0.1 / 1 / 10 / 100 / 1000 Hz
*/

#include <M5Cardputer.h>

struct Preset {
  float hz;
  const char* name;
};

const Preset PRESETS[] = {
  {174.0f,  "Solfeggio 174"},
  {285.0f,  "Solfeggio 285"},
  {396.0f,  "Solfeggio 396"},
  {417.0f,  "Solfeggio 417"},
  {528.0f,  "Solfeggio 528"},
  {639.0f,  "Solfeggio 639"},
  {741.0f,  "Solfeggio 741"},
  {852.0f,  "Solfeggio 852"},
  {963.0f,  "Solfeggio 963"},
  {432.0f,  "Concert A 432"},
  {440.0f,  "Concert A 440"},
  {136.1f,  "OM (Earth year)"},
  {111.0f,  "111 Hz"},
  {7.83f,   "Schumann 7.83*"},
  {40.0f,   "Gamma 40*"},
  {10.0f,   "Alpha 10*"},
  {6.0f,    "Theta 6*"},
  {2.0f,    "Delta 2*"},
};
const int NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

const float STEPS[] = {0.1f, 1.0f, 10.0f, 100.0f, 1000.0f};
const int NUM_STEPS = sizeof(STEPS) / sizeof(STEPS[0]);
const int TIMERS[] = {0, 5, 10, 15, 30, 60};
const int NUM_TIMERS = sizeof(TIMERS) / sizeof(TIMERS[0]);

M5Canvas canvas(&M5Cardputer.Display);

enum Mode { MODE_PRESETS, MODE_CUSTOM };
Mode mode = MODE_PRESETS;

int presetIdx = 0;
String customBuf = "432";
float curHz = 528.0f;
int stepIdx = 1;
int timerIdx = 0;
bool playing = false;
bool sine = true;
int volume = 120;  // 0-255
unsigned long playStartMs = 0;

uint8_t sineTable[64];

float clampHz(float hz) {
  if (hz < 0.1f) hz = 0.1f;
  if (hz > 20000.0f) hz = 20000.0f;
  return hz;
}

void startTone() {
  M5Cardputer.Speaker.stop();
  curHz = clampHz(curHz);
  M5Cardputer.Speaker.setVolume(volume);
  if (sine) {
    M5Cardputer.Speaker.tone(curHz, ~0u, 0, true, sineTable, sizeof(sineTable));
  } else {
    M5Cardputer.Speaker.tone(curHz, ~0u, 0, true);
  }
  playing = true;
  playStartMs = millis();
}

void stopTone() {
  M5Cardputer.Speaker.stop();
  playing = false;
}

void restartIfPlaying() {
  if (playing) {
    unsigned long keep = playStartMs;
    startTone();
    playStartMs = keep;  // don't reset the session timer on tweaks
  }
}

void syncBufFromHz() {
  char b[16];
  snprintf(b, sizeof(b), "%g", curHz);
  customBuf = b;
}

void draw() {
  canvas.fillSprite(BLACK);
  canvas.setTextColor(WHITE);

  // header
  canvas.setTextSize(1);
  canvas.setTextColor(CYAN);
  canvas.drawString(mode == MODE_PRESETS ? "PRESETS" : "CUSTOM", 4, 4);
  canvas.setTextColor(LIGHTGREY);
  char hdr[40];
  snprintf(hdr, sizeof(hdr), "%s  vol %d%%", sine ? "sine" : "sqr", volume * 100 / 255);
  canvas.drawRightString(hdr, 236, 4);
  canvas.drawFastHLine(0, 16, 240, DARKGREY);

  if (mode == MODE_PRESETS) {
    // scrolling list, 5 rows
    int first = presetIdx - 2;
    if (first < 0) first = 0;
    if (first > NUM_PRESETS - 5) first = NUM_PRESETS - 5;
    if (first < 0) first = 0;
    for (int i = 0; i < 5 && first + i < NUM_PRESETS; i++) {
      int idx = first + i;
      int y = 22 + i * 16;
      char line[48];
      snprintf(line, sizeof(line), "%-16s %7g Hz", PRESETS[idx].name, PRESETS[idx].hz);
      if (idx == presetIdx) {
        canvas.fillRect(0, y - 2, 240, 15, 0x2104);
        canvas.setTextColor(YELLOW);
      } else {
        canvas.setTextColor(WHITE);
      }
      canvas.drawString(line, 6, y);
    }
  } else {
    canvas.setTextColor(LIGHTGREY);
    canvas.drawString("Type Hz, ; up  , down  / step", 4, 22);
    canvas.setTextSize(3);
    canvas.setTextColor(YELLOW);
    String shown = customBuf + "_";
    canvas.drawString(shown, 6, 42);
    canvas.setTextSize(1);
    canvas.setTextColor(WHITE);
    canvas.drawString("Hz", 6 + 18 * (shown.length()) + 6, 58);
    char st[32];
    snprintf(st, sizeof(st), "step: %g Hz", STEPS[stepIdx]);
    canvas.setTextColor(LIGHTGREY);
    canvas.drawString(st, 6, 80);
  }

  // footer
  canvas.drawFastHLine(0, 104, 240, DARKGREY);
  canvas.setTextSize(2);
  if (playing) {
    canvas.setTextColor(GREEN);
    char p[32];
    snprintf(p, sizeof(p), "> %g Hz", curHz);
    canvas.drawString(p, 4, 110);
  } else {
    canvas.setTextColor(RED);
    canvas.drawString("stopped", 4, 110);
  }
  canvas.setTextSize(1);
  canvas.setTextColor(LIGHTGREY);
  char tm[32];
  if (TIMERS[timerIdx] == 0) {
    snprintf(tm, sizeof(tm), "timer off");
  } else if (playing) {
    long left = (long)TIMERS[timerIdx] * 60 - (long)((millis() - playStartMs) / 1000);
    if (left < 0) left = 0;
    snprintf(tm, sizeof(tm), "%ld:%02ld left", left / 60, left % 60);
  } else {
    snprintf(tm, sizeof(tm), "timer %d min", TIMERS[timerIdx]);
  }
  canvas.drawRightString(tm, 236, 112);
  if (curHz < 60.0f || (mode == MODE_PRESETS && PRESETS[presetIdx].hz < 60.0f)) {
    canvas.setTextColor(ORANGE);
    canvas.drawRightString("*speaker can't reproduce", 236, 124);
  }

  canvas.pushSprite(0, 0);
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  canvas.createSprite(240, 135);
  canvas.setTextDatum(top_left);

  for (int i = 0; i < 64; i++) {
    sineTable[i] = (uint8_t)(128 + 127 * sinf(2.0f * PI * i / 64.0f));
  }
  M5Cardputer.Speaker.setVolume(volume);
  curHz = PRESETS[presetIdx].hz;
  draw();
}

void handleChar(char c) {
  if (c == '-') {
    volume = max(0, volume - 15);
    M5Cardputer.Speaker.setVolume(volume);
    return;
  }
  if (c == '=') {
    volume = min(255, volume + 15);
    M5Cardputer.Speaker.setVolume(volume);
    return;
  }
  if (c == 'w' || c == 'W') {
    sine = !sine;
    restartIfPlaying();
    return;
  }
  if (c == 't' || c == 'T') {
    timerIdx = (timerIdx + 1) % NUM_TIMERS;
    if (playing) playStartMs = millis();
    return;
  }

  if (mode == MODE_PRESETS) {
    if (c == ';') {
      presetIdx = (presetIdx - 1 + NUM_PRESETS) % NUM_PRESETS;
      curHz = PRESETS[presetIdx].hz;
      restartIfPlaying();
    } else if (c == '.') {
      presetIdx = (presetIdx + 1) % NUM_PRESETS;
      curHz = PRESETS[presetIdx].hz;
      restartIfPlaying();
    }
  } else {
    if ((c >= '0' && c <= '9') || c == '.') {
      if (c == '.' && customBuf.indexOf('.') >= 0) return;
      if (customBuf.length() < 9) customBuf += c;
      curHz = clampHz(customBuf.toFloat());
      restartIfPlaying();
    } else if (c == ';') {
      curHz = clampHz(customBuf.toFloat() + STEPS[stepIdx]);
      syncBufFromHz();
      restartIfPlaying();
    } else if (c == ',') {
      curHz = clampHz(customBuf.toFloat() - STEPS[stepIdx]);
      syncBufFromHz();
      restartIfPlaying();
    } else if (c == '/') {
      stepIdx = (stepIdx + 1) % NUM_STEPS;
    }
  }
}

void loop() {
  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    auto st = M5Cardputer.Keyboard.keysState();

    if (st.tab) {
      mode = (mode == MODE_PRESETS) ? MODE_CUSTOM : MODE_PRESETS;
      if (mode == MODE_PRESETS) {
        curHz = PRESETS[presetIdx].hz;
      } else {
        curHz = clampHz(customBuf.toFloat());
      }
      restartIfPlaying();
    }
    if (st.del && mode == MODE_CUSTOM && customBuf.length() > 0) {
      customBuf.remove(customBuf.length() - 1);
      curHz = clampHz(customBuf.toFloat());
      restartIfPlaying();
    }
    if (st.enter) {
      if (playing) {
        stopTone();
      } else {
        curHz = (mode == MODE_PRESETS) ? PRESETS[presetIdx].hz : clampHz(customBuf.toFloat());
        startTone();
      }
    }
    for (char c : st.word) handleChar(c);
  }

  // auto-stop timer
  if (playing && TIMERS[timerIdx] > 0 &&
      millis() - playStartMs >= (unsigned long)TIMERS[timerIdx] * 60000UL) {
    stopTone();
  }

  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 100) {
    lastDraw = millis();
    draw();
  }
}
