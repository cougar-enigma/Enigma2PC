/*
 * csa_decode.c: Functions to decrypt TS Packets
 *
 * See the main source file 'descr.c' for copyright information.
 *
 */

#include <unistd.h>
#include "csa_decode.h"

extern "C" {
#include <dvbcsa/dvbcsa.h>
}


// --- cDeCSA -----------------------------------------------------------------

cDeCSA::cDeCSA(int CardIndex) :stall(MAX_STALL_MS) {
  cardindex=CardIndex;
  cs=dvbcsa_bs_batch_size();
  pcks = new dvbcsa_bs_batch_s[cs+1];

  csa_key_even = dvbcsa_key_alloc();
  csa_key_odd = dvbcsa_key_alloc();
  csa_bs_key_even = dvbcsa_bs_key_alloc();
  csa_bs_key_odd = dvbcsa_bs_key_alloc();

  for (int i=0; i<=cs; i++) {
    pcks[i].data = NULL;
  }

  printf("%d: clustersize=%d rangesize=%d",cardindex,cs,cs*2+5);
  range=MALLOC(unsigned char *,(cs*2+5));
  ResetState();
}

cDeCSA::~cDeCSA()
{
  dvbcsa_bs_key_free(csa_bs_key_even);
  dvbcsa_bs_key_free(csa_bs_key_odd);
  free(range);
}

void cDeCSA::ResetState(void)
{
  printf("%d: reset state",cardindex);
  //memset(even_odd,0,sizeof(even_odd));
  even_odd = 0;
  memset(flags,0,sizeof(flags));
  lastData=0;
}

void cDeCSA::SetActive(bool on)
{
  if(!on && active) ResetState();
  active=on;
  printf("%d: set active %s\n",cardindex,active?"on":"off");
}

static bool CheckNull(const unsigned char *data, int len)
{
  while(--len>=0) if(data[len]) return false;
  return true;
}

bool cDeCSA::SetDescr(ca_descr_t *ca_descr, bool initial)
{
  cMutexLock lock(&mutex);
  int idx=ca_descr->index;
  if(idx<MAX_CSA_IDX) {
    /*if(!initial && active && ca_descr->parity==(even_odd&0x40)>>6) {
      if(flags[idx] & (ca_descr->parity?FL_ODD_GOOD:FL_EVEN_GOOD)) {
        printf("%d.%d: %s key in use (%d ms)",cardindex,idx,ca_descr->parity?"odd":"even",MAX_REL_WAIT);
        if(wait.TimedWait(mutex,MAX_REL_WAIT)) printf("%d.%d: successfully waited for release",cardindex,idx);
        else printf("%d.%d: timed out. setting anyways",cardindex,idx);
        }
      else printf("%d.%d: late key set...",cardindex,idx);
      }*/
    //LDUMP(L_CORE_CSA,ca_descr->cw,8,"%d.%d: %4s key set",cardindex,idx,ca_descr->parity?"odd":"even");
    if(ca_descr->parity==0) {
      dvbcsa_key_set (ca_descr->cw, csa_key_even);
      dvbcsa_bs_key_set(ca_descr->cw, csa_bs_key_even);
      printf("New even key\n");

      if(!CheckNull(ca_descr->cw,8)) flags[idx]|=FL_EVEN_GOOD|FL_ACTIVITY;
      else printf("%d.%d: zero even CW",cardindex,idx);
      wait.Broadcast();
      }
    else {
      dvbcsa_key_set (ca_descr->cw, csa_key_odd);
      dvbcsa_bs_key_set(ca_descr->cw, csa_bs_key_odd);
      printf("New odd key\n");

      if(!CheckNull(ca_descr->cw,8)) flags[idx]|=FL_ODD_GOOD|FL_ACTIVITY;
      else printf("%d.%d: zero odd CW",cardindex,idx);
      wait.Broadcast();
      }
    }
  return true;
}

bool cDeCSA::Decrypt(unsigned char *data, int len, int& packetsCount)
{
  cMutexLock lock(&mutex);
  int r=-2, ccs=0, currIdx=-1;
  bool newRange=true;
  range[0]=0;
  len-=(TS_SIZE-1);
  int l;
  int packets=0, cryptedPackets=0;

  for(l=0; l<len && cryptedPackets<cs; l+=TS_SIZE) {
    unsigned int ev_od=data[l+3]&0xC0;
    int adaptation_field_exist = (data[l+3]&0x30)>>4;

    if((ev_od==0x80 || ev_od==0xC0) && adaptation_field_exist!=2) { // encrypted
      if(ev_od!=even_odd) {
        if (cryptedPackets==0) {
          even_odd=ev_od;
          wait.Broadcast();
          printf("%d.%d: change to %s key\n",cardindex,0,(ev_od&0x40)?"odd":"even");
        /*bool doWait=false;
        if(ev_od&0x40) {
          flags[0]&=~FL_EVEN_GOOD;
          if(!(flags[0]&FL_ODD_GOOD)) doWait=true;
        }
        else {
          flags[0]&=~FL_ODD_GOOD;
          if(!(flags[0]&FL_EVEN_GOOD)) doWait=true;
        }
        if(doWait) {
          printf("%d.%d: %s key not ready (%d ms)\n",cardindex,0,(ev_od&0x40)?"odd":"even",MAX_KEY_WAIT);
          if(flags[0]&FL_ACTIVITY) {
            flags[0]&=~FL_ACTIVITY;
            if(wait.TimedWait(mutex,MAX_KEY_WAIT))
              printf("%d.%d: successfully waited for key\n",cardindex,0);
            else
              printf("%d.%d: timed out. proceeding anyways\n",cardindex,0);
          } else
            printf("%d.%d: not active. wait skipped\n",cardindex,0);
        }*/
        } else {
          break;
        }
      }

      if (adaptation_field_exist==1) {
        pcks[cryptedPackets].data = data+l+4;
        pcks[cryptedPackets].len = 184;
      } else if (adaptation_field_exist==3) {
        pcks[cryptedPackets].data = data+l+5+data[l+4];
        pcks[cryptedPackets].len = 183-data[l+4];
      }
      data[l+3] &= 0x3F;
      cryptedPackets++;
    }
    packets++;
  }

  if (cryptedPackets>0) {
    for (int i=cryptedPackets;i<=cs;i++) {
      pcks[i].data = NULL;
    }

    if (even_odd&0x40) {
      dvbcsa_bs_decrypt(csa_bs_key_odd, pcks, 184);
    } else {
      dvbcsa_bs_decrypt(csa_bs_key_even, pcks, 184);
    }
  }

  packetsCount = packets;

  stall.Set(MAX_STALL_MS);
  return true;
}


// --- cDeCsaTSBuffer ---------------------------------------------------------

cDeCsaTSBuffer::cDeCsaTSBuffer(int File, int Size, int CardIndex, cDeCSA *DeCsa)
{
  f=File;
  size=Size;
  cardIndex=CardIndex;
  decsa=DeCsa;
  delivered=false;
  lastPacketsCount = 0;
  ringBuffer = new cRingBufferLinear(Size,TS_SIZE,true,"IN-TS");
  ringBuffer->SetTimeouts(100,100);
  bs_size = dvbcsa_bs_batch_size();
  decsa->SetActive(true);
  Start();
}

cDeCsaTSBuffer::~cDeCsaTSBuffer()
{
  Cancel(3);
  decsa->SetActive(false);
  delete ringBuffer;
}

void cDeCsaTSBuffer::Action(void)
{
  if(ringBuffer) {
    bool firstRead=true;
    cPoller Poller(f);
    while(Running()) {
      if(firstRead || Poller.Poll(100)) {
        firstRead=false;
        int r=ringBuffer->Read(f);
        if(r<0 && FATALERRNO) {
          if(errno==EOVERFLOW) {
            ringBuffer->Clear();
            printf("ERROR: driver buffer overflow on device %d",cardIndex);
          } else {
            printf("BREAK !!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            LOG_ERROR;
            break;
          }
        }
      }
    }
  }
}

uchar* cDeCsaTSBuffer::GetPackets(int &packetsCount) {
  int Count=0;
  if(delivered) {
    ringBuffer->Del(lastPacketsCount*TS_SIZE);
    delivered=false;
  }
  packetsCount = 0;

  if (ringBuffer->Available()<bs_size*TS_SIZE)
    return NULL;

  uchar* p=ringBuffer->Get(Count);
  if(p && Count>=TS_SIZE) {
    if(*p!=TS_SYNC_BYTE) {
      for(int i=1; i<Count; i++)
        if(p[i]==TS_SYNC_BYTE &&
           (i+TS_SIZE==Count || (i+TS_SIZE>Count && p[i+TS_SIZE]==TS_SYNC_BYTE)) ) {
          Count=i;
          break;
        }
      ringBuffer->Del(Count);
      printf("ERROR: skipped %d bytes to sync on TS packet on device %d",Count,cardIndex);
      return NULL;
    }
    if(!decsa->Decrypt(p,Count,packetsCount)) {
      cCondWait::SleepMs(20);
      return NULL;
    }
    lastPacketsCount = packetsCount;
    delivered=true;
    return p;
  }

  return NULL;
}

// --- cScDevice --------------------------------------------------------------

#define DEV_DVB_ADAPTER  "/dev/dvb/adapter"
#define DEV_DVB_DVR      "dvr"
#define DEV_DVB_CA       "ca"

cScDevice::cScDevice(int Adapter, int Frontend, int cafd)
{
  tsBuffer=0;
  fd_dvr=-1;
  decsa=new cDeCSA(0);
}

cScDevice::~cScDevice()
{
  delete decsa;
}

bool cScDevice::OpenDvr(void)
{
  CloseDvr();
  //fd_dvr=DvbOpen(DEV_DVB_DVR,adapter,frontend,O_RDONLY|O_NONBLOCK,true);
  fd_dvr=DvbOpen(DEV_DVB_DVR,0,0,O_RDONLY|O_NONBLOCK,true);
  if(fd_dvr>=0) {
    tsMutex.Lock();
    tsBuffer = new cDeCsaTSBuffer(fd_dvr,MEGABYTE(10),0+1,decsa);
    tsMutex.Unlock();
  }
  return fd_dvr>=0;
}

void cScDevice::CloseDvr(void)
{
  tsMutex.Lock();
  delete tsBuffer; tsBuffer=0;
  tsMutex.Unlock();
  if(fd_dvr>=0) { close(fd_dvr); fd_dvr=-1; }
}

uchar* cScDevice::GetTSPackets(int &Count) {
  if(tsBuffer)
    return tsBuffer->GetPackets(Count);
  return NULL;
}

bool cScDevice::SetCaDescr(ca_descr_t *ca_descr, bool initial)
{
  return decsa->SetDescr(ca_descr,initial);
}

int cScDevice::DvbOpen(const char *Name, int a, int f, int Mode, bool ReportError)
{
	char FileName[128];
	snprintf(FileName,sizeof(FileName),"%s%d/%s%d",DEV_DVB_ADAPTER,a,Name,f);
	int fd=open(FileName,Mode);
	//if(fd<0 && ReportError) LOG_ERROR_STR(FileName);
	return fd;
}

