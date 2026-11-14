#include <Arduino.h>
/*
  Cardputer ADV Frequency + Noise Generator  (v3)
  See README.md for the full manual. Press 'h' on the device for in-app help.

  Audio is generated in software (16-bit, 48 kHz) on its own task, with fixed
  headroom, a smooth gain ramp (no clicks) and a soft limiter on noise. This
  avoids the crackling that came from running the output at full scale.

  Tunables (if you still hear distortion, lower these):
    HW_VOLUME     codec/speaker hardware volume (0-255)
    MAX_GAIN_SPK  top software level in speaker mode (0-1)
    MAX_GAIN_HP   top software level in headphone mode (0-1)
*/

#include <M5Cardputer.h>
#include <math.h>

// ---------------- audio tunables ----------------
static const uint8_t HW_VOLUME = 192;
static const float MAX_GAIN_SPK = 1.0f;
static const float MAX_GAIN_HP = 0.5f;
static const float TONE_AMP_SINE = 0.5f;
static const float TONE_AMP_SQUARE = 0.3f;
static const uint32_t SR = 48000;
static const size_t BUF_LEN = 1024;

// Anything at or below this rate is too low to hear as a tone, so it is played
// as isochronic pulses: an audible carrier switched on/off at that rate.
static const float PULSE_MAX_HZ = 40.5f;
static const float CARRIERS[] = {150.0f, 200.0f, 250.0f, 300.0f, 400.0f};
static const int NUM_CARRIERS = sizeof(CARRIERS) / sizeof(CARRIERS[0]);

// ---------------- data ----------------
struct Preset {
  float hz;
  const char* name;
  const char* desc;  // wrapped to 2 lines (~37 chars each)
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
  // Rates at or below PULSE_MAX_HZ play as pulses of a carrier tone (isochronic).
  {2.0f,   "Delta 2", "Delta (deep sleep). Plays as slow pulses, 2 per second."},
  {6.0f,   "Theta 6", "Theta (drowsy, meditative). Pulses, 6 per second."},
  {7.83f,  "Schumann 7.83", "Earth's resonance. Soft pulses, 7.83 per second."},
  {10.0f,  "Alpha 10", "Alpha (relaxed wakefulness). Pulses, 10 per second."},
  {20.0f,  "Beta 20", "Beta (alert, active focus). Pulses, 20 per second."},
  {40.0f,  "Gamma 40", "Gamma (high focus). Pulses, 40 per second."},
};
const int NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

struct NoiseDef {
  const char* name;
  const char* shortName;
  const char* tag;   // spectrum shape, shown at the right of the list
  const char* desc;
};

// order must match noiseSample() types 0..6
const NoiseDef NOISES[] = {
  {"White noise",  "White",  "flat",       "Equal energy at all pitches. Bright hiss; masks sound."},
  {"Pink noise",   "Pink",   "-3 dB/oct",  "Softer than white; like steady rain. Good for focus/sleep."},
  {"Brown noise",  "Brown",  "-6 dB/oct",  "Deep rumble; like thunder or a waterfall. Calming."},
  {"Green noise",  "Green",  "~500 Hz",    "Mid-range around 500 Hz; natural, like forest or stream."},
  {"Blue noise",   "Blue",   "+3 dB/oct",  "Bright and hissy; stronger highs than pink. Can be sharp."},
  {"Violet noise", "Violet", "+6 dB/oct",  "Very bright, mostly high pitches. Can be harsh."},
  {"Grey noise",   "Grey",   "even",       "Approx. tuned to hearing: lows/highs boosted, mids softer."},
};
const int NUM_NOISES = sizeof(NOISES) / sizeof(NOISES[0]);

const float STEPS[] = {0.1f, 1.0f, 10.0f, 100.0f, 1000.0f};
const int NUM_STEPS = sizeof(STEPS) / sizeof(STEPS[0]);
const int TIMERS[] = {0, 5, 10, 15, 30, 60};
const int NUM_TIMERS = sizeof(TIMERS) / sizeof(TIMERS[0]);

// ---- in-app help pages (max 9 lines, ~38 chars per line) ----
const int HELP_PAGES = 4;
const char* HELP_TITLES[HELP_PAGES] = {"KEYS", "PRESETS & NOISE", "CUSTOM MODE", "TIPS"};
const char* HELP_LINES[HELP_PAGES][9] = {
  {"Enter   play / stop",
   "Tab     Presets > Custom > Noise",
   "-  =    volume down / up (0-10)",
   "o       output: spk / hp (hp = lower",
   "        limit, avoids crackle)",
   "w       sine / square wave",
   "t       auto-stop timer",
   "r       rotate screen    h  help",
   "c       pulse carrier pitch"},
  {";       previous (up)",
   ".       next (down)",
   "Enter   play / stop",
   "",
   "Changing selection while playing",
   "switches the sound instantly.",
   "Noise: white pink brown green",
   "blue violet grey",
   nullptr},
  {"0-9 .   type a frequency in Hz",
   "Del     backspace",
   ";       frequency + step",
   ",       frequency - step",
   "/       change step size",
   "        (0.1/1/10/100/1000 Hz)",
   "Range: 0.1 - 20000 Hz",
   nullptr, nullptr},
  {"Speaker is clearest 200 Hz - 8 kHz.",
   "Rates up to 40 Hz are played as",
   "pulses of a carrier tone (c = its",
   "pitch). Best on headphones; keep",
   "the volume low.",
   "",
   "Crackling? Press o for hp mode.",
   "Pulses: sine = soft, square = sharp."},
};

// ---------------- UI state ----------------
M5Canvas canvas(&M5Cardputer.Display);
int W = 240, H = 135;
int rot = 1;
uint16_t C_HDR, C_SEL, C_DIM, C_ACC, C_BOX;

enum Mode { MODE_PRESETS, MODE_CUSTOM, MODE_NOISE };
Mode mode = MODE_PRESETS;

int presetIdx = 0;
int noiseIdx = 1;
String customBuf = "432";
float curHz = 174.0f;
int stepIdx = 1;
int timerIdx = 0;
bool playing = false;
bool sine = true;
bool headphones = false;
int carrierIdx = 2;  // pulse carrier pitch (250 Hz)
int volume = 5;  // 0-10
unsigned long playStartMs = 0;
bool helpOn = false;
int helpPage = 0;

// ---------------- shared with audio task ----------------
volatile bool g_on = false;
volatile int g_src = 0;        // 0 = tone, 1..7 = noise type + 1
volatile float g_hz = 440.0f;
volatile float g_gain = 0.0f;  // 0..1 master level
volatile bool g_sine = true;
volatile float g_pulse = 0.0f;  // >0: pulse the tone at this rate (g_hz is then the carrier)

// ---------------- noise generators ----------------
struct NoiseState {
  float b0, b1, b2, b3, b4, b5, b6;
  float brown, pinkPrev, wPrev;
  float lp250, lp6k;
  float x1, x2, y1, y2;
  uint32_t rng;
};
static NoiseState ns;
// per-type scale so every noise has about the same loudness (RMS ~0.2)
static const float NOISE_SCALE[7] = {0.3462f, 0.1143f, 3.4794f, 1.6409f, 0.1911f, 0.2448f, 0.3105f};
static float BQ_B0, BQ_B2, BQ_A1, BQ_A2, A250, A6K;

void initNoise() {
  memset(&ns, 0, sizeof(ns));
  ns.rng = 0x1234ABCDu ^ (uint32_t)micros();
  if (ns.rng == 0) ns.rng = 1;
  // band-pass around 500 Hz for "green" noise
  const float w0 = 2.0f * (float)PI * 500.0f / (float)SR;
  const float alpha = sinf(w0) / (2.0f * 0.7f);
  BQ_B0 = alpha / (1.0f + alpha);
  BQ_B2 = -BQ_B0;
  BQ_A1 = -2.0f * cosf(w0) / (1.0f + alpha);
  BQ_A2 = (1.0f - alpha) / (1.0f + alpha);
  A250 = 1.0f - expf(-2.0f * (float)PI * 250.0f / (float)SR);
  A6K = 1.0f - expf(-2.0f * (float)PI * 6000.0f / (float)SR);
}

static inline float frand() {
  ns.rng ^= ns.rng << 13;
  ns.rng ^= ns.rng >> 17;
  ns.rng ^= ns.rng << 5;
  return (float)((int32_t)ns.rng) * (1.0f / 2147483648.0f);
}

static inline float softclip(float x) {
  if (x > 1.0f) x = 1.0f;
  if (x < -1.0f) x = -1.0f;
  return 1.5f * x - 0.5f * x * x * x;
}

float noiseSample(int type) {
  float w = frand();
  float v;
  switch (type) {
    case 0:
      v = w;
      break;
    case 1:
    case 4: {
      ns.b0 = 0.99886f * ns.b0 + w * 0.0555179f;
      ns.b1 = 0.99332f * ns.b1 + w * 0.0750759f;
      ns.b2 = 0.96900f * ns.b2 + w * 0.1538520f;
      ns.b3 = 0.86650f * ns.b3 + w * 0.3104856f;
      ns.b4 = 0.55000f * ns.b4 + w * 0.5329522f;
      ns.b5 = -0.7616f * ns.b5 - w * 0.0168980f;
      float pink = ns.b0 + ns.b1 + ns.b2 + ns.b3 + ns.b4 + ns.b5 + ns.b6 + w * 0.5362f;
      ns.b6 = w * 0.115926f;
      v = (type == 1) ? pink : (pink - ns.pinkPrev);  // blue = differentiated pink
      ns.pinkPrev = pink;
      break;
    }
    case 2:
      ns.brown = (ns.brown + 0.02f * w) / 1.02f;
      v = ns.brown;
      break;
    case 3: {
      float y = BQ_B0 * w + BQ_B2 * ns.x2 - BQ_A1 * ns.y1 - BQ_A2 * ns.y2;
      ns.x2 = ns.x1;
      ns.x1 = w;
      ns.y2 = ns.y1;
      ns.y1 = y;
      v = y;
      break;
    }
    case 5:
      v = w - ns.wPrev;  // violet = differentiated white
      break;
    default:
      ns.lp250 += A250 * (w - ns.lp250);
      ns.lp6k += A6K * (w - ns.lp6k);
      v = 0.6f * w + 3.0f * ns.lp250 + 0.8f * (w - ns.lp6k);
      break;
  }
  ns.wPrev = w;
  return softclip(v * NOISE_SCALE[type]);
}

// ---------------- audio task ----------------
static int16_t audioBuf[3][BUF_LEN];

void audioTask(void*) {
  int idx = 0;
  float phase = 0.0f;
  float mphase = 0.0f;  // pulse (modulation) phase
  float gain = 0.0f;
  bool pending = false;

  for (;;) {
    bool on = g_on;
    if (!on && gain < 0.0005f && !pending) {
      gain = 0.0f;
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    if (!pending) {
      const int src = g_src;
      const float hz = g_hz;
      const float target = on ? g_gain : 0.0f;
      const bool isSine = g_sine;
      const float pulseHz = g_pulse;
      const bool pulsing = (src == 0 && pulseHz > 0.0f);
      const float inc = hz / (float)SR;
      const float minc = pulseHz / (float)SR;
      const float amp = (isSine || pulsing) ? TONE_AMP_SINE : TONE_AMP_SQUARE;

      for (size_t i = 0; i < BUF_LEN; i++) {
        gain += (target - gain) * 0.003f;  // ~7 ms smoothing: no clicks, no pops
        float s;
        if (src == 0) {
          phase += inc;
          if (phase >= 1.0f) phase -= 1.0f;
          if (pulsing) {
            // sine carrier, amplitude-modulated at the pulse rate (isochronic tone)
            mphase += minc;
            if (mphase >= 1.0f) mphase -= 1.0f;
            float env;
            if (isSine) {
              env = 0.5f * (1.0f - cosf(2.0f * (float)PI * mphase));  // soft swell
            } else {
              env = 0.5f + 2.5f * sinf(2.0f * (float)PI * mphase);    // sharper on/off
              if (env < 0.0f) env = 0.0f;
              if (env > 1.0f) env = 1.0f;
            }
            s = sinf(2.0f * (float)PI * phase) * amp * env;
          } else if (isSine) {
            s = sinf(2.0f * (float)PI * phase) * amp;
          } else {
            s = (phase < 0.5f ? amp : -amp);
          }
        } else {
          s = noiseSample(src - 1);
        }
        float out = s * gain * 32000.0f;
        if (out > 32000.0f) out = 32000.0f;
        if (out < -32000.0f) out = -32000.0f;
        audioBuf[idx][i] = (int16_t)out;
      }
      pending = true;
    }

    if (M5Cardputer.Speaker.isPlaying(0) == 2) {
      vTaskDelay(1);
      continue;
    }
    if (M5Cardputer.Speaker.playRaw(audioBuf[idx], BUF_LEN, SR, false, 1, 0, false)) {
      idx = (idx + 1) % 3;
      pending = false;
    } else {
      vTaskDelay(1);
    }
  }
}

// ---------------- control helpers ----------------
float clampHz(float hz) {
  if (hz < 0.1f) hz = 0.1f;
  if (hz > 20000.0f) hz = 20000.0f;
  return hz;
}

void applyGain() {
  float maxG = headphones ? MAX_GAIN_HP : MAX_GAIN_SPK;
  float v = volume / 10.0f;
  g_gain = maxG * v * v;
}

void applySource() {
  if (mode == MODE_NOISE) {
    g_pulse = 0.0f;
    g_src = noiseIdx + 1;
  } else {
    float hz = clampHz(curHz);
    if (hz <= PULSE_MAX_HZ) {
      g_pulse = hz;                 // too low to hear: pulse a carrier at this rate
      g_hz = CARRIERS[carrierIdx];
    } else {
      g_pulse = 0.0f;
      g_hz = hz;
    }
    g_src = 0;
  }
  g_sine = sine;
  applyGain();
}

void startTone() {
  applySource();
  g_on = true;
  playing = true;
  playStartMs = millis();
}

void stopTone() {
  g_on = false;
  playing = false;
}

void restartIfPlaying() {
  if (playing) applySource();  // live change; session timer keeps running
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

// ---------------- drawing ----------------
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

  const char* tag = (mode == MODE_PRESETS) ? "PRESETS" : (mode == MODE_CUSTOM) ? "CUSTOM" : "NOISE";
  char hdr[40];
  snprintf(hdr, sizeof(hdr), "%s  v%d/10  %s  h:help", sine ? "sine" : "square", volume,
           headphones ? "hp" : "spk");
  drawHeader(tag, hdr);

  if (mode == MODE_PRESETS || mode == MODE_NOISE) {
    const bool isPre = (mode == MODE_PRESETS);
    const int count = isPre ? NUM_PRESETS : NUM_NOISES;
    const int sel = isPre ? presetIdx : noiseIdx;
    const int ROWS = 4, ROW_H = 13, TOP = 19;

    int first = sel - 1;
    if (first > count - ROWS) first = count - ROWS;
    if (first < 0) first = 0;

    canvas.setTextSize(1);
    for (int i = 0; i < ROWS && first + i < count; i++) {
      int idx = first + i;
      int y = TOP + i * ROW_H;
      if (idx == sel) {
        canvas.fillRect(0, y - 2, W - 8, ROW_H, C_SEL);
        canvas.setTextColor(YELLOW);
      } else {
        canvas.setTextColor(WHITE);
      }
      char right[20];
      if (isPre) {
        canvas.drawString(PRESETS[idx].name, 6, y);
        snprintf(right, sizeof(right), "%g Hz", PRESETS[idx].hz);
      } else {
        canvas.drawString(NOISES[idx].name, 6, y);
        snprintf(right, sizeof(right), "%s", NOISES[idx].tag);
      }
      canvas.drawRightString(right, W - 12, y);
    }

    // scrollbar
    int trackH = ROWS * ROW_H;
    int thumbH = max(6, trackH * ROWS / count);
    int thumbY = TOP - 2 + (trackH - thumbH) * sel / max(1, count - 1);
    canvas.fillRect(W - 5, TOP - 2, 3, trackH, C_BOX);
    canvas.fillRect(W - 5, thumbY, 3, thumbH, C_ACC);

    // description box
    canvas.drawRoundRect(2, 74, W - 4, 28, 4, C_DIM);
    canvas.setTextColor(LIGHTGREY);
    drawWrapped(isPre ? PRESETS[presetIdx].desc : NOISES[noiseIdx].desc, 7, 78, 37, 10, 2);
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
    if (mode == MODE_NOISE) {
      snprintf(p, sizeof(p), "> %s", NOISES[noiseIdx].shortName);
    } else {
      snprintf(p, sizeof(p), "> %g Hz", curHz);
    }
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

  if (mode != MODE_NOISE) {
    float shownHz = (mode == MODE_PRESETS) ? PRESETS[presetIdx].hz : clampHz(customBuf.toFloat());
    if (shownHz <= PULSE_MAX_HZ) {
      char pm[40];
      snprintf(pm, sizeof(pm), "Pulsed on %g Hz tone (c: pitch)", CARRIERS[carrierIdx]);
      canvas.setTextColor(C_ACC);
      canvas.drawString(pm, 4, 125);
    } else if (shownHz < 100.0f) {
      canvas.setTextColor(ORANGE);
      canvas.drawString("Too low for built-in speaker", 4, 125);
    }
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

// ---------------- setup / input ----------------
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

  initNoise();
  curHz = PRESETS[presetIdx].hz;
  applySource();

  M5Cardputer.Speaker.setVolume(HW_VOLUME);
  xTaskCreatePinnedToCore(audioTask, "audio", 6144, nullptr, 3, nullptr, 0);
  draw();
}

void handleChar(char c) {
  if (c == '-') {
    if (volume > 0) volume--;
    applyGain();
    return;
  }
  if (c == '=') {
    if (volume < 10) volume++;
    applyGain();
    return;
  }
  if (c == 'o' || c == 'O') {
    headphones = !headphones;
    applyGain();
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
  if (c == 'c' || c == 'C') {
    carrierIdx = (carrierIdx + 1) % NUM_CARRIERS;
    restartIfPlaying();
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
  } else if (mode == MODE_NOISE) {
    if (c == ';') {
      noiseIdx = (noiseIdx - 1 + NUM_NOISES) % NUM_NOISES;
      restartIfPlaying();
    } else if (c == '.') {
      noiseIdx = (noiseIdx + 1) % NUM_NOISES;
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
        mode = (Mode)((mode + 1) % 3);
        if (mode == MODE_PRESETS) {
          curHz = PRESETS[presetIdx].hz;
        } else if (mode == MODE_CUSTOM) {
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
          if (mode == MODE_PRESETS) curHz = PRESETS[presetIdx].hz;
          else if (mode == MODE_CUSTOM) curHz = clampHz(customBuf.toFloat());
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
