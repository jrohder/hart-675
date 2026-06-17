#include "LedManager.h"
#include <math.h>

// LEDC channels
#define CH_RGB_R 0
#define CH_RGB_G 1
#define CH_RGB_B 2
#define CH_HART_R 3
#define CH_HART_G 4

LedManager::LedManager()
    : state(LED_STATE_IDLE), brightnessPct(DEFAULT_LED_BRIGHTNESS),
      batteryOverride(false), batteryStart(0), animMark(0), flashOn(false),
      hartPulsing(false), hartPulseRed(false), hartPulseStart(0) {}

void LedManager::begin() {
  ledcSetup(CH_RGB_R, 5000, 8);
  ledcSetup(CH_RGB_G, 5000, 8);
  ledcSetup(CH_RGB_B, 5000, 8);
  ledcSetup(CH_HART_R, 5000, 8);
  ledcSetup(CH_HART_G, 5000, 8);
  ledcAttachPin(RGB_LED_RED, CH_RGB_R);
  ledcAttachPin(RGB_LED_GREEN, CH_RGB_G);
  ledcAttachPin(RGB_LED_BLUE, CH_RGB_B);
  ledcAttachPin(HART_LED_RED, CH_HART_R);
  ledcAttachPin(HART_LED_GREEN, CH_HART_G);

  applyRgb(0, 0, 0);
  applyHart(false, false);
}

void LedManager::runSelfTest() {
  applyRgb(255, 0, 0);
  delay(300);
  applyRgb(0, 255, 0);
  delay(300);
  applyRgb(0, 0, 255);
  delay(300);
  applyRgb(0, 0, 0);
  applyHart(true, false);
  delay(250);
  applyHart(false, true);
  delay(250);
  applyHart(false, false);
}

void LedManager::setBrightnessPercent(uint8_t pct) {
  brightnessPct = (pct > 100) ? 100 : pct;
}

uint8_t LedManager::scale(uint8_t intensity) const {
  return (uint16_t)intensity * brightnessPct / 100;
}

void LedManager::applyRgb(uint8_t r, uint8_t g, uint8_t b) {
  // Common anode: 255 = OFF, 0 = full ON. Invert scaled intensity.
  ledcWrite(CH_RGB_R, 255 - scale(r));
  ledcWrite(CH_RGB_G, 255 - scale(g));
  ledcWrite(CH_RGB_B, 255 - scale(b));
}

void LedManager::applyHart(bool red, bool green) {
  ledcWrite(CH_HART_R, red ? (255 - scale(255)) : 255);
  ledcWrite(CH_HART_G, green ? (255 - scale(255)) : 255);
}

void LedManager::setState(LedState s) {
  if (state != s) {
    state = s;
    animMark = millis();
    flashOn = false;
  }
}

void LedManager::showBatteryStatus(uint8_t percentage) {
  batteryOverride = true;
  batteryStart = millis();
  uint8_t r = 0, g = 0, b = 0;
  if (percentage > 70) {
    b = 255;
  } else if (percentage >= 31) {
    g = 255;
  } else {
    r = 255;
  }
  applyRgb(r, g, b);
}

void LedManager::pulseHartTx() {
  hartPulsing = true;
  hartPulseRed = true;
  hartPulseStart = millis();
}

void LedManager::pulseHartRx() {
  hartPulsing = true;
  hartPulseRed = false;
  hartPulseStart = millis();
}

void LedManager::update() {
  unsigned long now = millis();

  // Battery override takes the RGB LED for 5 seconds.
  if (batteryOverride) {
    if (now - batteryStart >= LED_BATTERY_DISPLAY_MS) {
      batteryOverride = false;
      animMark = now;
    }
  } else {
    renderState();
  }

  // HART LED: red pulse on transmit, green pulse on received HART data.
  if (hartPulsing) {
    applyHart(hartPulseRed, !hartPulseRed);
    if (now - hartPulseStart >= LED_HART_PULSE_MS) {
      hartPulsing = false;
    }
  } else {
    applyHart(false, false);
  }
}

void LedManager::renderState() {
  unsigned long now = millis();

  switch (state) {
  case LED_STATE_USB:
    applyRgb(0, 0, 255);
    break;

  case LED_STATE_TCP:
    applyRgb(0, 255, 0);
    break;

  case LED_STATE_LOW_BATTERY:
    if (now - animMark >= LED_SLOW_FLASH_MS) {
      animMark = now;
      flashOn = !flashOn;
    }
    applyRgb(flashOn ? 255 : 0, 0, 0);
    break;

  case LED_STATE_FW_UPDATE:
    if (now - animMark >= LED_FW_FLASH_MS) {
      animMark = now;
      flashOn = !flashOn;
    }
    applyRgb(flashOn ? 255 : 0, flashOn ? 255 : 0, 0);  // yellow
    break;

  case LED_STATE_ERROR:
    if (now - animMark >= LED_FAST_FLASH_MS) {
      animMark = now;
      flashOn = !flashOn;
    }
    applyRgb(flashOn ? 255 : 0, 0, 0);
    break;

  case LED_STATE_IDLE:
  default: {
    // Breathing blue: triangle wave over LED_BREATHE_PERIOD_MS.
    unsigned long phase = (now - animMark) % LED_BREATHE_PERIOD_MS;
    float frac = (float)phase / LED_BREATHE_PERIOD_MS;          // 0..1
    float tri = frac < 0.5f ? (frac * 2.0f) : (2.0f - frac * 2.0f);  // 0..1..0
    uint8_t level = (uint8_t)(tri * 255.0f);
    applyRgb(0, 0, level);
    break;
  }
  }
}
