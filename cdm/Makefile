# Makefile for CDM MIDAS frontend

EMCRAFT_ROOT = /home/agdaq/online/firmware/cdm/emcraft
CROSS_COMPILE_APPS = $(EMCRAFT_ROOT)/tools/arm-2010q1/bin/arm-uclinuxeabi-

# These are flags/tools used to build user-space programs
CFLAGS		:= -Os -mcpu=cortex-m3 -mthumb -I. -Wall
LDFLAGS		:= -mcpu=cortex-m3 -mthumb
CC		= $(CROSS_COMPILE_APPS)gcc
CXX		= $(CROSS_COMPILE_APPS)g++
#LIBS            = -L$(EMCRAFT_ROOT)/arm-uclinuxeabi/libc/usr/lib

DEPS		= lmk04800.h tmfe.h tmvodb.h

CFLAGS += -std=c++0x
CFLAGS += -DHAVE_MIDAS -I$(MIDASSYS)/include
LIBS += -L$(MIDASSYS)/linux-emcraft/lib -lmidas
LIBS += -L$(EMCRAFT_ROOT)/A2F/root/usr/lib -lpthread
LIBS += -lrt

all:: fecdm.exe

fecdm.exe: fecdm.o lmk04800.o tmfe.o tmodb.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

#%.o: %.c $(DEPS)
#	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.cxx $(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)

clean::
	-rm -f *.exe *.gdb *.o

#end
