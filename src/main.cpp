#include <Arduino.h>
/*
  Cardputer ADV Frequency Generator  (v2)
  See README.md for the full manual. Press 'h' on the device for in-app help.
*/

#include <M5Cardputer.h>

struct Preset {
  float hz;
  const char* name;
  const char* desc;  // short description, wrapped to 2 lines (~37 chars each)
};

const Preset PRESETS[] = {
  {174.0f, "Solfeggio 174", "Traditionally linked to relaxation and easing tension."},
  {285.0f, "Solfeggio 285", "Traditionally linked to rest, renewal and recovery."},
  {396.0f, "Solfeggio 396", "Traditionally linked to releasing fear and guilt."},
  {417.0f, "Solfeggio 417", "Traditionally linked to change and letting go."},
  {528.0f, "Solfeggio 528", "The 'love' tone. Popular for calm and positivity."},
  {639.0f, "Solfeggio 639", "Traditionally linked to connection and harmony."},
  {741.0f, "Solfeggio 741", "Traditionally linked to clarity and expression."},
  {852.0f, "Solfeggio 852", "Traditionally linked to intuition and focus."},
  {963.0f, "Solfeggio 963", "Traditionally linked to awareness and stillness."},
  {432.0f, "Concert A 432", "Alternate tuning of A; many find it warmer."},
  {440.0f, "Concert A 440", "Standard concert pitch A4. Handy tuning reference."},
  {136.1f, "OM tone", "Tied to Earth's year; used in meditation and chanting."},
  {111.0f, "111 Hz", "Popular meditation tone, said to aid relaxation."},
  {7.83f,  "Schumann 7.83", "Earth's resonance. Too low for the built-in speaker."},
  {40.0f,  "Gamma 40", "Gamma brainwave range (focus). Too low for speaker."},
  {10.0f,  "Alpha 10", "Alpha range (relaxed wakefulness). Too low for speaker."},
  {6.0f,   "Theta 6", "Theta range (drowsy, meditative). Too low for speaker."},
  {2.0f,   "Delta 2", "Delta range (deep sleep). Too low for speaker."},
};
const int NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

const float STEPS[] = {0.1f, 1.0f, 10.0f, 100.0f, 1000.0f};
const int NUM_STEPS = sizeof(STEPS) / sizeof(STEPS[0]);
const int TIMERS[] = {0, 5, 10, 15, 30, 60};
const int NUM_TIMERS = sizeof(TIMERS) / sizeof(TIMERS[0]);

// ---- in-app help pages (max 9 lines, ~38 chars per line) ----
const int HELP_PAGES = 4;
const char* HELP_TITLES[HELP_PAGES] = {"KEYS", "PRESETS MODE", "CUSTOM MODE", "TIPS"};
const char* HELP_LINES[HELP_PAGES][9] = {
  {"Enter   play / stop",
   "Tab     Presets <-> Custom",
   "-  =    volume down / up",
   "w       sine / square wave",
   "t       auto-stop timer",
   "r       rotate screen 180",
   "h       this help",
   nullptr, nullptr},
  {";       previous preset (up)",
   ".       next preset (down)",
   "Enter   play / stop the tone",
   "",
   "Changing preset while playing",
   "switches the tone instantly.",
   nullptr, nullptr, nullptr},
  {"0-9 .   type a frequency in Hz",
   "Del     backspace",
   ";       frequency + step",
   ",       frequency - step",
   "/       change step size",
   "        (0.1/1/10/100/1000 Hz)",
   "Range: 0.1 - 20000 Hz",
   nullptr, nullptr},
  {"Speaker is clearest 200Hz-8kHz.",
   "Below ~100 Hz it is very faint or",
   "silent: use an external amp or",
   "headphones for low presets.",
   "",
   "Sine is smoother; square is louder.",
   "Use the timer for long sessions.",
   nullptr, nullptr},
};

M5Canvas canvas(&M5Cardputer.Display);
int W = 240, H = 135;
int rot = 1;
uint16_t C_HDR, C_SEL, C_DIM, C_ACC, C_BOX;

enum Mode { MODE_PRESETS, MODE_CUSTOM };
Mode mode = MODE_PRESETS;

int presetIdx = 0;
String customBuf = "432";
float curHz = 174.0f;
int stepIdx = 1;
int timerIdx = 0;
bool playing = false;
bool sine = true;
int volume = 120;  // 0-255
unsigned long playStartMs = 0;
bool helpOn = false;
int helpPage = 0;

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

// Pick a landscape rotation (wider than tall) whatever the panel's native orientation.
void pickLandscape() {
  const int tryRot[4] = {1, 3, 0, 2};
  for (int i = 0; i < 4; i++) {
    M5Cardputer.Display.setRotation(tryRot[i]);
    if (M5Cardputer.Display.width() > M5Cardputer.Display.height()) {
      rot = tryRot[i];
      return;
    }
  }
}

void drawWrapped(const char* text, int x, int y, int maxChars, int lineH, int maxLines) {
  String s = text;
  int line = 0;
  while (s.length() > 0 && line < maxLines) {
    if ((int)s.length() > maxChars) {
      int cut = s.lastIndexOf(' ', maxChars);
      if (cut <= 0) cut = maxChars;
      canvas.drawString(s.substring(0, cut), x, y + line * lineH);
      s = s.substring(cut + 1);
    } else {
      canvas.drawString(s, x, y + line * lineH);
      s = "";
    }
    line++;
  }
}

void drawHeader(const char* tag, const char* right) {
  canvas.fillRect(0, 0, W, 16, C_HDR);
  canvas.setTextSize(1);
  canvas.setTextColor(C_ACC);
  canvas.drawString(tag, 6, 4);
  canvas.setTextColor(WHITE);
  canvas.drawRightString(right, W - 6, 4);
}

void draw() {
  canvas.fillSprite(BLACK);

  char hdr[40];
  snprintf(hdr, sizeof(hdr), "%s  vol %d%%  h:help", sine ? "sine" : "square", volume * 100 / 255);
  drawHeader(mode == MODE_PRESETS ? "PRESETS" : "CUSTOM", hdr);

  if (mode == MODE_PRESETS) {
    const int ROWS = 4, ROW_H = 13, TOP = 19;
    int first = presetIdx - 1;
    if (first > NUM_PRESETS - ROWS) first = NUM_PRESETS - ROWS;
    if (first < 0) first = 0;

    canvas.setTextSize(1);
    for (int i = 0; i < ROWS && first + i < NUM_PRESETS; i++) {
      int idx = first + i;
      int y = TOP + i * ROW_H;
      if (idx == presetIdx) {
        canvas.fillRect(0, y - 2, W - 8, ROW_H, C_SEL);
        canvas.setTextColor(YELLOW);
      } else {
        canvas.setTextColor(WHITE);
      }
      canvas.drawString(PRESETS[idx].name, 6, y);
      char hz[16];
      snprintf(hz, sizeof(hz), "%g Hz", PRESETS[idx].hz);
      canvas.drawRightString(hz, W - 12, y);
    }

    // scrollbar
    int trackH = ROWS * ROW_H;
    int thumbH = max(6, trackH * ROWS / NUM_PRESETS);
    int thumbY = TOP - 2 + (trackH - thumbH) * presetIdx / max(1, NUM_PRESETS - 1);
    canvas.fillRect(W - 5, TOP - 2, 3, trackH, C_BOX);
    canvas.fillRect(W - 5, thumbY, 3, thumbH, C_ACC);

    // description box
    canvas.drawRoundRect(2, 74, W - 4, 28, 4, C_DIM);
    canvas.setTextColor(LIGHTGREY);
    drawWrapped(PRESETS[presetIdx].desc, 7, 78, 37, 10, 2);
  } else {
    canvas.setTextSize(1);
    canvas.setTextColor(C_DIM);
    canvas.drawString("Frequency", 6, 21);

    canvas.setTextSize(3);
    canvas.setTextColor(YELLOW);
    String shown = customBuf + "_";
    canvas.drawString(shown, 6, 33);
    canvas.setTextSize(2);
    canvas.setTextColor(WHITE);
    canvas.drawString("Hz", 6 + 18 * (int)shown.length() + 4, 41);

    canvas.setTextSize(1);
    char st[40];
    snprintf(st, sizeof(st), "step: %g Hz   (/ to change)", STEPS[stepIdx]);
    canvas.setTextColor(LIGHTGREY);
    canvas.drawString(st, 6, 66);
    canvas.setTextColor(C_DIM);
    canvas.drawString(";  +step      ,  -step", 6, 80);
    canvas.drawString("0-9 .  type      Del  backspace", 6, 91);
  }

  // footer
  canvas.drawFastHLine(0, 104, W, C_BOX);
  canvas.setTextSize(2);
  if (playing) {
    canvas.setTextColor(GREEN);
    char p[32];
    snprintf(p, sizeof(p), "> %g Hz", curHz);
    canvas.drawString(p, 4, 107);
  } else {
    canvas.setTextColor(RED);
    canvas.drawString("stopped", 4, 107);
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
  canvas.drawRightString(tm, W - 6, 108);

  float shownHz = (mode == MODE_PRESETS) ? PRESETS[presetIdx].hz : clampHz(customBuf.toFloat());
  if (shownHz < 100.0f) {
    canvas.setTextColor(ORANGE);
    canvas.drawString("Too low for built-in speaker", 4, 125);
  }

  canvas.pushSprite(0, 0);
}

void drawHelp() {
  canvas.fillSprite(BLACK);
  char title[40];
  snprintf(title, sizeof(title), "HELP %d/%d  %s", helpPage + 1, HELP_PAGES, HELP_TITLES[helpPage]);
  drawHeader(title, "");

  canvas.setTextSize(1);
  canvas.setTextColor(WHITE);
  for (int i = 0; i < 9; i++) {
    const char* line = HELP_LINES[helpPage][i];
    if (line == nullptr) break;
    canvas.drawString(line, 6, 21 + i * 11);
  }
  canvas.drawFastHLine(0, 121, W, C_BOX);
  canvas.setTextColor(C_DIM);
  canvas.drawString(";/. page    any other key: close", 6, 124);
  canvas.pushSprite(0, 0);
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  pickLandscape();
  W = M5Cardputer.Display.width();
  H = M5Cardputer.Display.height();

  canvas.createSprite(W, H);
  canvas.setTextDatum(top_left);

  C_HDR = M5Cardputer.Display.color565(24, 48, 96);
  C_SEL = M5Cardputer.Display.color565(30, 50, 90);
  C_DIM = M5Cardputer.Display.color565(140, 140, 150);
  C_ACC = M5Cardputer.Display.color565(80, 200, 255);
  C_BOX = M5Cardputer.Display.color565(50, 50, 60);

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
  if (c == 'r' || c == 'R') {
    rot = (rot + 2) % 4;
    M5Cardputer.Display.setRotation(rot);
    return;
  }
  if (c == 'h' || c == 'H') {
    helpOn = true;
    helpPage = 0;
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

    if (helpOn) {
      bool nav = false;
      for (char c : st.word) {
        if (c == ';') {
          helpPage = (helpPage + HELP_PAGES - 1) % HELP_PAGES;
          nav = true;
        } else if (c == '.') {
          helpPage = (helpPage + 1) % HELP_PAGES;
          nav = true;
        }
      }
      if (!nav) helpOn = false;
    } else {
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
  }

  // auto-stop timer
  if (playing && TIMERS[timerIdx] > 0 &&
      millis() - playStartMs >= (unsigned long)TIMERS[timerIdx] * 60000UL) {
    stopTone();
  }

  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 100) {
    lastDraw = millis();
    if (helpOn) drawHelp(); else draw();
  }
}
