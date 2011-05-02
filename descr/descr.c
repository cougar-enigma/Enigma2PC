/*
 * descr.c
 *
 * DVBSoftwareCA - Copyright cougar 2011
 *
 * Portions of this code also have copyright:
 * Video Disk Recorder - Copyright (C) 2000, 2003, 2006, 2008 Klaus Schmidinger
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 *
 * This code is based on the Video Disk Recorder and Softcam plugin to VDR
 *
 */

#include <sched.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include "ringbuffer.h"
#include "ca_netlink.h"
#include "csa_decode.h"
#include "decoder.h"


static void hexdump (const char *str, const void *data, unsigned int len) {
  const uint8_t	*p = (uint8_t*)data;
  unsigned int	i;

  printf("- %s -\n", str);
  for (i = 0; i < len; i++)
    printf("0x%02x,%c", p[i], (i+1)%16 ? ' ' : '\n');
  puts("");
}

#define FIFO_FILE "/tmp/ENIGMA_FIFO"
cDecoder* example_decoder;

void signal_handler(int signum) {
	example_decoder->Restart();
	printf("SIGNAL %d\n", signum);
}

int main() {
  int ret;
  int fd_out;
  int counter = 0;
  struct sched_param sp;
  unsigned char *data;

  sp.sched_priority = 1;
  sched_setscheduler (0, SCHED_FIFO, &sp);

  signal (SIGPIPE, signal_handler);

  ret = ca_netlink_init();
  if (ret<0) {
    printf("Creating generic netlink error\n");
    return 1;
  }

  example_decoder = new cDecoder(MEGABYTE(2));

  umask(0);
  mknod(FIFO_FILE, S_IFIFO|0666, 0);

  while(1) {
    printf("Try to open fifo\n");
    fd_out = open(FIFO_FILE, O_WRONLY);
    printf("FIFO Opened %d\n", fd_out);

    while(1) {
      data = NULL;
      int len;
      data = example_decoder->GetData(len);
      if (data && len>0) {
        ret = safe_write(fd_out, data, len);
        example_decoder->DelData(len);
        if (ret<0)
          break;

        counter++;
        if (counter%1000==0)
          printf("fifo_counter %d\n", counter);

      } else {
        usleep(10000);
      }
    }

    close(fd_out);
  }

  ca_netlink_close();

  return 0;
}

