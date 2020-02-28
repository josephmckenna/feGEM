INC_DIR   = $(MIDASSYS)/include
LIB_DIR   = $(MIDASSYS)/linux/lib
SRC_DIR   = $(MIDASSYS)/src

CC     = gcc
CFLAGS = -Wall -O2 -g -Wall -DOS_LINUX -Dextname
CFLAGS += -std=c++11 -Wall -O2 -g -I. -I$(INC_DIR) -I$(MIDASSYS)/../mxml/
CFLAGS += $(PGFLAGS)
LIBS = -lm -lz -lutil -lnsl -lpthread
LIB = $(LIB_DIR)/libmidas.a -lrt

#MODBUS_DIR = $(MIDASSYS)/drivers/divers
#CFLAGS += -I$(MODBUS_DIR)

MODULES = $(LIB_DIR)/mfe.o

#all:: fealpha16.exe
all:: feevb.exe
all:: fexudp.exe
#all:: feyair.exe
all:: fecaenr14xxet.exe
all:: fewienerlvps.exe
all:: femoxa.exe
all:: fegastelnet.exe
all:: fectrl.exe
#all:: feice450.exe
all:: test_tcp.exe
#all:: grifc.exe
all:: mlu_gen.exe
all::febvlvdb.exe

#fealpha16.exe: $(LIB) $(LIB_DIR)/mfe.o fealpha16.o mscbcxx.o
#	$(CXX) -o $@ $(CFLAGS) $^ $(LIB) $(LDFLAGS) $(LIBS)

#grifc.exe: grifc.o GrifComm.o tmfe.o tmodb.o
#	$(CXX) -o $@ $(CFLAGS) $^ $(LIB) $(LDFLAGS) $(LIBS)

mlu_gen.exe: mlu_gen.o
	$(CXX) -o $@ $(CFLAGS) $^

feevb.exe: $(LIB) $(LIB_DIR)/mfe.o feevb.o TsSync.o tmodb.o tmfe.o
	$(CXX) -o $@ $(CFLAGS) $^ $(LIB) $(LDFLAGS) $(LIBS)

fexudp.exe: $(LIB) fexudp.o tmodb.o tmfe.o
	$(CXX) -o $@ $(CFLAGS) $^ $(LIB) $(LDFLAGS) $(LIBS)

#feyair.exe: %.exe: %.o KOsocket.o tmfe.o
#	$(CXX) -o $@ $^ $(CFLAGS) $(LIB) $(LDFLAGS) $(LIBS)

fecaenr14xxet.exe: %.exe: %.o KOtcp.o tmfe.o tmodb.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIB) $(LDFLAGS) $(LIBS)

fewienerlvps.exe: %.exe: %.o tmfe.o tmodb.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIB) $(LDFLAGS) $(LIBS)

femoxa.exe: %.exe: %.o KOtcp.o tmfe.o tmodb.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIB) $(LDFLAGS) $(LIBS)

fegastelnet.exe: %.exe: %.o KOtcp.o tmfe.o tmodb.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIB) $(LDFLAGS) $(LIBS)

fectrl.exe: %.exe: %.o KOtcp.o tmfe.o tmodb.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIB) $(LDFLAGS) $(LIBS)

febvlvdb.exe: KOtcp.o JsonTo.o EsperComm.o febvlvdb.o tmfe.o tmodb.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIB) $(LDFLAGS) $(LIBS)

#feice450.exe: %.exe: %.o tmfe.o tmodb.o
#	$(CXX) -o $@ $^ $(CFLAGS) $(LIB) $(LDFLAGS) $(LIBS)

test_tcp.exe: KOtcp.cxx
	$(CXX) -o $@ -DMAIN $^ $(CFLAGS) $(LIB) $(LDFLAGS) $(LIBS)

#modbus.exe: %.exe: %.o ModbusTcp.o
#	$(CXX) -o $@ $^ $(CFLAGS) $(LIB) $(LDFLAGS) $(LIBS)
#
#ModbusTcp.o: %.o: $(MODBUS_DIR)/%.cxx
#	$(CXX) -o $@ $(CFLAGS) -c $<

%.o: %.cxx
	$(CXX) -o $@ $(CFLAGS) -c $<

clean::
	-rm -f *.o *~ \#* *.exe

#end
