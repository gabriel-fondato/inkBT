#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// ---------- Button pins (change to whatever GPIOs you wire up) ----------
#define BTN_PLAY_PAUSE  25
#define BTN_NEXT        26
#define BTN_PREV        27

// ---------- I2S pins to the PCM5102A ----------
#define I2S_BCK_PIN   22
#define I2S_DATA_PIN  21
#define I2S_LRC_PIN   19

I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

bool isPlaying = false;

// simple debounce helper
bool buttonPressed(int pin) {
  static uint32_t lastPress[40] = {0};
  if (digitalRead(pin) == LOW) {           // active-low, using INPUT_PULLUP
    uint32_t now = millis();
    if (now - lastPress[pin] > 250) {      // 250ms debounce
      lastPress[pin] = now;
      return true;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);

  pinMode(BTN_PLAY_PAUSE, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_PREV, INPUT_PULLUP);

  // Configure I2S pins via AudioTools
  auto cfg = i2s.defaultConfig();
  cfg.pin_bck = I2S_BCK_PIN;
  cfg.pin_ws = I2S_LRC_PIN;
  cfg.pin_data = I2S_DATA_PIN;
  i2s.begin(cfg);

  a2dp_sink.set_on_data_received([]() {
    isPlaying = true;
  });

  // Start advertising with a custom Bluetooth name
  a2dp_sink.start("InkBT");
}

void loop() {
  if (buttonPressed(BTN_PLAY_PAUSE)) {
    if (isPlaying) {
      a2dp_sink.pause();
      isPlaying = false;
    } else {
      a2dp_sink.play();
      isPlaying = true;
    }
  }

  if (buttonPressed(BTN_NEXT)) {
    a2dp_sink.next();
  }

  if (buttonPressed(BTN_PREV)) {
    a2dp_sink.previous();
  }

  delay(10);
}
