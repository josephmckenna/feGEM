// fecaenet14xxet.cxx
//
// MIDAS frontend for CAEN HV PS R1419ET/R1470ET

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include <vector>

#include "tmfe.h"

#include "KOtcp.h"

#include "midas.h"

#define C(x) ((x).c_str())

class R14xxet: public TMFeRpcHandlerInterface
{
public:
   TMFE* mfe = NULL;
   TMFeEquipment* eq = NULL;
   KOtcpConnection* s = NULL;

   std::string fBdnch;
   int fNumChan = 0;

   time_t fFastUpdate = 0;

#if 0
   bool Wait(int wait_sec, const char* explain)
   {
      time_t to = time(NULL) + wait_sec;
      while (1) {
         int a = 0;

         KOtcpError e = s->BytesAvailable(&a);
         if (e.error) {
            mfe->Msg(MERROR, "Wait", "Socket error %s", e.message.c_str());
            s->Close();
            eq->SetStatus("Lost connection", "red");
            return false;
         }
         
         //printf("Wait %d sec, available %d\n", wait_sec, a);
         if (a > 0)
            return true;
         
         if (time(NULL) > to) {
            mfe->Msg(MERROR, "Wait", "Timeout waiting for repy to command: %s", explain);
            s->Close();
            eq->SetStatus("Lost connection", "red");
            return false;
         }
         
         mfe->SleepMSec(1);
         
         if (mfe->fShutdown) {
            mfe->Msg(MERROR, "Wait", "Shutdown command while waiting for reply to command: %s", explain);
            return false;
         }
      }
      // not reached
   }
#endif
         
   bool Wait(int wait_sec, const char* explain)
   {
      int a = 0;

      KOtcpError e = s->WaitBytesAvailable(wait_sec*1000, &a);
      if (e.error) {
         mfe->Msg(MERROR, "Wait", "Error waiting for reply to command \"%s\": %s", explain, e.message.c_str());
         s->Close();
         eq->SetStatus("Lost connection", "red");
         return false;
      }
         
      //printf("Wait %d sec, available %d\n", wait_sec, a);
      if (a > 0)
         return true;
         
      mfe->Msg(MERROR, "Wait", "Timeout waiting for reply to command \"%s\"", explain);
      s->Close();
      eq->SetStatus("Lost connection", "red");
      return false;
   }
         
   std::string Exch(const char* cmd)
   {
      if (mfe->fShutdown)
         return "";
      
      std::string ss = cmd;
      ss += '\r';
      ss += '\n';
      
      KOtcpError err = s->WriteString(ss);
      
      if (err.error) {
         mfe->Msg(MERROR, "Exch", "Communication error: Command [%s], WriteString error [%s]", cmd, err.message.c_str());
         s->Close();
         eq->SetStatus("Lost connection", "red");
         return "";
      }
      
      if (!Wait(5, cmd))
         return "";
      
      std::string reply;
      
      err = s->ReadString(&reply, 64*1024);
      
      if (err.error) {
         mfe->Msg(MERROR, "Exch", "Communication error: Command [%s], ReadString error [%s]", cmd, err.message.c_str());
         s->Close();
         eq->SetStatus("Lost connection", "red");
         return "";
      }
      
      printf("command %s, reply [%s]\n", cmd, reply.c_str());
      
      return reply;
   }

   static std::string V(const std::string& s)
   {
      std::string::size_type p = s.find("VAL:");
      if (p == std::string::npos)
         return "";
      return s.substr(p+4);
   }

   static std::vector<std::string> split(const std::string& s)
   {
      std::vector<std::string> v;
      
      std::string::size_type p = 0;
      while (1) {
         std::string::size_type pp = s.find(";", p);
         //printf("p %d, pp %d\n", p, pp);
         if (pp == std::string::npos) {
            v.push_back(s.substr(p));
            return v;
         }
         v.push_back(s.substr(p, pp-p));
         p = pp + 1;
      }
      // not reached
   }

   static std::vector<double> D(std::vector<std::string>& v)
   {
      std::vector<double> vv;
      for (unsigned i=0; i<v.size(); i++) {
         //printf("v[%d] is [%s]\n", i, C(v[i]));
         vv.push_back(atof(C(v[i])));
      }
      return vv;
   }

   void WR(const char* name, const char* v)
   {
      if (mfe->fShutdown)
         return;
      
      std::string path;
      path += "/Equipment/";
      path += eq->fName;
      path += "/Readback/";
      path += name;
      //printf("Write ODB %s Readback %s: %s\n", C(path), name, v);
      int status = db_set_value(mfe->fDB, 0, C(path), v, strlen(v)+1, 1, TID_STRING);
      if (status != DB_SUCCESS) {
         printf("WR: db_set_value status %d\n", status);
      }
   }
         
   void WVD(const char* name, const std::vector<double> &v)
   {
      if (mfe->fShutdown)
         return;
      
      std::string path;
      path += "/Equipment/";
      path += eq->fName;
      path += "/Variables/";
      path += name;
      //printf("Write ODB %s Readback %s: %s\n", C(path), name, v);
      int status = db_set_value(mfe->fDB, 0, C(path), &v[0], sizeof(double)*v.size(), v.size(), TID_DOUBLE);
      if (status != DB_SUCCESS) {
         printf("WVD: db_set_value status %d\n", status);
      }
   }
         
   void WRStat(const std::vector<double> &stat)
   {
      if (mfe->fShutdown)
         return;
      
      std::string path;
      path += "/Equipment/";
      path += eq->fName;
      path += "/Readback/";
      path += "STAT_BITS";
      
      std::string v;
      
      for (unsigned i=0; i<stat.size(); i++) {
         if (i>0)
            v += ";";
         
         int b = stat[i];
         char buf[256];
         sprintf(buf, "0x%04x", b);
         v += buf;
         
         if (b & (1<<0)) v += " ON";
         if (b & (1<<1)) { v += " RUP"; fFastUpdate = time(NULL) + 10; }
         if (b & (1<<2)) { v += " RDW"; fFastUpdate = time(NULL) + 10; }
         if (b & (1<<3)) v += " OVC";
         if (b & (1<<4)) v += " OVV";
         if (b & (1<<5)) v += " UNV";
         if (b & (1<<6)) v += " MAXV";
         if (b & (1<<7)) v += " TRIP";
         if (b & (1<<8)) v += " OVP";
         if (b & (1<<9)) v += " OVT";
         if (b & (1<<10)) v += " DIS";
         if (b & (1<<11)) v += " KILL";
         if (b & (1<<12)) v += " ILK";
         if (b & (1<<13)) v += " NOCAL";
         if (b & (1<<14)) v += " bit14";
         if (b & (1<<15)) v += " bit15";
      }
      
      //printf("Write ODB %s value %s\n", C(path), C(v));

      int status = db_set_value(mfe->fDB, 0, C(path), C(v), v.length()+1, 1, TID_STRING);
      if (status != DB_SUCCESS) {
         printf("WR: db_set_value status %d\n", status);
      }
   }
         
   void WRAlarm(const std::string &alarm)
   {
      if (mfe->fShutdown)
         return;
      
      std::string path;
      path += "/Equipment/";
      path += eq->fName;
      path += "/Readback/";
      path += "BDALARM_BITS";
      
      std::string v;
      
      int b = atoi(C(alarm));
      
      char buf[256];
      sprintf(buf, "0x%04x", b);
      v += buf;
      
      if (b & (1<<0)) v += " CH0";
      if (b & (1<<1)) v += " CH1";
      if (b & (1<<2)) v += " CH2";
      if (b & (1<<3)) v += " CH3";
      if (b & (1<<4)) v += " PWFAIL";
      if (b & (1<<5)) v += " OVP";
      if (b & (1<<6)) v += " HVCKFAIL";
      
      //printf("Write ODB %s value %s\n", C(path), C(v));
      int status = db_set_value(mfe->fDB, 0, C(path), C(v), v.length()+1, 1, TID_STRING);
      if (status != DB_SUCCESS) {
         printf("WR: db_set_value status %d\n", status);
      }
      
      if (b) {
         std::string vv = "Alarm: " + v;
         eq->SetStatus(C(vv), "#FF0000");
         
         std::string aa = eq->fName + " alarm " + v;
         mfe->TriggerAlarm(C(eq->fName), C(aa), "Alarm");
      } else {
         path.clear();
         path += "/Equipment/";
         path += eq->fName;
         path += "/Variables/";
         path += "VMON[2]";
         double awv;
         int size = sizeof(awv);
         db_get_value(mfe->fDB, 0, C(path), &awv, &size, TID_DOUBLE, FALSE);
         path.clear();
         path += "/Equipment/";
         path += eq->fName;
         path += "/Variables/";
         path += "IMON[2]";
         double awi;
         size = sizeof(awi);
         db_get_value(mfe->fDB, 0, C(path), &awi, &size, TID_DOUBLE, FALSE);
         char str[64];
         sprintf(str, "Anode Wires %4.0lf[V]@%3.0lf[nA]", awv, 1000.*awi);
         if (awi*1000. > 100.0) {
            eq->SetStatus(str, "#F1C40F");
         } else {
            eq->SetStatus(str, "#00FF00");
         }
         mfe->ResetAlarm(C(eq->fName));
      }
   }

   std::string RE1(const char* name)
   {
      if (mfe->fShutdown)
         return "";
      std::string cmd;
      cmd += "$BD:00:CMD:MON,PAR:";
      cmd += name;
      std::string r = Exch(C(cmd));
      if (r.length() < 1)
         return "";
      std::string v = V(r);
      WR(name, C(v));
      return v;
   }

   std::string RE(const char* name)
   {
      if (mfe->fShutdown)
         return "";
      std::string cmd;
      //Exch(s, "$BD:00:CMD:MON,CH:4,PAR:VSET");
      cmd += "$BD:00:CMD:MON,CH:";
      cmd += fBdnch;
      cmd += ",PAR:";
      cmd += name;
      std::string r = Exch(C(cmd));
      if (r.length() < 1)
         return "";
      std::string v = V(r);
      WR(name, C(v));
      return v;
   }

   std::vector<double> VE(const char* name)
   {
      std::vector<double> vd;
      if (mfe->fShutdown)
         return vd;
      std::string cmd;
      //Exch(s, "$BD:00:CMD:MON,CH:4,PAR:VSET");
      cmd += "$BD:00:CMD:MON,CH:";
      cmd += fBdnch;
      cmd += ",PAR:";
      cmd += name;
      std::string r = Exch(C(cmd));
      if (r.length() < 1)
         return vd;
      std::string v = V(r);
      std::vector<std::string> vs = split(v);
      vd = D(vs);
      //WVD(name, vd);
      eq->fOdbEqVariables->WDA(name, vd);
      return vd;
   }

   // write parameter, no value

   void WE(const char* name, int ch)
   {
      char cmd[256];
      sprintf(cmd, "$BD:00:CMD:SET,CH:%d,PAR:%s", ch, name);
      Exch(cmd);
   }

   // write parameter, floating point value

   void WED(const char* name, int ch, double v)
   {
      char cmd[256];
      sprintf(cmd, "$BD:00:CMD:SET,CH:%d,PAR:%s,VAL:%f", ch, name, v);
      Exch(cmd);
   }

   // write parameter, string value

   void WES(const char* name, int ch, const char* v)
   {
      char cmd[256];
      sprintf(cmd, "$BD:00:CMD:SET,CH:%d,PAR:%s,VAL:%s", ch, name, v);
      Exch(cmd);
   }

   void WES(const char* name, const char* v)
   {
      char cmd[256];
      sprintf(cmd, "$BD:00:CMD:SET,PAR:%s,VAL:%s", name, v);
      Exch(cmd);
   }

   // read important parameters

   void ReadImportant()
   {
      RE1("BDILK"); // interlock status

      std::string bdalarm = RE1("BDALARM"); // alarm status
      if (bdalarm.length() > 0) {
         WRAlarm(bdalarm);
      }

      VE("VSET"); // voltage setpoint VSET
      VE("VMON"); // voltage actual value VMON

      VE("ISET"); // current setpoint ISET, uA
      VE("IMON"); // current actual value, uA

      std::vector<double> stat = VE("STAT"); // channel status
      WRStat(stat);
   }

   void ReadSettings()
   {
      RE1("BDILKM"); // interlock mode
      RE1("BDCTR"); // control mode
      RE1("BDTERM"); // local bus termination
      
      mfe->PollMidas(1);

      VE("VMIN"); // VSET minimum value
      VE("VMAX"); // VSET maximum value
      VE("VDEC"); // VSET number of decimal digits
         
      mfe->PollMidas(1);

      VE("IMIN");    // ISET minimum value
      VE("IMAX");    // ISET maximum value
      VE("ISDEC");   // ISET number of decimal digits
      RE("IMRANGE"); // current monitor range HIGH/LOW
      VE("IMDEC");   // IMON number of decimal digits
         
      mfe->PollMidas(1);

      VE("MAXV");  // MAXVSET max VSET value
      VE("MVMIN"); // MAXVSET minimum value
      VE("MVMAX"); // MAXVSET maximum value
      VE("MVDEC"); // MAXVSET number of decimal digits

      mfe->PollMidas(1);

      VE("RUP"); // ramp up V/s
      VE("RUPMIN");
      VE("RUPMAX");
      VE("RUPDEC");
         
      mfe->PollMidas(1);

      VE("RDW"); // ramp down V/s
      VE("RDWMIN");
      VE("RDWMAX");
      VE("RDWDEC");
         
      mfe->PollMidas(1);

      VE("TRIP"); // trip time, sec
      VE("TRIPMIN");
      VE("TRIPMAX");
      VE("TRIPDEC");

      mfe->PollMidas(1);

      RE("PDWN"); // power down RAMP/KILL
      RE("POL"); // polarity
   }

   void TurnOn(int chan)
   {
      mfe->Msg(MINFO, "TurnOn", "Turning on channel %d", chan);
      WE("ON", chan);
   }
         
   void TurnOff(int chan)
   {
      mfe->Msg(MINFO, "TurnOff", "Turning off channel %d", chan);
      WE("OFF", chan);
   }

   std::vector<double> vset;
   std::vector<double> iset;
   std::vector<double> maxv;
   std::vector<double> rup;
   std::vector<double> rdw;
   std::vector<double> trip;
   std::vector<double> pdwn;
   std::vector<double> imrange;

   void UpdateSettings()
   {
      mfe->Msg(MINFO, "UpdateSettings", "Writing settings from ODB to hardware");

      //Exch(mfe, s, "$BD:00:CMD:SET,PAR:BDILKM,VAL:OPEN"); // set interlock mode
      //Exch(mfe, s, "$BD:00:CMD:SET,PAR:BDILKM,VAL:CLOSED");
      WES("BDILKM", "CLOSED");
      
      //Exch(mfe, s, "$BD:00:CMD:SET,PAR:BDCLR"); // clear alarm signal
      
      //Exch(mfe, s, "$BD:00:CMD:SET,CH:4,PAR:VSET,VAL:1;2;3;4");
     
      int nch = fNumChan;

      eq->fOdbEqSettings->RDA("VSET", &vset, true, nch);
      eq->fOdbEqSettings->RDA("ISET", &iset, true, nch);
      eq->fOdbEqSettings->RDA("MAXV", &maxv, true, nch);
      eq->fOdbEqSettings->RDA("RUP",  &rup,  true, nch);
      eq->fOdbEqSettings->RDA("RWD",  &rdw,  true, nch);
      eq->fOdbEqSettings->RDA("TRIP", &trip, true, nch);
      eq->fOdbEqSettings->RDA("PDWN", &pdwn, true, nch);
      eq->fOdbEqSettings->RDA("IMRANGE", &imrange, true, nch);
 
      for (int i=0; i<nch; i++) {
         WED("VSET", i, vset[i]);
         WED("ISET", i, iset[i]);
         WED("MAXV", i, maxv[i]);
         WED("RUP",  i, rup[i]);
         WED("RDW",  i, rdw[i]);
         WED("TRIP", i, trip[i]);
         
         if (pdwn[i] == 1) {
            WES("PDWN", i, "RAMP");
         } else if (pdwn[i] == 2) {
            WES("PDWN", i, "KILL");
         }
         
         if (imrange[i] == 1) {
            WES("IMRANGE", i, "HIGH");
         } else if (imrange[i] == 2) {
            WES("IMRANGE", i, "LOW");
         }
         
#if 0
         double onoff = OdbGetValue(mfe, eq->fName, "ONOFF", i, nch);
         if (onoff == 1) {
            WE(mfe, eq, s, "ON", i);
         } else if (onoff == 2) {
            WE(mfe, eq, s, "OFF", i);
         }
#endif
      }
      
      fFastUpdate = time(NULL) + 30;
   }

   std::string HandleRpc(const char* cmd, const char* args)
   {
      mfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);

      int mask = 0;
      int all = 0;
      int chan = 0;
      if (strcmp(args, "all") == 0) {
         all = 1;
         mask = 0xF;
      } else {
         chan = atoi(args);
         mask |= (1<<chan);
      }

      //printf("mask 0x%x\n", mask);

      if (strcmp(cmd, "update_settings")==0) {
         UpdateSettings();

         sleep(1);
         sleep(1);

         ReadImportant();

         fFastUpdate = time(NULL) + 30;

         return "OK";
      } else if (strcmp(cmd, "turn_on")==0) {

         UpdateSettings();

         if (all) {
            for (int i=0; i<fNumChan; i++) {
               TurnOn(i);
            }
         } else {
            TurnOn(chan);
         }

         sleep(1);
         sleep(1);

         ReadImportant();

         fFastUpdate = time(NULL) + 30;

         return "OK";
      } else if (strcmp(cmd, "turn_off")==0) {

         //UpdateSettings();

         if (all) {
            for (int i=0; i<fNumChan; i++) {
               TurnOff(i);
            }
         } else {
            TurnOff(chan);
         }

         sleep(1);
         sleep(1);

         ReadImportant();

         fFastUpdate = time(NULL) + 30;

         return "OK";
      } else {
         return "";
      }
   }
};

#define CHECK(delay) { if (!s->fConnected) break; mfe->PollMidas(delay); if (mfe->fShutdown) break; }
#define CHECK1(delay) { if (!s->fConnected) break; mfe->PollMidas(delay); if (mfe->fShutdown) break; }

int main(int argc, char* argv[])
{
   setbuf(stdout, NULL);
   setbuf(stderr, NULL);

   signal(SIGPIPE, SIG_IGN);

   const char* name = argv[1];

   if (strcmp(name, "hvps01")==0) {
      // good
   } else if (strcmp(name, "hvps02")==0) {
      // good
   } else {
      printf("Only hvps01 or hvps02 permitted. Bye.\n");
      return 1;
   }

   TMFE* mfe = TMFE::Instance();

   TMFeError err = mfe->Connect(C(std::string("fecaen_") + name));
   if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
   }

   //mfe->SetWatchdogSec(0);

   TMFeCommon *eqc = new TMFeCommon();
   eqc->EventID = 3;
   eqc->FrontendName = std::string("fecaen_") + name;
   eqc->LogHistory = 1;
   
   TMFeEquipment* eq = new TMFeEquipment(C(std::string("CAEN_") + name));
   eq->Init(mfe->fOdbRoot, eqc);
   eq->SetStatus("Starting...", "white");

   mfe->RegisterEquipment(eq);

   const char* port = "1470";
   KOtcpConnection* s = new KOtcpConnection(name, port);

   class R14xxet* hv = new R14xxet;

   hv->mfe = mfe;
   hv->eq = eq;
   hv->s = s;

   mfe->RegisterRpcHandler(hv);
   mfe->SetTransitionSequence(-1, -1, -1, -1);

   while (!mfe->fShutdown) {
      bool first_time = true;

      if (!s->fConnected) {
         eq->SetStatus("Connecting...", "white");

         int delay = 100;
         while (!mfe->fShutdown) {
            KOtcpError e = s->Connect();
            if (!e.error) {
               mfe->Msg(MINFO, "main", "Connected to %s:%s", name, port);
               eq->SetStatus("Connected...", "white");
               break;
            }
            eq->SetStatus("Cannot connect, trying again", "red");
            mfe->Msg(MINFO, "main", "Cannot connect to %s:%s, Connect() error %s", name, port, e.message.c_str());
            mfe->PollMidas(delay);
            if (delay < 5*60*1000) {
               delay *= 2;
            }
         }
      }

      while (!mfe->fShutdown) {

         //time_t start_time = time(NULL);

         //Exch(mfe, s, "$BD:00:CMD:MON,PAR:BDNAME");
         std::string bdname = hv->RE1("BDNAME"); // mainframe name and type
         std::string bdnch  = hv->RE1("BDNCH"); // channels number

         if (mfe->fShutdown) {
            break;
         }

         if (bdname.length() < 1 || bdnch.length() < 1) {
            mfe->Msg(MERROR, "main", "Cannot read BDNAME or BDNCH, will try to reconnect after 10 sec...");
            mfe->PollMidas(10000);
            break;
         }

         hv->fBdnch = bdnch;
         hv->fNumChan = atoi(bdnch.c_str());

         std::string bdfrel = hv->RE1("BDFREL"); // firmware release
         std::string bdsnum = hv->RE1("BDSNUM"); // serial number

         if (first_time) {
            mfe->Msg(MINFO, "main", "Device %s is model %s with %s channels, firmware %s, serial %s", name, C(bdname), C(bdnch), C(bdfrel), C(bdsnum));
         }

         hv->ReadImportant();
         
         hv->ReadSettings();

         //time_t end_time = time(NULL);

         //printf("readout time: %d sec\n", (int)(end_time - start_time));

         if (first_time) {
            hv->UpdateSettings();
         }

         first_time = false;
         
         if (0) {
            //mfe->SleepMSec(1000);

            //Exch(mfe, s, "$BD:00:CMD:SET,PAR:BDILKM,VAL:OPEN");
            hv->Exch("$BD:00:CMD:SET,PAR:BDILKM,VAL:CLOSED");

            hv->Exch("$BD:00:CMD:SET,PAR:BDCLR");
            
            //Exch(mfe, s, "$BD:00:CMD:SET,CH:4,PAR:VSET,VAL:1;2;3;4");
            hv->Exch("$BD:00:CMD:SET,CH:0,PAR:VSET,VAL:10");
            hv->Exch("$BD:00:CMD:SET,CH:1,PAR:VSET,VAL:11");
            hv->Exch("$BD:00:CMD:SET,CH:2,PAR:VSET,VAL:12");
            hv->Exch("$BD:00:CMD:SET,CH:3,PAR:VSET,VAL:13");
         }

         if (hv->fFastUpdate != 0) {
            if (time(NULL) > hv->fFastUpdate)
               hv->fFastUpdate = 0;
         }

         if (hv->fFastUpdate) {
            //mfe->Msg(MINFO, "main", "fast update!");
            mfe->PollMidas(1000);
            if (mfe->fShutdown)
               break;
         } else {
            for (int i=0; i<3; i++) {
               mfe->PollMidas(1000);
               if (mfe->fShutdown)
                  break;
            }
            if (mfe->fShutdown)
               break;
         }
      }
   }

   if (s->fConnected)
      s->Close();
   delete s;
   s = NULL;

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
