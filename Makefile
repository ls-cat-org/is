VERSION= 0.20

all: is mh

is: is.c is.h mr.c isGlobals.c adsc.c imtype.c mar345.c
	gcc -Wall -o is is.c isGlobals.c mr.c adsc.c imtype.c mar345.c  -ltiff -ljpeg -lm

mh: mh.c
	gcc -Wall -o mh mh.c

clean:
	rm -f is *.jpeg *.o *~

dist:
	ln -fs . is-$(VERSION)
	tar czvf is-$(VERSION).tar.gz is-$(VERSION)/*.c is-$(VERSION)/*.h is-$(VERSION)/Makefile is-$(VERSION)/isWrap is-$(VERSION)/etcService is-$(VERSION)/carpsIS
	cd is-$(VERSION)
	rm -f is-$(VERSION)

install: is
	install mh /usr/local/bin
	install is /usr/local/bin
	install isWrap /usr/local/bin
	install mhWrap
	grep -q 14850/tcp /etc/services || echo "Please append etcService to /etc/services"
	grep -q 14851/tcp /etc/services || echo "Please append etcService to /etc/services"
	if [ -d /etc/xinetd.d ]; then cp carpsIS /etc/xinetd.d; echo "Please restart xinetd"; fi
	if [ -d /etc/xinetd.d ]; then cp mhxml   /etc/xinetd.d; echo "Please restart xinetd"; fi

