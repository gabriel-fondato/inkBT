#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// ---------- Button pins ----------
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

// ---------- Debounce helper ----------
bool buttonPressed(int pin) {
  static uint32_t lastPress[40] = {0};
  if (digitalRead(pin) == LOW) {
    uint32_t now = millis();
    if (now - lastPress[pin] > 250) {
      lastPress[pin] = now;
      return true;
    }
  }
  return false;
}

// ---------- CALLBACKS: connection status ----------
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
  Serial.print("[CONNECTION] ");
  switch (state) {
    case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
      Serial.println("Disconnected");
      break;
    case ESP_A2D_CONNECTION_STATE_CONNECTING:
      Serial.println("Connecting...");
      break;
    case ESP_A2D_CONNECTION_STATE_CONNECTED:
      Serial.println("Connected!");
      Serial.print("[DEVICE] Peer name: ");
      Serial.println(a2dp_sink.get_peer_name());
      break;
    case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
      Serial.println("Disconnecting...");
      break;
  }
}

// ---------- CALLBACKS: audio play/pause/stop status ----------
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
  Serial.print("[AUDIO STATE] ");
  switch (state) {
    case ESP_A2D_AUDIO_STATE_STOPPED:
      Serial.println("Stopped");
      isPlaying = false;
      break;
    case ESP_A2D_AUDIO_STATE_STARTED:
      Serial.println("Playing");
      isPlaying = true;
      break;
  }
}

// ---------- CALLBACKS: track metadata (title, artist, album, etc.) ----------
void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  Serial.print("[METADATA] ");
  switch (id) {
    case ESP_AVRC_MD_ATTR_TITLE:
      Serial.print("Title: ");
      break;
    case ESP_AVRC_MD_ATTR_ARTIST:
      Serial.print("Artist: ");
      break;
    case ESP_AVRC_MD_ATTR_ALBUM:
      Serial.print("Album: ");
      break;
    case ESP_AVRC_MD_ATTR_GENRE:
      Serial.print("Genre: ");
      break;
    case ESP_AVRC_MD_ATTR_TRACK_NUM:
      Serial.print("Track number: ");
      break;
    case ESP_AVRC_MD_ATTR_NUM_TRACKS:
      Serial.print("Total tracks: ");
      break;
    case ESP_AVRC_MD_ATTR_PLAYING_TIME:
      Serial.print("Playing time (ms): ");
      break;
    default:
      Serial.print("Unknown attr 0x");
      Serial.print(id, HEX);
      Serial.print(": ");
      break;
  }
  Serial.println((const char *)text);
}

// ---------- CALLBACKS: play status notifications (play/pause/stop from phone side) ----------
void avrc_rn_playstatus_callback(esp_avrc_playback_stat_t playback) {
  Serial.print("[PLAY STATUS] ");
  switch (playback) {
    case ESP_AVRC_PLAYBACK_STOPPED:  Serial.println("Stopped"); break;
    case ESP_AVRC_PLAYBACK_PLAYING:  Serial.println("Playing"); break;
    case ESP_AVRC_PLAYBACK_PAUSED:   Serial.println("Paused"); break;
    case ESP_AVRC_PLAYBACK_FWD_SEEK: Serial.println("Seeking forward"); break;
    case ESP_AVRC_PLAYBACK_REV_SEEK: Serial.println("Seeking backward"); break;
    default:                         Serial.println("Error/unknown"); break;
  }
}

// ---------- Print help menu over serial ----------
void print_help() {
  Serial.println();
  Serial.println("===== Serial Console Commands =====");
  Serial.println(" p  = play/pause toggle");
  Serial.println(" n  = next track");
  Serial.println(" b  = previous track");
  Serial.println(" s  = print current status");
  Serial.println(" h  = show this help menu");
  Serial.println("====================================");
  Serial.println();
}

void print_status() {
  Serial.println();
  Serial.println("----- Current Status -----");
  Serial.print("Connected: ");
  Serial.println(a2dp_sink.is_connected() ? "Yes" : "No");
  if (a2dp_sink.is_connected()) {
    Serial.print("Peer device: ");
    Serial.println(a2dp_sink.get_peer_name());
  }
  Serial.print("Playing: ");
  Serial.println(isPlaying ? "Yes" : "No");
  Serial.println("---------------------------");
  Serial.println();
}

// ---------- Handle serial commands typed into the Serial Monitor ----------
void handleSerialInput() {
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'p':
        if (isPlaying) { a2dp_sink.pause(); Serial.println("[CMD] Pause"); }
        else { a2dp_sink.play(); Serial.println("[CMD] Play"); }
        isPlaying = !isPlaying;
        break;
      case 'n':
        a2dp_sink.next();
        Serial.println("[CMD] Next track");
        break;
      case 'b':
        a2dp_sink.previous();
        Serial.println("[CMD] Previous track");
        break;
      case 's':
        print_status();
        break;
      case 'h':
        print_help();
        break;
      default:
        // ignore newlines/whitespace silently
        break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BTN_PLAY_PAUSE, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_PREV, INPUT_PULLUP);

  // I2S output config
  auto cfg = i2s.defaultConfig();
  cfg.pin_bck = I2S_BCK_PIN;
  cfg.pin_ws = I2S_LRC_PIN;
  cfg.pin_data = I2S_DATA_PIN;
  i2s.begin(cfg);

  // Register all the info callbacks BEFORE start()
  a2dp_sink.set_on_connection_state_changed(connection_state_changed);
  a2dp_sink.set_on_audio_state_changed(audio_state_changed);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_avrc_rn_playstatus_callback(avrc_rn_playstatus_callback);

  a2dp_sink.start("InkBT");

  Serial.println("\n[BOOT] InkBT ready — waiting for a Bluetooth connection...");
  print_help();
}

void loop() {
  handleSerialInput();

  if (buttonPressed(BTN_PLAY_PAUSE)) {
    if (isPlaying) { a2dp_sink.pause(); Serial.println("[BUTTON] Pause"); }
    else { a2dp_sink.play(); Serial.println("[BUTTON] Play"); }
    isPlaying = !isPlaying;
  }

  if (buttonPressed(BTN_NEXT)) {
    a2dp_sink.next();
    Serial.println("[BUTTON] Next track");
  }

  if (buttonPressed(BTN_PREV)) {
    a2dp_sink.previous();
    Serial.println("[BUTTON] Previous track");
  }

  delay(10);
}