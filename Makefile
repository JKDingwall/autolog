# *********************************************************
# Change this section as needed
# The -g flag is to include debugging information.  It gets
# stripped back out in the install command anyway.
CC      ?= gcc
CFLAGS  += -Werror=implicit-function-declaration
CFLAGS  += -D_REGEX_RE_COMP=1
#CFLAGS += -g
MANDIR  = $(DESTDIR)/usr/share/man
BINDIR  = $(DESTDIR)/usr/sbin/
STATEDIR = $(DESTDIR)/var/lib/autolog/
CONFDIR = $(DESTDIR)/etc/
INIDIR  = $(DESTDIR)/etc/init.d/
# *********************************************************

autolog:	autolog.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o autolog autolog.c

install:	autolog
	install -d $(BINDIR) $(INIDIR) $(CONFDIR) $(STATEDIR) $(LOGDIR) $(MANDIR)
	install -m744 -o 0 -g 0 autolog           $(BINDIR)
#	install -m744 -o 0 -g 0 autolog.init      $(INIDIR)/autolog
	install -m644 -o 0 -g 0 autolog.conf      $(CONFDIR)

clean:
	rm -f autolog
