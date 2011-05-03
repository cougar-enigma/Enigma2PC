CFLAGS = -O2 -pthread
LIBS =  -lpthread -lrt -lnl -lnl-genl -ldvbcsa

all: descr

descr: descr.o ringbuffer.o thread.o tools.o ca_netlink.o csa_decode.o decoder.o
	g++ $(CFLAGS) -o descr descr.o ringbuffer.o thread.o tools.o ca_netlink.o csa_decode.o decoder.o $(LIBS)

descr.o: descr.c
	g++ -c $(CFLAGS) descr.c
ringbuffer.o: ringbuffer.c
	g++ -c $(CFLAGS) ringbuffer.c
thread.o: thread.c
	g++ -c $(CFLAGS) thread.c -pthread
tools.o: tools.c
	g++ -c $(CFLAGS) tools.c
ca_netlink.o: ca_netlink.c
	g++ -c $(CFLAGS) ca_netlink.c
csa_decode.o: csa_decode.c
	g++ -c $(CFLAGS) csa_decode.c
decoder.o: decoder.c
	g++ -c $(CFLAGS) decoder.c
clean:
	rm -f *.o
	rm -f descr
