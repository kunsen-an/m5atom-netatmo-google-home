#include <M5Atom.h>
#include <SD.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoLog.h>

#include <esp8266-google-home-notifier.h>

GoogleHomeNotifier ghn;
  
int
outputUrlToGoogleHome(String url, String googleHomeName) {
  Log.notice("outputUrlToGoogleHome%s, %s)\n", url.c_str(), googleHomeName.c_str());
  Log.notice("connecting to Google Home...\n");

  while ( ghn.device(googleHomeName.c_str(), "ja") != true) {
    Log.error("google home notifier cannot connect to speaker. %s\n", ghn.getLastError());
    return -1;
  }

  Log.notice("found Google Home(%s:%d)\n", ghn.getIPAddress().toString().c_str(), ghn.getPort());

  while ( ghn.play(url.c_str()) != true ) {
    Log.error("play failed. %s\n", ghn.getLastError());
    return -1;
  }
  Log.notice("Done.\n");
  return 0;
}