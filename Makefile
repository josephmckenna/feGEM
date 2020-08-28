INC_DIR   = $(MIDASSYS)/include
LIB_DIR   = $(MIDASSYS)/lib
SRC_DIR   = $(MIDASSYS)/src

CC     = gcc
CFLAGS = -Wall -O2 -g -Wall -DOS_LINUX -Dextname
CFLAGS += -std=c++11 -Wall -O2 -g -I. -I$(INC_DIR) -I$(MIDASSYS)/mxml/ -I$(MIDASSYS)/mvodb/ -I$(MIDASSYS)/mjson/
CFLAGS += $(PGFLAGS)
LIBS = -lm -lz -lutil -lnsl -lpthread
LIB = $(LIB_DIR)/libmidas.a -lrt

#MODBUS_DIR = $(MIDASSYS)/drivers/divers
#CFLAGS += -I$(MODBUS_DIR)

MODULES = $(LIB_DIR)/mfe.o

all::feGEM.exe

feGEM.exe: %.exe: %.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIB) $(LDFLAGS) $(LIBS) -lssl -lcrypto




%.o: %.cxx
	$(CXX) -o $@ $(CFLAGS) -c $<

clean::
	-rm -f *.o *~ \#* *.exe

#end
