#ifndef __DECODER_H
#define __DECODER_H

#include <pthread.h>
#include "ringbuffer.h"
#include "thread.h"
#include "csa_decode.h"

#define AUDIO_STREAM_S   0xC0
#define AUDIO_STREAM_E   0xDF
#define VIDEO_STREAM_S   0xE0
#define VIDEO_STREAM_E   0xEF

class cDecoder : public cThread {
private:
  cRingBufferLinear *outBuffer;
  bool delivered;
  bool stream_correct;

  virtual void Action(void);

public:
  cScDevice* sc;
  bool decoder_working;
  bool working;

  cDecoder(int Size);
  ~cDecoder();
  void Restart();
  uchar* GetData(int &Count);
  void DelData(int Count);
};

#endif
