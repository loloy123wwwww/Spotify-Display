#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <SpotifyEsp32.h>
#include <SPI.h>

#define TFT_CS   1
#define TFT_RST  2
#define TFT_DC   3
#define TFT_SCLK 4
#define TFT_MOSI 5

#define BTN_PREV      6
#define BTN_PLAYPAUSE 7
#define BTN_NEXT      8

#define DEBOUNCE_MS 300

const char* SSID           = "YOUR WIFI";
const char* PASSWORD       = "YOUR PASS";
const char* CLIENT_ID      = "YOUR CLIENT ID";
const char* CLIENT_SECRET  = "YOUR CLIENT SECRET";
const char* REFRESH_TOKEN  = "";

#define INFO_X  70
#define NAV_Y   116
#define BAR_X_S 38
#define BAR_X_E 122
#define BAR_Y   88
#define BAR_H   7
#define TIME_Y  78

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
Spotify sp(CLIENT_ID, CLIENT_SECRET, REFRESH_TOKEN);

String lastArtist = "";
String lastTrack  = "";
bool   isPlaying  = false;

long         trackProgress = 0;
long         trackDuration = 180000;
unsigned long progressAt   = 0;

unsigned long lastPoll         = 0;
unsigned long lastProgressDraw = 0;
unsigned long lastBtnPress[3]  = {0, 0, 0};

const unsigned long POLL_MS          = 3000;
const unsigned long PROGRESS_DRAW_MS = 1000;


String truncate(String s, int n) {
  if (s.length() <= n) return s;
  return s.substring(0, n - 2) + "..";
}


String fmtMs(long ms) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", (int)(ms / 60000), (int)((ms / 1000) % 60));
  return String(buf);
}


void drawPlayIcon(bool playing, uint16_t col) {
  int cx = 80;
  int cy = NAV_Y;

  tft.fillRect(cx - 10, cy - 9, 22, 19, ST77XX_BLACK);

  if (playing) {
    tft.fillRect(cx - 8, cy - 8, 5, 16, col);
    tft.fillRect(cx + 3, cy - 8, 5, 16, col);
  } else {
    tft.fillTriangle(cx - 7, cy - 9, cx - 7, cy + 9, cx + 9, cy, col);
  }
}


void drawProgress() {
  long elapsed = trackProgress + (millis() - progressAt);
  if (elapsed > trackDuration) elapsed = trackDuration;

  tft.fillRect(0, TIME_Y - 2, 160, 30, ST77XX_BLACK);

  String el  = fmtMs(elapsed);
  String dur = fmtMs(trackDuration);

  tft.setCursor(BAR_X_S - el.length() * 6 - 3, TIME_Y);
  tft.print(el);

  tft.setCursor(BAR_X_E + 3, TIME_Y);
  tft.print(dur);

  tft.fillRoundRect(BAR_X_S, BAR_Y, BAR_X_E - BAR_X_S, BAR_H, 3, 0x2945);

  int fillW = (BAR_X_E - BAR_X_S) * elapsed / trackDuration;
  tft.fillRoundRect(BAR_X_S, BAR_Y, fillW, BAR_H, 3, ST77XX_GREEN);
}


void drawTrackInfo() {
  tft.fillRect(INFO_X, 0, 160 - INFO_X, 60, ST77XX_BLACK);

  tft.setCursor(INFO_X, 8);
  tft.setTextColor(ST77XX_CYAN);
  tft.print(truncate(lastArtist, 14));

  tft.setCursor(INFO_X, 28);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(truncate(lastTrack, 14));
}


void pollSpotify() {
  String artist = sp.current_artist_names();
  String track  = sp.current_track_name();
  bool   play   = sp.is_playing();

  if (artist == "" || track == "") return;

  if (play != isPlaying) {
    isPlaying = play;
    drawPlayIcon(isPlaying, isPlaying ? ST77XX_GREEN : ST77XX_YELLOW);
    if (isPlaying) progressAt = millis() - trackProgress;
  }

  bool changed = (artist != lastArtist || track != lastTrack);

  if (changed) {
    lastArtist    = artist;
    lastTrack     = track;
    trackProgress = 0;
    progressAt    = millis();
    drawTrackInfo();
  }
}


void setup() {
  Serial.begin(115200);

  pinMode(BTN_PREV,      INPUT_PULLUP);
  pinMode(BTN_PLAYPAUSE, INPUT_PULLUP);
  pinMode(BTN_NEXT,      INPUT_PULLUP);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  sp.set_scopes("user-read-playback-state user-modify-playback-state");
  sp.begin();
  while (!sp.is_auth()) { sp.handle_client(); }

  pollSpotify();
  drawPlayIcon(false, ST77XX_YELLOW);
}


void loop() {
  unsigned long now = millis();

  // previous
  if (digitalRead(BTN_PREV) == LOW && now - lastBtnPress[0] > DEBOUNCE_MS) {
    lastBtnPress[0] = now;
    sp.previous();
  }

  // play / stop
  if (digitalRead(BTN_PLAYPAUSE) == LOW && now - lastBtnPress[1] > DEBOUNCE_MS) {
    lastBtnPress[1] = now;
    if (isPlaying) {
      sp.pause_playback();
      isPlaying = false;
    } else {
      sp.start_resume_playback();
      isPlaying = true;
    }
    drawPlayIcon(isPlaying, isPlaying ? ST77XX_GREEN : ST77XX_YELLOW);
  }

  // next
  if (digitalRead(BTN_NEXT) == LOW && now - lastBtnPress[2] > DEBOUNCE_MS) {
    lastBtnPress[2] = now;
    sp.skip();
  }

  if (now - lastPoll >= POLL_MS) {
    lastPoll = now;
    pollSpotify();
  }

  if (now - lastProgressDraw >= PROGRESS_DRAW_MS && isPlaying) {
    lastProgressDraw = now;
    drawProgress();
  }
}