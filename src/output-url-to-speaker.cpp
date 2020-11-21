#include <M5Atom.h>
#include <SD.h>     // これがないと vfs_api.h が見つからず fatal error: vfs_api.h: No such file or directory

#include <ArduinoLog.h>

#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

#include "mysettings.h"

AudioFileSourceHTTPStream *file;
AudioFileSourceBuffer     *buff;
AudioGeneratorMP3         *mp3;
AudioOutputI2S            *out;

#define CONFIG_I2S_BCK_PIN      19
#define CONFIG_I2S_LRCK_PIN     33
#define CONFIG_I2S_DATA_PIN     22

int
outputUrlToSpeaker(String url) {
  Log.notice("outputUrlToSpeaker(%s)\n", url.c_str());

 
  // audioLogger = &Serial;
  file = new AudioFileSourceHTTPStream( (const char *)url.c_str() );

  // buff = new AudioFileSourceBuffer(file, 10240);

  out = new AudioOutputI2S();
  out->SetPinout(CONFIG_I2S_BCK_PIN, CONFIG_I2S_LRCK_PIN, CONFIG_I2S_DATA_PIN);
  out->SetChannels(1);
  out->SetGain(0.6);

  mp3 = new AudioGeneratorMP3();

  // mp3->begin(buff, out);
  mp3->begin(file, out);

  //
  while (mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      return -1;
    }
    M5.update();
  }
  Log.notice("outputUrlToSpeaker done.\n");
  return 0;
}