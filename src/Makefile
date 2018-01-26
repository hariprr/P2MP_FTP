.PHONY: p2mp
p2mp : 
	rm -f p2mp
	rm -f sender 
	gcc reliable_udp.c -o p2mp
	gcc sender.c linklist.c -o sender

OBJS = p2mp

all: ${OBJS}
