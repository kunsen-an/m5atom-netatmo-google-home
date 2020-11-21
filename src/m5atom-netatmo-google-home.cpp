#include <Arduino.h>
#include <M5Atom.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

#include <ArduinoLog.h>
#include <google-tts.h>

#include "mysettings.h"
#include "netatmo-access.h"

extern int outputUrlToSpeaker(String url);
extern int outputUrlToGoogleHome(String url, String googleHomeName);

#define CO2_WARNING_LEVEL   800
#define INTERVAL_IN_MINUTE  10

const char* ssid     = MY_WIFI_SSID;
const char* password = MY_WIFI_PASSWORD;

WiFiMulti WiFiMulti;

String targetLocation = MY_TARGET_LOCATION;
String smartSpeaker   = MY_GOOGLE_HOME;

#define GREEN_COLOR     0xff0000
#define RED_COLOR       0x00ff00
#define BLUE_COLOR      0x0000ff
#define YELLOW_COLOR    0xffff00

void setLEDRed() {
  M5.dis.drawpix(0, RED_COLOR);
}

void setLEDGreen() {
  M5.dis.drawpix(0, GREEN_COLOR);
}

void setLEDBlue() {
  M5.dis.drawpix(0, BLUE_COLOR);
}

void setLEDYellow() {
  M5.dis.drawpix(0, YELLOW_COLOR);
}

#define INITIAL_RESET_FUSE  5
#define RETRY_DELAY_IN_SEC  10

//declare reset function at address 0
void(* resetFunc) (void) = 0; // CPU reset
static int fuse = INITIAL_RESET_FUSE;

void restoreFuse() {
  fuse = INITIAL_RESET_FUSE;
  setLEDGreen();
}

void decrementFuse() {
  setLEDRed();
  if ( fuse-- <= 0 ) {
    resetFunc();  // reset
  } else {
    delay(RETRY_DELAY_IN_SEC*1000);
  }
}

// print current time
void printCurrentTime() {
  time_t    nowSecs = time(nullptr);
  struct tm timeinfo;

  gmtime_r(&nowSecs, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

// Setting clock just to be sure...
void setClock() {
  configTime(0, 0, "ntp.nict.jp", "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    yield();
    nowSecs = time(nullptr);
  }
  Serial.println();

  printCurrentTime();
}

void cleanupClient(WiFiClientSecure *client) {
  if (client->connected()) client->stop();
  delete client;
}

String composeMessage(String targetLocation, bool forced) {
  // targetLocation の二酸化炭素濃度が基準値 O2_WARNING_LEVEL を超えていたら
  // それを知らせる文字列を返す。
  // forced が真の場合には、基準値に達していない場合も文字列を返す。
  // そうでない場合には空文字列を返す。
  Log.notice("composeMessage(%s, %d)\n", targetLocation.c_str(),forced);
  WiFiClientSecure *client = new WiFiClientSecure;
  if ( client == NULL ) {
    Log.error("Unable to create client\n");
    return "_fail";
  }
  
  // get access token 
  String accessToken = getAccessToken(client);
  if ( accessToken == "" ) {
    Log.error("getAccessToken failed.\n");
    cleanupClient(client);
    return "_fail";
  }

  // get CO2 value
  int co2value = getCO2value(client, accessToken, targetLocation);
  if ( co2value < 0 ) {
    Log.error("getCO2value failed.\n");
    cleanupClient(client);
    return "_fail";
  }

  // clean up client
  cleanupClient(client);

  String message;
  if ( co2value >= CO2_WARNING_LEVEL || forced ) {
    message = targetLocation + "のCO2濃度は、" + co2value + " ppmです。";
  }
  if ( co2value >= CO2_WARNING_LEVEL ) {
    message = message + "窓を開けるなどして、換気してください。";
  }
  Log.notice("message='%s'\n", message.c_str());

  return message;
}

TTS googleTTS;


String getGoogleSpeechUrl(String text) {
  // 引数の text を音声合成したファイルへのURLを返す。
  Log.notice("getGoogleTTSUrl(%s)\n", text.c_str());

  String url = googleTTS.getSpeechUrl(text, "ja");
  while (  url.startsWith("_")  ) {
    Log.notice("url=%s\n", url.c_str());
    decrementFuse();
    delay(1000);
    url = googleTTS.getSpeechUrl(text, "ja");
  }
  restoreFuse();

  String http = "http" + url.substring(5);
  Log.notice("http=%s\n", http.c_str());
  
  return http;
}

void setup() {
  Serial.begin(115200);
  M5.begin(true, false, true);
  Log.begin(LOG_LEVEL_VERBOSE, &Serial, true);
  //Log.begin(LOG_LEVEL_WARNING, &Serial, true);
  
  setLEDBlue();
  
  Serial.println();
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);

  // wait for WiFi connection
  Serial.print("Waiting for WiFi to connect");
  while ((WiFiMulti.run() != WL_CONNECTED)) {
    Serial.print(".");
    decrementFuse();
  }
  restoreFuse();

  Serial.println(" connected");

  IPAddress ipAddress = WiFi.localIP();
  Log.notice("IP address: %d.%d.%d.%d\n", ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);  //Print the local IP
  byte mac[6];
  WiFi.macAddress(mac);
  //Log.notice("MAC: %x:%x:%x:%x:%x:%x\n", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);

  // sync clock with NTP
  setClock();
}

time_t waitingFor() {
  int minute = INTERVAL_IN_MINUTE;
  String waitingMessage = String("Waiting for ") + minute + " minute(s) before the next round...";
  Log.notice("%s\n", waitingMessage.c_str());
  return time(nullptr) + minute*60;
}

time_t   nextTime = 0;

void loop() {
  time_t currentTime = time(nullptr);
  bool pressed = M5.Btn.isPressed();
  if ( nextTime < currentTime || pressed ) {
    setLEDGreen();
  
    String message = composeMessage(targetLocation, pressed);
    while ( message.startsWith("_") ) {
        decrementFuse();
        delay(7*1000);
        message = composeMessage(targetLocation,pressed);
    }
    restoreFuse();

    if ( message != "" ) {
      String url;
      url = getGoogleSpeechUrl(message);
      //outputUrlToSpeaker(url);
      while ( outputUrlToGoogleHome(url, smartSpeaker) != 0 )
        decrementFuse();
    }
    restoreFuse();

    printCurrentTime();
    nextTime = waitingFor();
    setLEDYellow();
  }
  M5.update();
}
