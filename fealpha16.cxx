//
// fealpha16.cxx
//
// Frontend for configuration and monitoring of GRIF16/ALPHA16 boards.
//

#include <stdio.h>
#include <netdb.h> // getnameinfo()
//#include <stdlib.h>
#include <string.h> // memcpy()
#include <errno.h> // errno
//#include <unistd.h>
//#include <time.h>

#include <string>
#include <vector>

#include "midas.h"
#include "msystem.h"
#include "mjson.h"

const char *frontend_name = "fealpha16";                 /* fe MIDAS client name */
const char *frontend_file_name = __FILE__;               /* The frontend file name */

extern "C" {
   BOOL frontend_call_loop = TRUE;       /* frontend_loop called periodically TRUE */
   int display_period = 0;               /* status page displayed with this freq[ms] */
   int max_event_size = 1*1024*1024;     /* max event size produced by this frontend */
   int max_event_size_frag = 5 * 1024 * 1024;     /* max for fragmented events */
   int event_buffer_size = 8*1024*1024;           /* buffer size to hold events */
}

extern "C" {
  int interrupt_configure(INT cmd, INT source, PTYPE adr);
  INT poll_event(INT source, INT count, BOOL test);
  int frontend_init();
  int frontend_exit();
  int begin_of_run(int run, char *err);
  int end_of_run(int run, char *err);
  int pause_run(int run, char *err);
  int resume_run(int run, char *err);
  int frontend_loop();
  int read_event(char *pevent, INT off);
}

#ifndef EQ_NAME
#define EQ_NAME "ALPHA16"
#endif

#ifndef EQ_EVID
#define EQ_EVID 1
#endif

EQUIPMENT equipment[] = {
   { EQ_NAME,                         /* equipment name */
      {EQ_EVID, 0, "SYSTEM",          /* event ID, trigger mask, Evbuf */
       EQ_MULTITHREAD, 0, "MIDAS",    /* equipment type, EventSource, format */
       TRUE, RO_ALWAYS,               /* enabled?, WhenRead? */
       50, 0, 0, 0,                   /* poll[ms], Evt Lim, SubEvtLim, LogHist */
       "", "", "",}, read_event,      /* readout routine */
   },
   {""}
};
////////////////////////////////////////////////////////////////////////////

static HNDLE hDB;

#include "mscbcxx.h"

void alpha16_info(MscbSubmaster* s)
{
   int size;
   char buf[256];

   printf("ALPHA16 MSCB submaster %s\n", s->GetName().c_str());

   s->Read(0, 0, buf, sizeof(buf), &size);
   printf("    MAC address: %s\n", buf);

   s->Read(1, 2, buf, sizeof(buf), &size);
   printf("    sp_frun: %d\n", buf[0]);

   int temperature;
   s->Read(1, 5, &temperature, 4, &size);
   printf("    cpu_temp: %d\n", temperature);

   int nim_sta;
   int nim_cnt;
   int est_sta;
   int est_cnt;

   s->Read(1, 50, &nim_sta, 4, &size);
   s->Read(1, 51, &nim_cnt, 4, &size);
   s->Read(1, 52, &est_sta, 4, &size);
   s->Read(1, 53, &est_cnt, 4, &size);

   printf("    NIM: level %d, count %d, ESATA: level %d, count %d\n", nim_sta, nim_cnt, est_sta, est_cnt);

   int f_esata;
   int f_adc;

   s->Read(1, 47, &f_esata, 4, &size);
   s->Read(1, 49, &f_adc, 4, &size);

   printf("    Clock freq: ESATA: %d, ADC: %d\n", f_esata, f_adc);

   s->Read(1, 109, buf, sizeof(buf), &size);
   printf("    UDP Dest IP: %s\n", buf);

   s->Read(1, 111, buf, sizeof(buf), &size);
   printf("    UDP Dest MAC: %s\n", buf);

   uint16_t dst_prt;
   s->Read(1, 113, &dst_prt, 2, &size);
   printf("    UDP Dest port:  %d\n", dst_prt);

   int udp_on;
   s->Read(1, 115, &udp_on, 4, &size);
   printf("    UDP on:  %d\n", udp_on);

   int udp_cnt;
   s->Read(1, 118, &udp_cnt, 4, &size);
   printf("    UDP count:  %d\n", udp_cnt);

   for (int i=0; i<16; i++) {
      printf("  ADC%02d: ", i);

      int a_fgain;
      int t_on;
      int t_tdly;
      int w_tpnt;
      int w_spnt;

      s->Read(2+i, 0, &a_fgain, 4, &size);
      s->Read(2+i, 3, &t_on,    4, &size);
      s->Read(2+i, 4, &t_tdly,  4, &size);
      s->Read(2+i, 5, &w_tpnt,  4, &size); // w_tpnt
      s->Read(2+i, 6, &w_spnt,  4, &size);
      printf(" %d %d %d %d %d", a_fgain, t_on, t_tdly, w_tpnt, w_spnt);

      printf("\n");
   }
}

struct Alpha16Config
{
   bool sata_clock;
   bool sata_trigger;
   bool udp_enable;
   std::string udp_dest_ip;
   uint16_t    udp_dest_port;

   int waveform_samples;
   int waveform_pre_trigger;

   int t_tdly;

   Alpha16Config() // ctor
   {
      sata_clock = false;
      sata_trigger = false;
      udp_enable = false;
      udp_dest_port = 50006;

      waveform_samples = 63;
      waveform_pre_trigger = 16;
      t_tdly = 0;
   }
};

struct Alpha16Submaster
{
   MscbSubmaster* s;
   Alpha16Config* c;

   Alpha16Submaster(MscbSubmaster* xs, Alpha16Config* xc) // ctor
   {
      s = xs;
      c = xc;
   }

   ~Alpha16Submaster() // dtor
   {
      if (s)
         delete s;
      s = NULL;

      // FIXME: Alpha16Config::c is shared between all boards
      c = NULL;
   }

   void StartRun()
   {
      int one = 1;
      s->Write(1, 2, &one, 4);
   }

   void StopRun()
   {
      int zero = 0;
      s->Write(1, 2, &zero, 4);
   }

   void Init()
   {
      int one = 1;
      int zero = 0;
      //char buf[256];

      //int nim_ena = 1;
      //int esata_ena = 0;

      int lmk_sel_clkin2_clock = 2;
      int lmk_sel_esata_clock = 1;

      printf("ALPHA16 at MSCB submaster %s, initializing...\n", s->GetName().c_str());

      StopRun();

      if (c->sata_clock)
         s->Write(1, 18, &lmk_sel_esata_clock, 4);
      else
         s->Write(1, 18, &lmk_sel_clkin2_clock, 4);

      if (c->sata_trigger)
         s->Write(1, 21, &zero, 4); // nim_ena
      else
         s->Write(1, 21, &one, 4); // nim_ena
      s->Write(1, 22, &one, 4); // nim_inv
      if (c->sata_trigger)
         s->Write(1, 23, &one, 4); // est_ena
      else
         s->Write(1, 23, &zero, 4); // est_ena
      s->Write(1, 24, &zero, 4); // est_inv

      s->Write(1, 115, &zero, 4); // udp_on

      if (c->udp_enable) {
         s->Write(1, 116, &one, 4); // udp_rst
         s->Write(1, 116, &zero, 4); // udp_rst
         s->Write(1, 109, c->udp_dest_ip.c_str(), 16); // dst_ip
         s->Write(1, 113, &c->udp_dest_port, 2); // dst_prt
         s->Write(1, 115, &one, 4); // udp_on
      }
   
      for (int i=0; i<16; i++) {
         //int a_fgain;
         //int t_on;
         
         //s->Read(2+i, 0, &a_fgain, 4, &size);
         //s->Read(2+i, 3, &t_on, 4, &size);
         s->Write(2+i, 4, &c->t_tdly, 4); // t_tdly
         s->Write(2+i, 5, &c->waveform_pre_trigger, 4); // w_tpnt
         s->Write(2+i, 6, &c->waveform_samples, 4); // w_spnt
      }
      
      printf("done.\n");
   }
};

MscbDriver *gMscb = NULL;

int interrupt_configure(INT cmd, INT source, PTYPE adr)
{
   return SUCCESS;
}

struct BoardSet
{
   Alpha16Config fConfig;
   std::vector<std::string> fIpList;
};

class TMFeInterface
{
public:
   TMFeInterface(); // ctor
   virtual ~TMFeInterface(); // dtor

public:
   virtual int Init() { return SUCCESS; };
   virtual int Exit() { return SUCCESS; };
public:
   virtual int Thread() { return SUCCESS; };
public:
   virtual int BegunRun(int run_number) { return SUCCESS; };
   virtual int EndRun() { return SUCCESS; };
   virtual int PauseRun() { return SUCCESS; };
   virtual int ResumeRun() { return SUCCESS; };
};

TMFeInterface::TMFeInterface() // ctor
{
};

TMFeInterface::~TMFeInterface() // dtor
{
};

class Fealpha16: public TMFeInterface
{
public:
   BoardSet fExpt;
   BoardSet fOneWire;
   std::vector<Alpha16Submaster*> fDevices;

public:
   int Init();
   int Thread();

   int BeginRun(int num_number);
   int EndRun();

public:
   int Start();
   int Stop();
};

int Fealpha16::Init()
{
   int status;

   cm_set_transition_sequence(TR_START,  900);
   cm_set_transition_sequence(TR_RESUME, 900);
   cm_set_transition_sequence(TR_STOP,   100);
   cm_set_transition_sequence(TR_PAUSE,  100);

   status = cm_get_experiment_database(&hDB, NULL);
   if (status != CM_SUCCESS) {
      cm_msg(MERROR, "frontend_init", "Cannot connect to ODB, cm_get_experiment_database() returned %d", status);
      return FE_ERR_ODB;
   }

   std::string path;
   path += "/Equipment";
   path += "/";
   path += EQ_NAME;
   path += "/Settings";

   gMscb = new MscbDriver();
   status = gMscb->Init();
   printf("MSCB::Init: status %d\n", status);

   // normal configuration

   fExpt.fConfig.sata_clock = true;
   fExpt.fConfig.sata_trigger = true;

   fExpt.fConfig.udp_enable = true;
   fExpt.fConfig.udp_dest_ip = "192.168.1.1";

   fExpt.fConfig.t_tdly = 0;
   fExpt.fConfig.waveform_samples = 700;
   fExpt.fConfig.waveform_pre_trigger = 150;

   // special configuration for 1-wire TPC test

   fOneWire.fConfig.udp_enable = true;
   fOneWire.fConfig.udp_dest_ip = "142.90.111.69";

   for (int i=1; i<=20; i++) {
      std::string name = path + "/enable/mod" + std::to_string(i);
      std::string ip = "192.168.1." + std::to_string(100+i);

      int enabled = 0;
      int size = sizeof(enabled);

      status = db_get_value(hDB, 0, name.c_str(), &enabled, &size, TID_BOOL, TRUE);
      printf("name [%s] ip [%s] status %d, enabled %d\n", name.c_str(), ip.c_str(), status, enabled);

      if (enabled) {
         fExpt.fIpList.push_back(ip);
      }
   }

   for (unsigned i=0; i<fExpt.fIpList.size(); i++) {
      const char* ip = fExpt.fIpList[i].c_str();
      MscbSubmaster* sm = gMscb->GetEthernetSubmaster(ip);

      status = sm->Open();
      printf("ADC %s: MSCB::Open: status %d\n", ip, status);
      
      if (status != SUCCESS) {
         cm_msg(MERROR, "frontend_init", "ALPHA16[%s] Open error %d\n", ip, status);
         continue;
      }

      status = sm->Ping(1,1);

      if (status != SUCCESS) {
         cm_msg(MERROR, "frontend_init", "ALPHA16[%s] Ping error %d\n", ip, status);
         continue;
      }

      status = sm->Init();

      if (status != SUCCESS) {
         cm_msg(MERROR, "frontend_init", "ALPHA16[%s] Init error %d\n", ip, status);
         continue;
      }

      status = sm->ScanPrint(0, 100);
      printf("ADC %s: MSCB::ScanPrint: status %d\n", ip, status);
      
      if (status != SUCCESS)
         continue;

      Alpha16Submaster* a16 = new Alpha16Submaster(sm, &fExpt.fConfig);
      fDevices.push_back(a16);
   }

#if 0
   MscbSubmaster* adczz = gMscb->GetEthernetSubmaster("daqtmp2");

   status = adczz->Init();
   printf("MSCB::adczz::Init: status %d\n", status);

   if (status == SUCCESS) {
      status = adczz->ScanPrint(0, 100);
      printf("MSCB::adczz::ScanPrint: status %d\n", status);

      gAlpha16list.push_back(new Alpha16Submaster(adczz, &czz));
   }
#endif

   Stop();

   for (unsigned i=0; i<fDevices.size(); i++)
      fDevices[i]->Init();
   
   for (unsigned i=0; i<fDevices.size(); i++)
      alpha16_info(fDevices[i]->s);

   cm_msg(MINFO, "frontend_init", "Initialized %d ALPHA16 boards.", (int)fDevices.size());

   int run_state = 0;
   int size = sizeof(run_state);
   status = db_get_value(hDB, 0, "/Runinfo/State", &run_state, &size, TID_INT, FALSE);
   
   if (status != DB_SUCCESS) {
      cm_msg(MERROR, "frontend_init", "Cannot read /Runinfo/State, db_get_value() returned %d", status);
      return FE_ERR_ODB;
   }

   if (run_state == STATE_RUNNING) {
      Start();
   }

   cm_msg(MINFO, "frontend_init", "Frontend equipment \"%s\" is ready.", EQ_NAME);
   
   return SUCCESS;
}

double GetDouble(MJsonNode* n, const char* name)
{
   if (!n)
      return 0;
   const MJsonNode* nn = n->FindObjectNode("vars");
   if (!nn)
      return 0;
   nn = nn->FindObjectNode(name);
   if (!nn)
      return 0;
   nn = nn->FindObjectNode("d");
   if (!nn)
      return 0;
   return nn->GetDouble();
}

int Fealpha16::Thread()
{
   while (1) {
      //printf("Thread!\n");
      
      for (unsigned i=0; i<fDevices.size(); i++) {
         const char* name = fDevices[i]->s->GetName().c_str();

         std::string cmd;
         cmd += "curl -s --max-time 10 \"http://";
         cmd += name;
         cmd += "/mscb?node=1&dataOnly=true\"";

         //printf("command: %s\n", cmd.c_str());

         std::string reply;

         FILE *fp = popen(cmd.c_str(), "r");
         while (fp) {
            char buf[1024];
            char* s = fgets(buf, sizeof(buf), fp);
            if (!s) {
               pclose(fp);
               fp = NULL;
               break;
            }

            reply += buf;
         }

         //printf("reply: %s\n", reply.c_str());

         if (1) {
            std::string odb;
            odb += "/Equipment/";
            odb += EQ_NAME;
            odb += "/Readback/";
            odb += name;
            odb += "_json";

            int size = reply.length() + 1;
            int status = db_set_value(hDB, 0, odb.c_str(), reply.c_str(), size, 1, TID_STRING);
         }

         MJsonNode* data = MJsonNode::Parse(reply.c_str());

         if (!data)
            continue;

         //data->Dump();

         if (1) {
            std::string odb;
            odb += "/Equipment/";
            odb += EQ_NAME;
            odb += "/Variables/";
            odb += name;
            odb += "_cpu_temp";

            double fvalue = GetDouble(data, "cpu_temp");
            int size = sizeof(fvalue);
            int status = db_set_value(hDB, 0, odb.c_str(), &fvalue, size, 1, TID_DOUBLE);
         }

         if (1) {
            std::string odb;
            odb += "/Equipment/";
            odb += EQ_NAME;
            odb += "/Variables/";
            odb += name;
            odb += "_up_time";

            double fvalue = GetDouble(data, "up_time");
            int size = sizeof(fvalue);
            int status = db_set_value(hDB, 0, odb.c_str(), &fvalue, size, 1, TID_DOUBLE);
         }

         delete data;

         ss_sleep(1000);
      }

      ss_sleep(10000);
   }

   return SUCCESS;
}

int Fealpha16::Start()
{
   for (unsigned i=0; i<fDevices.size(); i++) {
      //ss_sleep(100);
      fDevices[i]->StartRun();
   }

   return SUCCESS;
}

int Fealpha16::Stop()
{
   for (unsigned i=0; i<fDevices.size(); i++)
      fDevices[i]->StopRun();
   return SUCCESS;
}

int Fealpha16::BeginRun(int run_number)
{
   printf("begin_of_run!\n");

   Stop();

   for (unsigned i=0; i<fDevices.size(); i++)
      fDevices[i]->Init();
   
   for (unsigned i=0; i<fDevices.size(); i++)
      alpha16_info(fDevices[i]->s);

   Start();

   printf("begin_of_run done!\n");
   return SUCCESS;
}

int Fealpha16::EndRun()
{
   printf("end_of_run!\n");
   Stop();
   return SUCCESS;
}

class Fealpha16* fe = new Fealpha16;

int xthread(void* param)
{
   TMFeInterface* mfe = (TMFeInterface*) param;
   return mfe->Thread();
}

int frontend_init()
{
   ss_thread_create(xthread, fe);
   return fe->Init();
}

int frontend_loop()
{
   ss_sleep(100);
   return SUCCESS;
}

int begin_of_run(int run_number, char *error)
{
   return fe->BeginRun(run_number);
}

int end_of_run(int run_number, char *error)
{
   return fe->EndRun();
}

int pause_run(INT run_number, char *error)
{
   return fe->PauseRun();
}

int resume_run(INT run_number, char *error)
{
   return fe->ResumeRun();
}

int frontend_exit()
{
   return fe->Exit();
}

INT poll_event(INT source, INT count, BOOL test)
{
   //printf("poll_event: source %d, count %d, test %d\n", source, count, test);

   if (test) {
      for (int i=0; i<count; i++)
         ss_sleep(10);
      return 1;
   }

   return 1;
}

int read_event(char *pevent, int off)
{
   ss_sleep(100);
   return 0;
#if 0
   bk_init32(pevent);
   char* pdata;
   bk_create(pevent, bankname, TID_BYTE, (void**)&pdata);
   memcpy(pdata, buf, length);
   bk_close(pevent, pdata + length);
   return bk_size(pevent); 
#endif
}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
