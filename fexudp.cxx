//
// fexudp.cxx
//
// Frontend for receiving and storing UDP packets as MIDAS data banks.
//

#include <stdio.h>
#include <netdb.h> // getnameinfo()
//#include <stdlib.h>
#include <string.h> // memcpy()
#include <errno.h> // errno
//#include <unistd.h>
//#include <time.h>
#include <signal.h> // SIGPIPE
#include <assert.h> // assert()

#include <string>
#include <vector>
#include <mutex>
#include <thread>

#include "midas.h"
#include "tmfe.h"

//#define EQ_NAME "fexudp"

#if 0
#include <sys/time.h>

static double GetTimeSec()
{
   struct timeval tv;
   gettimeofday(&tv,NULL);
   return tv.tv_sec + 0.000001*tv.tv_usec;
}
#endif

struct Source
{
  struct sockaddr addr;
  char bank_name[5];
  std::string host_name;
};

static std::vector<Source> gSrc;

static int gUnknownPacketCount = 0;
static bool gSkipUnknownPackets = false;

int open_udp_socket(int server_port)
{
   int status;
   
   int fd = socket(AF_INET, SOCK_DGRAM, 0);
   
   if (fd < 0) {
      cm_msg(MERROR, "open_udp_socket", "socket(AF_INET,SOCK_DGRAM) returned %d, errno %d (%s)", fd, errno, strerror(errno));
      return -1;
   }

   int opt = 1;
   status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

   if (status == -1) {
      cm_msg(MERROR, "open_udp_socket", "setsockopt(SOL_SOCKET,SO_REUSEADDR) returned %d, errno %d (%s)", status, errno, strerror(errno));
      return -1;
   }

   int bufsize = 64*1024*1024;
   //int bufsize = 20*1024;

   status = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));

   if (status == -1) {
      cm_msg(MERROR, "open_udp_socket", "setsockopt(SOL_SOCKET,SO_RCVBUF) returned %d, errno %d (%s)", status, errno, strerror(errno));
      return -1;
   }

   struct sockaddr_in local_addr;
   memset(&local_addr, 0, sizeof(local_addr));

   local_addr.sin_family = AF_INET;
   local_addr.sin_port = htons(server_port);
   local_addr.sin_addr.s_addr = INADDR_ANY;

   status = bind(fd, (struct sockaddr *)&local_addr, sizeof(local_addr));

   if (status == -1) {
      cm_msg(MERROR, "open_udp_socket", "bind(port=%d) returned %d, errno %d (%s)", server_port, status, errno, strerror(errno));
      return -1;
   }

   int xbufsize = 0;
   unsigned size = sizeof(xbufsize);

   status = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &xbufsize, &size);

   //printf("status %d, xbufsize %d, size %d\n", status, xbufsize, size);

   if (status == -1) {
      cm_msg(MERROR, "open_udp_socket", "getsockopt(SOL_SOCKET,SO_RCVBUF) returned %d, errno %d (%s)", status, errno, strerror(errno));
      return -1;
   }

   cm_msg(MINFO, "open_udp_socket", "UDP port %d socket receive buffer size is %d", server_port, xbufsize);

   return fd;
}

bool addr_match(const Source* s, void *addr, int addr_len)
{
  int v = memcmp(&s->addr, addr, addr_len);
#if 0
  for (int i=0; i<addr_len; i++)
    printf("%3d - 0x%02x 0x%02x\n", i, ((char*)&s->addr)[i], ((char*)addr)[i]);
  printf("match %d, hostname [%s] bank [%s], status %d\n", addr_len, s->host_name.c_str(), s->bank_name, v);
#endif
  return v==0;
}

int wait_udp(int socket, int msec)
{
   int status;
   fd_set fdset;
   struct timeval timeout;

   FD_ZERO(&fdset);
   FD_SET(socket, &fdset);

   timeout.tv_sec = msec/1000;
   timeout.tv_usec = (msec%1000)*1000;

   status = select(socket+1, &fdset, NULL, NULL, &timeout);

#ifdef EINTR
   if (status < 0 && errno == EINTR) {
      return 0; // watchdog interrupt, try again
   }
#endif

   if (status < 0) {
      cm_msg(MERROR, "wait_udp", "select() returned %d, errno %d (%s)", status, errno, strerror(errno));
      return -1;
   }

   if (status == 0) {
      return 0; // timeout
   }

   if (FD_ISSET(socket, &fdset)) {
      return 1; // have data
   }

   // timeout
   return 0;
}

int find_source(Source* src, const sockaddr* paddr, int addr_len)
{
   char host[NI_MAXHOST], service[NI_MAXSERV];
      
   int status = getnameinfo(paddr, addr_len, host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);
      
   if (status != 0) {
      cm_msg(MERROR, "read_udp", "getnameinfo() returned %d (%s), errno %d (%s)", status, gai_strerror(status), errno, strerror(errno));
      return -1;
   }

   char bankname[NAME_LENGTH];
   //int size = sizeof(bankname);

   if (host[0] == 'a' && host[1] == 'd' && host[2] == 'c') {
      int module = atoi(host+3);
      bankname[0] = 'A';
      bankname[1] = 'A';
      bankname[2] = '0' + module/10;
      bankname[3] = '0' + module%10;
      bankname[4] = 0;
   } else if (host[0] == 'p' && host[1] == 'w' && host[2] == 'b') {
      int module = atoi(host+3);
      bankname[0] = 'P';
      bankname[1] = 'B';
      bankname[2] = '0' + module/10;
      bankname[3] = '0' + module%10;
      bankname[4] = 0;
   } else {
      cm_msg(MERROR, "read_udp", "UDP packet from unknown host \"%s\"", host);
      return -1;
   }

#if 0      
   status = db_get_value(hDB, hKeySet, host, bankname, &size, TID_STRING, FALSE);
   
   if (status == DB_NO_KEY) {
      cm_msg(MERROR, "read_udp", "UDP packet from unknown host \"%s\"", host);
      cm_msg(MINFO, "read_udp", "Register this host by running following commands:");
      cm_msg(MINFO, "read_udp", "odbedit -c \"create STRING /Equipment/%s/Settings/%s\"", EQ_NAME, host);
      cm_msg(MINFO, "read_udp", "odbedit -c \"set /Equipment/%s/Settings/%s AAAA\", where AAAA is the MIDAS bank name for this host", EQ_NAME, host);
      return -1;
   } else if (status != DB_SUCCESS) {
      cm_msg(MERROR, "read_udp", "db_get_value(\"/Equipment/%s/Settings/%s\") status %d", EQ_NAME, host, status);
      return -1;
   }

   if (strlen(bankname) != 4) {
      cm_msg(MERROR, "read_udp", "ODB \"/Equipment/%s/Settings/%s\" should be set to a 4 character MIDAS bank name", EQ_NAME, host);
      cm_msg(MINFO, "read_udp", "Use this command:");
      cm_msg(MINFO, "read_udp", "odbedit -c \"set /Equipment/%s/Settings/%s AAAA\", where AAAA is the MIDAS bank name for this host", EQ_NAME, host);
      return -1;
   }
#endif
      
   cm_msg(MINFO, "read_udp", "UDP packets from host \"%s\" will be stored in bank \"%s\"", host, bankname);
      
   src->host_name = host;
   strlcpy(src->bank_name, bankname, 5);
   memcpy(&src->addr, paddr, sizeof(src->addr));
   
   return 0;
}

int read_udp(int socket, char* buf, int bufsize, char* pbankname)
{
   if (wait_udp(socket, 100) < 1)
      return 0;

#if 0
   static int count = 0;
   static double tt = 0;
   double t = GetTimeSec();

   double dt = (t-tt)*1e6;
   count++;
   if (dt > 1000) {
      printf("read_udp: %5d %6.0f usec\n", count, dt);
      count = 0;
   }
   tt = t;
#endif

   struct sockaddr addr;
   socklen_t addr_len = sizeof(addr);
   int rd = recvfrom(socket, buf, bufsize, 0, &addr, &addr_len);
   
   if (rd < 0) {
      cm_msg(MERROR, "read_udp", "recvfrom() returned %d, errno %d (%s)", rd, errno, strerror(errno));
      return -1;
   }

   for (unsigned i=0; i<gSrc.size(); i++) {
      if (addr_match(&gSrc[i], &addr, addr_len)) {
         strlcpy(pbankname, gSrc[i].bank_name, 5);
         //printf("rd %d, bank [%s]\n", rd, pbankname);
         return rd;
      }
   }

   if (gSkipUnknownPackets)
      return -1;

   Source sss;

   int status = find_source(&sss, &addr, addr_len);

   if (status < 0) {

      gUnknownPacketCount++;

      if (gUnknownPacketCount > 10) {
         gSkipUnknownPackets = true;
         cm_msg(MERROR, "read_udp", "further messages are now suppressed...");
         return -1;
      }

      return -1;
   }

   gSrc.push_back(sss);
         
   strlcpy(pbankname, sss.bank_name, 5);
         
   return rd;
}

int udp_begin_of_run()
{
   gUnknownPacketCount = 0;
   gSkipUnknownPackets = false;
   return SUCCESS;
}

struct UdpPacket
{
   char bank_name[5];
   std::vector<char> data;
};

std::vector<UdpPacket*> gUdpPacketBuf;
std::mutex gUdpPacketBufLock;
int gMaxBufferPackets = 10000;
int gCountDroppedPackets = 0;
int gMaxBuffered = 0;

class UdpReader
{
public:
   TMFE* fMfe = NULL;
   TMFeEquipment* fEq = NULL;
   std::thread* fUdpReadThread = NULL;
   int fDataSocket = 0;

public:
   UdpReader(TMFE* mfe, TMFeEquipment* eq)
   {
      fMfe = mfe;
      fEq = eq;
   }

   bool OpenSocket(int udp_port)
   {
      fDataSocket = open_udp_socket(udp_port);
   
      if (fDataSocket < 0) {
         printf("frontend_init: cannot open udp socket\n");
         cm_msg(MERROR, "frontend_init", "Cannot open UDP socket for port %d", udp_port);
         return false;
      }

      return true;
   }

   void UdpReadThread()
   {
      printf("UdpReader thread started\n");

      UdpPacket* p = NULL;

      std::vector<UdpPacket*> buf;

      while (!fMfe->fShutdown) {
         const int packet_size = 1500;

         if (p == NULL) {
            p = new UdpPacket;
            p->data.resize(packet_size);
         }
         
         int length = read_udp(fDataSocket, p->data.data(), packet_size, p->bank_name);
         assert(length < packet_size);
         //printf("%d.", length);
         if (length > 0) {
            p->data.resize(length);
            buf.push_back(p);
            p = NULL;
         }

         if ((p != NULL) || (buf.size() > 1000)) {

            //printf("flush %d\n", (int)buf.size());

            {
               std::lock_guard<std::mutex> lock(gUdpPacketBufLock);

               int size = buf.size();
               for (int i=0; i<size; i++) {
                  UdpPacket* pp = buf[i];
                  buf[i] = NULL;
                  int xsize = gUdpPacketBuf.size();
                  if (xsize > gMaxBuffered)
                     gMaxBuffered = xsize;
                  if (xsize < gMaxBufferPackets) {
                     gUdpPacketBuf.push_back(pp);
                     pp = NULL;
                  } else {
                     gCountDroppedPackets++;
                  }
                  if (pp)
                     free(pp);
               }
            }

            buf.clear();
         }
      }

      printf("UdpReader thread shutdown\n");
   }

   void StartThread()
   {
      assert(fUdpReadThread == NULL);
      fUdpReadThread = new std::thread(&UdpReader::UdpReadThread, this);
   }

   void JoinThreads()
   {
      if (fUdpReadThread) {
         fUdpReadThread->join();
         delete fUdpReadThread;
         fUdpReadThread = NULL;
      }
   }
};

class Xudp : public TMFeRpcHandlerInterface
{
public:
   TMFE* fMfe = NULL;
   TMFeEquipment* fEq = NULL;
   std::vector<UdpReader*> fUdpReaders;

   void AddReader(UdpReader* r)
   {
      fUdpReaders.push_back(r);
   }

   void ThreadPeriodic()
   {
   }

   std::string HandleRpc(const char* cmd, const char* args)
   {
      fMfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);
      return "OK";
   }

   void BeginRun(bool start)
   {
      fMfe->Msg(MINFO, "BeginRun", "Begin run begin!");
      gCountDroppedPackets = 0;
      gMaxBuffered = 0;
   }

   void HandleBeginRun()
   {
      fMfe->Msg(MINFO, "HandleBeginRun", "Begin run!");
      BeginRun(true);
      udp_begin_of_run();
   }

   void HandleEndRun()
   {
      fMfe->Msg(MINFO, "HandleEndRun", "End run begin!");
      fEq->WriteStatistics();
   }
};

int main(int argc, char* argv[])
{
   setbuf(stdout, NULL);
   setbuf(stderr, NULL);

   signal(SIGPIPE, SIG_IGN);

   TMFE* mfe = TMFE::Instance();

   TMFeError err = mfe->Connect("fexudp");
   if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
   }

   //mfe->SetWatchdogSec(0);

   TMFeCommon *eqc = new TMFeCommon();
   eqc->EventID = 1;
   eqc->FrontendName = "fexudp";
   eqc->LogHistory = 0;
   eqc->Buffer = "BUFUDP";
   
   TMFeEquipment* eq = new TMFeEquipment("XUDP");
   eq->Init(mfe->fOdbRoot, eqc);
   eq->SetStatus("Starting...", "white");

   mfe->RegisterEquipment(eq);

   eq->WriteStatistics();

   Xudp* xudp = new Xudp;

   xudp->fMfe = mfe;
   xudp->fEq = eq;

   mfe->RegisterRpcHandler(xudp);
   //mfe->SetTransitionSequence(910, 90, -1, -1);

   int udp_port = 50006;
   int udp_port_adc = 50007;
   int udp_port_pwb = 50008;
   int udp_port_pwb_a = 50001;
   int udp_port_pwb_b = 50002;
   int udp_port_pwb_c = 50003;
   int udp_port_pwb_d = 50004;

   xudp->fEq->fOdbEqSettings->RI("udp_port", 0, &udp_port, true);
   xudp->fEq->fOdbEqSettings->RI("udp_port_adc", 0, &udp_port_adc, true);
   xudp->fEq->fOdbEqSettings->RI("udp_port_pwb", 0, &udp_port_pwb, true);

   xudp->fEq->fOdbEqSettings->RI("udp_port_pwb_a", 0, &udp_port_pwb_a, true);
   xudp->fEq->fOdbEqSettings->RI("udp_port_pwb_b", 0, &udp_port_pwb_b, true);
   xudp->fEq->fOdbEqSettings->RI("udp_port_pwb_c", 0, &udp_port_pwb_c, true);
   xudp->fEq->fOdbEqSettings->RI("udp_port_pwb_d", 0, &udp_port_pwb_d, true);

   xudp->fEq->fOdbEqSettings->RI("max_buffer_packets", 0, &gMaxBufferPackets, true);

   UdpReader* r = new UdpReader(mfe, eq);
   r->OpenSocket(udp_port);
   r->StartThread();

   xudp->AddReader(r);

   UdpReader* radc = new UdpReader(mfe, eq);
   radc->OpenSocket(udp_port_adc);
   radc->StartThread();

   xudp->AddReader(radc);

   UdpReader* rpwb = new UdpReader(mfe, eq);
   rpwb->OpenSocket(udp_port_pwb);
   rpwb->StartThread();
   xudp->AddReader(rpwb);

   UdpReader* rpwb_a = new UdpReader(mfe, eq);
   rpwb_a->OpenSocket(udp_port_pwb_a);
   rpwb_a->StartThread();
   xudp->AddReader(rpwb_a);

   UdpReader* rpwb_b = new UdpReader(mfe, eq);
   rpwb_b->OpenSocket(udp_port_pwb_b);
   rpwb_b->StartThread();
   xudp->AddReader(rpwb_b);

   UdpReader* rpwb_c = new UdpReader(mfe, eq);
   rpwb_c->OpenSocket(udp_port_pwb_c);
   rpwb_c->StartThread();
   xudp->AddReader(rpwb_c);

   UdpReader* rpwb_d = new UdpReader(mfe, eq);
   rpwb_d->OpenSocket(udp_port_pwb_d);
   rpwb_d->StartThread();
   xudp->AddReader(rpwb_d);


   eq->SetStatus("Started readers...", "white");

   {
      int run_state = 0;
      mfe->fOdbRoot->RI("Runinfo/State", 0, &run_state, false);
      bool running = (run_state == 3);
      if (running) {
         xudp->HandleBeginRun();
      } else {
         xudp->BeginRun(false);
      }
   }

   printf("init done!\n");

   time_t next_periodic = time(NULL) + 1;

   while (!mfe->fShutdown) {
      time_t now = time(NULL);

      if (now > next_periodic) {
         next_periodic += 5;

         int buffered = 0;

         {
            std::lock_guard<std::mutex> lock(gUdpPacketBufLock);
            buffered = (int)gUdpPacketBuf.size();
         }
         
         {
            char buf[256];
            sprintf(buf, "buffered %d (max %d), dropped %d", buffered, gMaxBuffered, gCountDroppedPackets);
            eq->SetStatus(buf, "#00FF00");
         }

         eq->WriteStatistics();
      }

      {
         std::vector<UdpPacket*> buf;

         {
            std::lock_guard<std::mutex> lock(gUdpPacketBufLock);
            int size = gUdpPacketBuf.size();
            //printf("Have events: %d\n", size);
            for (int i=0; i<size; i++) {
               buf.push_back(gUdpPacketBuf[i]);
               gUdpPacketBuf[i] = NULL;
            }
            gUdpPacketBuf.clear();
         }

         if (buf.size() > 0) {
            const int event_size = 30*1024*1024;
            static char event[event_size];

            while (!mfe->fShutdown) {
               eq->ComposeEvent(event, event_size);
               eq->BkInit(event, sizeof(event));

               int num_banks = 0;
               for (unsigned i=0; i<buf.size(); i++) {
                  if (buf[i]) {
                     UdpPacket* p = buf[i];
                     buf[i] = NULL;
                  
                     char* xptr = (char*)eq->BkOpen(event, p->bank_name, TID_BYTE);
                     char* ptr = xptr;
                     int size = p->data.size();
                     memcpy(ptr, p->data.data(), size);
                     ptr += size;
                     eq->BkClose(event, ptr);
                     num_banks ++;
                     
                     delete p;

                     //printf("event size %d.", eq->BkSize(event));

                     if (num_banks >= 8000) {
                        break;
                     }
                  }
               }

               //printf("send %d banks, %d bytes\n", num_banks, eq->BkSize(event));

               if (num_banks == 0) {
                  break;
               }
               
               eq->SendEvent(event);

               static time_t next_update = 0;
               time_t now = time(NULL);
               if (now > next_update) {
                  next_update = now + 5;
                  eq->WriteStatistics();
                  mfe->MidasPeriodicTasks();
               }
            }
            
            buf.clear();

            eq->WriteStatistics();
            mfe->MidasPeriodicTasks();
         }
      }

      //printf("*** POLL MIDAS ***\n");

      mfe->PollMidas(10);
      if (mfe->fShutdown)
         break;
   }

   mfe->Disconnect();

   return 0;
}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
