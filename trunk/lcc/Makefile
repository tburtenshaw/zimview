# Wedit Makefile for project modem
SRCDIR=e:\programming\lcc\projects\default
CFLAGS=-Ie:\programming\lcc\include  -g2 
CC=$(LCCROOT)\bin\lcc.exe
LINKER=$(LCCROOT)\bin\lcclnk.exe
OBJS=\
	modem.obj \
	modem.res

LIBS=
EXE=modem.exe

$(EXE):	$(OBJS) Makefile
	$(LINKER)  -subsystem console -o $(SRCDIR)\lcc\modem.exe $(OBJS) $(LIBS)

# Build modem.c
MODEM_C=\
	$(SRCDIR)\modemres.h\

modem.obj: $(MODEM_C) $(SRCDIR)\modem.c
	$(CC) -c $(CFLAGS) $(SRCDIR)\modem.c

# Build modem.res
MODEM_RC=\
	$(SRCDIR)\modemres.h\

modem.res:	$(MODEM_RC) $(SRCDIR)\modem.rc
	$(LCCROOT)\bin\lrc.exe -I$(SRCDIR) -Ie:\programming\lcc\include  $(SRCDIR)\modem.rc

link:
	$(LINKER)  -subsystem console -o $(SRCDIR)\lcc\modem.exe $(OBJS) $(LIBS)

clean:
	del $(OBJS) modem.exe
