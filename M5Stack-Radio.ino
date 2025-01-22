#include <Arduino.h>
#include <WiFi.h>
#include <M5Stack.h>
#include <SD.h>

#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"

#include <AudioFileSource.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceICYStream.h>
#include <AudioGeneratorTalkie.h>
#include <AudioGeneratorAAC.h>
#include <AudioOutputI2S.h>
#include <spiram-fast.h>

File myFile;

// your WiFi setup goes here:
String SSID = String("");
String PASSWORD = String("");
String wifiConfig = String ("/wifi.config");
// wifi.config structure is:
// SSID:_here_
// PASSWORD:_here_

// 2 do - put link into the external file
char *URL = "http://ice.creacast.com/sudradio";

char BannerText[] = "^WSimple online radio by ^R5^W0u15pec7a70r ^B/^G/^Y/^G/^B/ ^Whttp://ice.creacast.com/sudradio ^B/^G/^Y/^G/^B/ "; // ^W - white, red, green, blue, yellow
int displayLen = 52;
unsigned long Frame = 120;
unsigned long nextFrame = millis() + Frame;
uint16_t BUFFERCOLOR = WHITE;

int spriteX = 320;
int spriteY = 20;
int text1Width = 52; //53
int font1Width = 6;

char *noWIFIText = " no wi-fi connection ";
char *noSTREAMINGText = " no streaming ";
char *SDErrorText = " SD card error ";
char *noSDErrorText = " SD card is missed ";
char *WIFIReadingErrorText = " wifi.config reading error ";

AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

// This will store and control our volume
float audioGain = 1.0;
float gainfactor = 0.08;

// This will indicate playing/streaming status
bool bPlay = false;

// Buffer for sprite
TFT_eSprite Disbuff = TFT_eSprite(&M5.Lcd);

// We need it for streaming
const int bufferSize = 32 * 1024;  // M0AAAAAAARE!!!!~

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void)isUnicode;  // Punt this ball for now
                    // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
}

// Some drawing routines
void drawVolumeMeter() {
  String VolumeMeter = String("");
  int i = 1;
  int iMax = 10;

  Disbuff.setCursor(75, 0);
  Disbuff.setTextColor(DARKGREY);
  Disbuff.print("[+");

  while (i <= audioGain) {
    VolumeMeter += "=";
    i += 1;
  }
  Disbuff.setTextColor(GREEN);
  Disbuff.print(VolumeMeter);

  VolumeMeter = String("");
  while (i <= iMax) {
    VolumeMeter += "=";
    i += 1;
  }
  Disbuff.setTextColor(DARKGREEN);
  Disbuff.print(VolumeMeter);

  Disbuff.setTextColor(DARKGREY);
  Disbuff.print("+]");
}

void showVolumeStatus() {
  Disbuff.createSprite(spriteX, spriteY);
  Disbuff.fillScreen(BLACK);
  Disbuff.setTextSize(2);

  drawVolumeMeter();

  Disbuff.pushSprite(0, 170);
  Disbuff.deleteSprite();
}

void drawVolumeDown() {
  Disbuff.setCursor(50, 0);
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print("[ ");
  Disbuff.setTextColor(LIGHTGREY,0x18e3);
  Disbuff.print("-");
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print(" ]");
}

void drawVolumeDownM() {
  Disbuff.setCursor(50, 0);
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print("[ ");
  Disbuff.setTextColor(0x3186,0x18e3);
  Disbuff.print("-");
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print(" ]");
}

void drawPause() {
  Disbuff.setCursor(130, 0);
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print("[ ");
  Disbuff.setTextColor(RED,0x18e3);
  Disbuff.print(" STOP ");
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print(" ]");
}

void drawPlay() {
  Disbuff.setCursor(130, 0);
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print("[ ");
  Disbuff.setTextColor(GREEN,0x18e3);
  Disbuff.print("STREAM");
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print(" ]");
}

void drawVolumeUp() {
  Disbuff.setCursor(240, 0);
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print("[ ");
  Disbuff.setTextColor(LIGHTGREY,0x18e3);
  Disbuff.print("+");
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print(" ]");
}

void drawVolumeUpM() {
  Disbuff.setCursor(240, 0);
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print("[ ");
  Disbuff.setTextColor(0x3186,0x18e3);
  Disbuff.print("+");
  Disbuff.setTextColor(WHITE,0x18e3);
  Disbuff.print(" ]");
}

void showStatus() {
  Disbuff.createSprite(spriteX, spriteY);
  Disbuff.fillScreen(BLACK);
  Disbuff.setTextSize(1);

  if (audioGain == 0.0) {
    drawVolumeDownM();
  } else {
    drawVolumeDown();
  }

  if (bPlay) {
    drawPause();
  } else {
    drawPlay();
  }

  if (audioGain == 10.0) {
    drawVolumeUpM();
  } else {
    drawVolumeUp();
  }

  Disbuff.pushSprite(0, 225);
  Disbuff.deleteSprite();
}

void drawBanner() {
  Disbuff.createSprite(spriteX, spriteY);
  Disbuff.fillScreen(BLACK);
  Disbuff.setTextSize(1);
  Disbuff.setTextColor(BUFFERCOLOR);

  char firstChar;
  char secondChar;
  char thirdChar;
  bool foundSpecial = false;
  int i = 0;

  if ((BannerText[0] == '^') and ((BannerText[1] == 'W') or (BannerText[1] == 'R') or (BannerText[1] == 'G') or (BannerText[1] == 'B') or (BannerText[1] == 'Y'))) {
    firstChar = BannerText[0];
    secondChar = BannerText[1];
    thirdChar = BannerText[2];
    foundSpecial = true;
  } else {
    firstChar = BannerText[0];
  }
  
  while (i < (strlen(BannerText)-1)){
    if (foundSpecial){
      if ((i+3) < strlen(BannerText)) {
        BannerText[i] = BannerText[i+3];
      }
    } else {
      BannerText[i] = BannerText[i+1];
    }
    i++;
  }

  if (foundSpecial){
    BannerText[strlen(BannerText)-3] = firstChar;
    BannerText[strlen(BannerText)-2] = secondChar;
    BannerText[strlen(BannerText)-1] = thirdChar;
  } else {
    BannerText[strlen(BannerText)-1] = firstChar;
  }

  i = 0;
  int yCoord = 5; // Y start position
  int prnTMP = 0; // printing position

  while (prnTMP <= displayLen) {

    if ((BannerText[i] == '^') and (BannerText[i+1] == 'W')) {
      Disbuff.setTextColor(WHITE);
      if (prnTMP == 0) {
        BUFFERCOLOR = WHITE;
      }
      i++;
      i++;
      if (i>=strlen(BannerText)) {
        i = 0;
      }
    } else if ((BannerText[i] == '^') and (BannerText[i+1] == 'R')) {
      Disbuff.setTextColor(RED);
      if (prnTMP == 0) {
        BUFFERCOLOR = RED;
      }
      i++;
      i++;
      if (i>=strlen(BannerText)) {
        i = 0;
      }
    } else if ((BannerText[i] == '^') and (BannerText[i+1] == 'G')) {
      Disbuff.setTextColor(GREEN);
      if (prnTMP == 0) {
        BUFFERCOLOR = GREEN;
      }
      i++;
      i++;
      if (i>=strlen(BannerText)) {
        i = 0;
      }
    } else if ((BannerText[i] == '^') and (BannerText[i+1] == 'B')) {
      Disbuff.setTextColor(BLUE);
      if (prnTMP == 0) {
        BUFFERCOLOR = BLUE;
      }
      i++;
      i++;
      if (i>=strlen(BannerText)) {
        i = 0;
      }
    } else if ((BannerText[i] == '^') and (BannerText[i+1] == 'Y')) {
      Disbuff.setTextColor(YELLOW);
      if (prnTMP == 0) {
        BUFFERCOLOR = YELLOW;
      }
      i++;
      i++;
      if (i>=strlen(BannerText)) {
        i = 0;
      }
    } else {
      int x = font1Width*((text1Width-displayLen)/2+prnTMP);
      Disbuff.setCursor(x, yCoord + 5*sin(x) );
      Disbuff.print(BannerText[i]);
      prnTMP++; // printed
      i++;
      if (i>=strlen(BannerText)) {
        i = 0;
      }
    }
  }
  
  Disbuff.pushSprite(0, 90);
  Disbuff.deleteSprite();
}

void drawWIFIinfo() {
  char *MACbuffer = new char[WiFi.BSSIDstr().length() + 1];  //str to char
  strcpy(MACbuffer, WiFi.BSSIDstr().c_str());
  M5.Lcd.setTextColor(0x4228);
  M5.Lcd.setCursor((font1Width*(text1Width- 22)/2), 130); //+8 = "((( "+" )))"
  M5.Lcd.printf("MAC: %*s", 17, MACbuffer); // 22 (5+17)
}

void drawPower() {
  Disbuff.createSprite(40, 10);
  Disbuff.fillScreen(BLACK);
  
  Disbuff.setCursor(0, 0);
  Disbuff.setTextSize(1);

  if (M5.Power.isCharging()) {
    Disbuff.setTextColor(0xffff);
    Disbuff.printf("+");
  } else {
    Disbuff.printf(" "); // Dirty hack
  }
 
  if (M5.Power.getBatteryLevel() == 100) {
    Disbuff.setTextColor(0x07e0);
  } else if ((M5.Power.getBatteryLevel() < 100) && (M5.Power.getBatteryLevel() >= 75)) {
    Disbuff.setTextColor(0x0660);
  } else if ((M5.Power.getBatteryLevel() < 75) && (M5.Power.getBatteryLevel() >= 50)) {
    Disbuff.setTextColor(0x04e0);
  } else if ((M5.Power.getBatteryLevel() < 50) && (M5.Power.getBatteryLevel() >= 25)) {
    Disbuff.setTextColor(0x0360);
  } else if ((M5.Power.getBatteryLevel() < 25) && (M5.Power.getBatteryLevel() >= 0)) {
    Disbuff.setTextColor(0x01e0);
  }

  Disbuff.printf("%3d%%", M5.Power.getBatteryLevel());
  
  Disbuff.pushSprite(290, 0);
  Disbuff.deleteSprite();
}

void displayMainScreen() {
  M5.Lcd.clearDisplay();

  drawBanner();
  drawWIFIinfo();
  showVolumeStatus();
  showStatus();
}

// Error screens. I'll keep 'em simple.
void displayError(char *ErrorMessage) {
  M5.Lcd.clearDisplay();
  M5.Lcd.setCursor((font1Width*(text1Width-strlen(ErrorMessage)-8)/2), 120); //+8 = "((( "+" )))"
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.print("((");
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.print("(");
  M5.Lcd.setTextColor(RED);
  M5.Lcd.print(ErrorMessage);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.print(")");
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.print("))");
}

// Some supplement functions
void readWiFIconfig() {
  if (!SD.begin(4)) {
      displayError(SDErrorText);
      while (1); // Nowhere to go.
    }

  if(SD.cardType() == CARD_NONE){
      displayError(noSDErrorText);;
      while (1); // Nowhere to go.
  }

  myFile = SD.open(wifiConfig);
    
  int lineIndex = 1;
  int charIndex = 0;
  char tmpChar;
  if (myFile) {
    while (myFile.available()) {
      tmpChar = char (myFile.read());
      charIndex += 1;

      // Dirty hacks everywhere! x_x
      while (tmpChar != '\n') {
        if ((lineIndex == 1) && (charIndex > strlen("SSID:"))) {
          SSID += tmpChar;
        }
        if ((lineIndex == 2) && (charIndex > strlen("PASSWORD:"))) {
          PASSWORD += tmpChar;
        }
        tmpChar = char (myFile.read());
        charIndex += 1;
      }
      lineIndex += 1;
      charIndex = 0;
    }
    myFile.close();
  } else {
    displayError(WIFIReadingErrorText);
    while (1); // Nowhere to go.
  }
}

// To add & to code...
void turn_off_lcd() {
  return;
}

void turn_on_lcd() {
  return;
}

// Main streaming routine
void play() {
  file = new AudioFileSourceICYStream(URL);
  file->RegisterMetadataCB(MDCallback, (void *)"ICY");
  buff = new AudioFileSourceBuffer(file, bufferSize);
  buff->RegisterStatusCB(StatusCallback, (void *)"buffer");
  out = new AudioOutputI2S(0, true);
  out->SetOutputModeMono(true);
  out->SetGain(audioGain * gainfactor);
  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void *)"mp3");
  mp3->begin(buff, out);

  if (mp3 != NULL) {
    if (mp3->isRunning()) {
      bPlay = true;
      displayMainScreen();
    }
  }
}

void resume() {
  bPlay = true;
  showStatus();
  out->begin();
}

void stop() {
  bPlay = false;
  showStatus();
  out->stop();
}

void volume_up() {
  audioGain += 1.0;
  if (audioGain > 10.0) {
    audioGain = 10.0;
  }
  if (audioGain == 1.0) { // I want VolumeUp work as START in some cases
    resume();
  }
  showVolumeStatus();
  showStatus();

  out->SetGain(audioGain * gainfactor);
}

void volume_down() {
  audioGain -= 1.0;
  if (audioGain < 0.0) {
    audioGain = 0.0;
  }
  if (audioGain == 0.0) { // I want VolumeDown work as STOP in some cases
    stop();
  }
  showVolumeStatus();
  showStatus();

  out->SetGain(audioGain * gainfactor);
}

void loopMP3() {
  if (mp3 != NULL) {  // To avoid crash
    if (mp3->isRunning()) {
      if (!mp3->loop()) mp3->stop();
    } else {;
      displayError(noSTREAMINGText);
      bPlay = false;

      delay(1000);  //wait 1 second and try to reconnect
      play();
    }
  }
}

void startWIFI() {
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    displayError(noWIFIText);
    delay(1000);
  }
}

void setup() {
  M5.begin();
  M5.Power.begin(); // I've got teh POWER!

  delay(1000);

  readWiFIconfig();
  startWIFI();

  play();
}

// Your entry point starts here
void loop() {
  loopMP3();
  drawPower();

  if (millis() > nextFrame) {
    nextFrame = millis() + Frame;
    drawBanner();
  }
  
  M5.update(); // ...and UPDATE everything! Just get the buttons status.

  if (M5.BtnA.wasPressed()) {  // button A (((-)))
    volume_down();
    delay (200);
  }

  if (M5.BtnB.wasPressed()) {  // button B (((*)))
    if (bPlay) {
      stop();
    } else {
      if (audioGain == 0.0) { // It will work as VolumeUp in this case.
        audioGain = 1.0;
        out->SetGain(audioGain * gainfactor);
        showVolumeStatus();
      }
      resume();
    }
    delay (200);
  }

  if (M5.BtnC.wasPressed()) {  // button C (((+)))
    volume_up();
    delay (200);
  }
}