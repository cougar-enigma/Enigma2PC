#ifndef __CSA_DECODE_H
#define __CSA_DECODE_H

#include <linux/dvb/ca.h>
#include "thread.h"
#include "ringbuffer.h"

#define TS_SIZE          188
#define TS_SYNC_BYTE     0x47

#define MAX_CSA_IDX  16
#define MAX_STALL_MS 70
#define FL_EVEN_GOOD 1
#define FL_ODD_GOOD  2
#define FL_ACTIVITY  4
#define MAX_KEY_WAIT 500 // time to wait if key not ready on change
//#define MAX_CSA_PIDS 8192
//#define MAX_REL_WAIT 100 // time to wait if key in used on set

class cDeCSA {
private:
  int cs;
  unsigned char **range, *lastData;
  unsigned int /*even_odd[MAX_CSA_IDX], */flags[MAX_CSA_IDX];
  unsigned int even_odd;
  cMutex mutex;
  cCondVar wait;
  cTimeMs stall;
  bool active;
  int cardindex;
  struct dvbcsa_key_s* csa_key_even;
  struct dvbcsa_key_s* csa_key_odd;
  struct dvbcsa_bs_key_s* csa_bs_key_even;
  struct dvbcsa_bs_key_s* csa_bs_key_odd;
  struct dvbcsa_bs_batch_s *pcks;

  void ResetState(void);
public:
  cDeCSA(int CardIndex);
  ~cDeCSA();
  bool Decrypt(unsigned char *data, int len, int& packetsCount);
  bool SetDescr(ca_descr_t *ca_descr, bool initial);
  void SetActive(bool on);
};

class cDeCsaTSBuffer : public cThread {
private:
  int f;
  int cardIndex, size;
  bool delivered;
  cRingBufferLinear *ringBuffer, *outBuffer;
  pthread_mutex_t mutex_data;
  cDeCSA *decsa;
  int bs_size;
  int lastPacketsCount;

  virtual void Action(void);
public:
  cDeCsaTSBuffer(int File, int Size, int CardIndex, cDeCSA *DeCsa);
  ~cDeCsaTSBuffer();
  uchar *GetPackets(int &packetsCount);
};

class cScDevice {
private:
  cDeCSA *decsa;
  cDeCsaTSBuffer *tsBuffer;
  cMutex tsMutex;
  int fd_dvr;
  cMutex cafdMutex;
  pthread_mutex_t data_mutex;

public:
  bool OpenDvr(void);
  void CloseDvr(void);
  uchar *GetTSPackets(int &Count);

  cScDevice(int Adapter, int Frontend, int cafd);
  ~cScDevice();
  bool SetCaDescr(ca_descr_t *ca_descr, bool initial);

  static int DvbOpen(const char *Name, int a, int f, int Mode, bool ReportError);
};


#endif
