/*
 * decoder.c: Thread which try to decrypt TS stream.
 *
 * See the main source file 'descr.c' for copyright information.
 *
 */

#include <unistd.h>
#include "decoder.h"

pthread_t decoder_thread;

cDecoder::cDecoder(int Size) {
  outBuffer  = new cRingBufferLinear(Size,TS_SIZE,true,"OUT-TS");
  sc = new cScDevice(0, 0, 0);
  decoder_working = true;
  Start();
}

cDecoder::~cDecoder() {
  delete outBuffer;
}

void cDecoder::Action(void) {
  unsigned char *data;
  int counter = 0;

  while(decoder_working) {
    sc->OpenDvr();
    stream_correct = 0;
    int d = 0;
    working = 1;

    while(working) {
      data = NULL;
      int packetsCount = 0;
      data = sc->GetTSPackets(packetsCount);
      if (data && packetsCount>0) {
        if (!stream_correct) {
          
          for (int i=0;i<packetsCount;i++) {
            unsigned char* packet = data+i*TS_SIZE;

            int adaptation_field_exist = (packet[3]&0x30)>>4;
            unsigned char* wsk;
            int len;
            if (adaptation_field_exist==3) {
              wsk = packet+5+packet[4];
              len = 183-packet[4];
            } else {
              wsk = packet+4;
              len = 184;
            }

            if (len>4 && wsk[0]==0 && wsk[1]==0 && wsk[2]==1
                  && ((wsk[3]>=VIDEO_STREAM_S && wsk[3]<=VIDEO_STREAM_E)
                  || (wsk[3]>=AUDIO_STREAM_S && wsk[3]<=AUDIO_STREAM_E)) ) {
              //video_ok = true;
              stream_correct = true;
              printf("-------------------- I have PES ---------------------- %02X\n", wsk[3]);
              outBuffer->Put(packet, (packetsCount-i)*TS_SIZE);
              break;
            }
          }
          
        } else {
          outBuffer->Put(data, packetsCount*TS_SIZE);

          /*if (d%1000==0)
            outBuffer->PrintDebugRBL();
          d++;*/
        }

        counter++;

        if (counter%1000==0)
          printf("decoder_counter %d\n", counter);
      } else {
        usleep(10000);
      }
    }
    printf("Close DVR\n");
    sc->CloseDvr();
    outBuffer->Clear();
    usleep(100000);
  }
}

void cDecoder::Restart() {
  stream_correct = false;
  working = false;
}

uchar* cDecoder::GetData(int &Count) {
  return outBuffer->Get(Count);
}

void cDecoder::DelData(int Count) {
  outBuffer->Del(Count);
}

