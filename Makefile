# *********************************************************
# Change this section as needed
# The -g flag is to include debugging information.  It gets
# stripped back out in the install command anyway.
CC	= gcc
CFLAGS	= -g
MANDIR  = /usr/man
BINDIR  = /usr/sbin/
LOGDIR  = /var/log/
INIDIR  = /sbin/init.d/
# *********************************************************

autolog:	autolog.c
	$(CC) $(CFLAGS) -o autolog autolog.c

install:	autolog
	install -m744 -o 0 -g 0 autolog           $(BINDIR)
	install -m744 -o 0 -g 0 autolog.init      $(INIDIR)/autolog
	install -m644 -o 0 -g 0 autolog.conf	  /etc
	install -m644 -o 0 -g 0 autolog.log       $(LOGDIR)
	install -m644          	autolog.conf.5.gz $(MANDIR)/man5
	install -m644		autolog.8.gz      $(MANDIR)/man8

clean:
	rm autolog
