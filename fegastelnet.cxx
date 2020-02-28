// fegastelnet.cxx
//
// MIDAS frontend for MKS gas system controller via telnet

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include <vector>
#include <sstream>

#include "tmfe.h"
#include "tmvodb.h"
#include "KOtcp.h"
#include "midas.h"

#define C(x) ((x).c_str())

static std::vector<std::string> split(const char* sep, const std::string& s)
{
   std::vector<std::string> v;

   std::string::size_type p = 0;
   while (1) {
      std::string::size_type pp = s.find(sep, p);
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

std::string join(const char* sep, const std::vector<std::string>& s)
{
   std::string r;
   for (unsigned i=0; i<s.size(); i++) {
      if (i>0)
         r += sep;
      r += s[i];
   }
   return r;
}

static bool gUpdate = false;

class GasTelnet: public TMFeRpcHandlerInterface
{
public:
   TMFE* mfe = NULL;
   TMFeEquipment* eq = NULL;
   KOtcpConnection* s = NULL;
   TMVOdb *fV = NULL;
   TMVOdb *fS = NULL;
   //TMVOdb *fHS = NULL;          // History display settings

   time_t fFastUpdate = 0;

   void WB(const char* varname, int i, BOOL v)
   {
      std::string path;
      path += "/Equipment/";
      path += eq->fName;
      path += "/Settings";
      path += "/";

      printf(">>>>>>>>>>>>>>> %s -> %s\n", path.c_str(), varname);

      // db_set_value_index(mfe->fDB, 0, varname, &v, sizeof(BOOL), i, TID_BOOL, false);
      db_set_value_index(mfe->fDB, 0, "test", &v, sizeof(BOOL), i, TID_BOOL, false);
      path += "test";
      bool test;
      fS->RB(path.c_str(), i, &test, true);
      if(test)
         mfe->Msg(MERROR, "WB", "%d true", i);
      else
         mfe->Msg(MERROR, "WB", "%d false", i);
   };

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

   KOtcpError Exch(const char* cmd, std::vector<std::string>* reply)
   {
      KOtcpError err;

      reply->clear();

      if (mfe->fShutdown)
         return err;

      if (cmd) {
         std::string ss = cmd;
         ss += '\r';
         ss += '\n';

         err = s->WriteString(ss);

         if (err.error) {
            mfe->Msg(MERROR, "Exch", "Communication error: Command [%s], WriteString error [%s]", cmd, err.message.c_str());
            s->Close();
            eq->SetStatus("Lost connection", "red");
            return err;
         }

         if (!Wait(5, cmd))
            return err;

      } else {
         cmd = "(null)";
      }

      std::string sss;

      while (1) {
         int nbytes = 0;
         err = s->WaitBytesAvailable(100, &nbytes);
         //printf("available %d\n", nbytes);

         if (nbytes < 1) {
            break;
         }

         char buf[nbytes+1];

         err = s->ReadBytes(buf, nbytes);

         for (int i=0; i<nbytes; i++) {
            if (buf[i] == '\r' && buf[i+1] == 0) { buf[i] = ' '; buf[i+1] = ' '; };
            if (buf[i] == '\r') buf[i] = ' ';
            //if (buf[i] == '\n') buf[i] = 'N';
            if (buf[i] == 0) buf[i] = 'X';
         }

         buf[nbytes] = 0; // make sure string is NUL terminated

         //printf("read %d, err %d, errno %d, string [%s]\n", nbytes, err.error, err.xerrno, buf);

         //if ((sss.length() >= 1) && (sss[0] != 0)) {
         //   reply->push_back(sss);
         //}

         if (err.error) {
            mfe->Msg(MERROR, "Exch", "Communication error: Command [%s], ReadString error [%s]", cmd, err.message.c_str());
            s->Close();
            eq->SetStatus("Lost connection", "red");
            return err;
         }

         sss += buf;
      }

      //printf("total: %d bytes\n", sss.length());

      *reply = split("\n", sss);

      // printf("command %s, reply %d [%s]\n", cmd, (int)reply->size(), join("|", *reply).c_str());

      //for (unsigned i=0; i<reply->size(); i++) {
      //   printf("reply[%d] is [%s]\n", i, reply->at(i).c_str());
      //}

      return KOtcpError();
   }

   std::vector<int> parse(const std::vector<std::string>& r)
   {
      std::vector<int> v;
      for (unsigned i=0; i<r.size(); i++) {
         const char* rrr = r[i].c_str();
         const char* p = strchr(rrr, ':');
         if (!p)
            continue;
         p = strstr(p, "0x");
         if (!p)
            continue;
         unsigned long int value = strtoul(p, NULL, 0);
         value &= 0xFFFF; // only 16 bits come from the device
         if (value & 0x8000) // sign-extend from 16-bit to 32-bit signed integer
            value |= 0xFFFF0000;
         v.push_back(value);
         //printf("parse %d: [%s], value %d 0x%x, have %d, at [%s]\n", i, rrr, value, value, v.size(), p);
      }
      return v;
   }

   std::vector<int> parse2(const std::vector<std::string>& r)
   {
      std::vector<int> v;
      for (unsigned i=0; i<r.size(); i++) {
         const char* rrr = r[i].c_str();
         const char* p = strstr(rrr, ": ");
         if (!p)
            continue;
         p += 2; // skip ':'
         int value = strtol(p, NULL, 0);
         v.push_back(value);
         //printf("parse %d: [%s], value %d 0x%x, have %d, at [%s]\n", i, rrr, value, value, v.size(), p);
      }
      return v;
   }

   static std::string V(const std::string& s)
   {
      std::string::size_type p = s.find("VAL:");
      if (p == std::string::npos)
         return "";
      return s.substr(p+4);
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

   void ReadImportant()
   {
   }

   void ReadSettings()
   {
   }

   void SetValve(int chan, int state)
   {
      mfe->Msg(MINFO, "SetValve", "SetValve(%d, %d)", chan, state);
      std::vector<int> states;
      fS->RIA("do",&states,true,5);
      states[chan] = state;
      fS->WIA("do", states);
      // WB("do", chan, bool(state));
   }

   void UpdateSettings()
   {
      mfe->Msg(MINFO, "UpdateSettings", "Writing settings from ODB to hardware");

      fFastUpdate = time(NULL) + 30;
   }

   std::string HandleRpc(const char* cmd, const char* args)
   {
      mfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);

      if (strcmp(cmd, "set_valve")==0) {

         if (gUpdate) {
            UpdateSettings();
         }

         std::istringstream iss(args);
         int chan, state;
         iss >> chan >> state;
         SetValve(chan,state);

         sleep(1);
         sleep(1);

         ReadImportant();

         //gUpdate = true;
         fFastUpdate = time(NULL) + 30;

         return "OK";
      } else {
         return "";
      }
   }
};

#define CHECK(delay) { if (!s->fConnected) break; mfe->PollMidas(delay); if (mfe->fShutdown) break; if (gUpdate) continue; }
#define CHECK1(delay) { if (!s->fConnected) break; mfe->PollMidas(delay); if (mfe->fShutdown) break; }

static void handler(int a, int b, int c, void* d)
{
   //printf("db_watch handler %d %d %d\n", a, b, c);
   // cm_msg(MINFO, "handler", "db_watch requested update settings!");
   gUpdate = true;
}

void setup_watch(TMFE* mfe, TMFeEquipment* eq)
{
   std::string path;
   path += "/Equipment/";
   path += eq->fName;
   path += "/Settings";

   HNDLE hkey;
   int status = db_find_key(mfe->fDB, 0, C(path), &hkey);

   //printf("db_find_key status %d\n", status);
   if (status != DB_SUCCESS)
      return;

   status = db_watch(mfe->fDB, hkey, handler, NULL);

   //printf("db_watch status %d\n", status);
}

int main(int argc, char* argv[])
{
   setbuf(stdout, NULL);
   setbuf(stderr, NULL);

   signal(SIGPIPE, SIG_IGN);

   TMFE* mfe = TMFE::Instance();

   TMFeError err = mfe->Connect("fegastelnet");
   if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
   }

   //mfe->SetWatchdogSec(0);

   TMFeCommon *eqc = new TMFeCommon();
   eqc->EventID = 3;
   eqc->FrontendName = "fegastelnet";
   eqc->LogHistory = 1;

   TMFeEquipment* eq = new TMFeEquipment("TpcGas");
   eq->Init(mfe->fOdbRoot, eqc);
   eq->SetStatus("Starting...", "white");

   mfe->RegisterEquipment(eq);

   setup_watch(mfe, eq);

   gUpdate = true;

   const char* name = "algas";
   const char* port = "23";

   KOtcpConnection* s = new KOtcpConnection(name, port);

   GasTelnet* gas = new GasTelnet;

   gas->mfe = mfe;
   gas->eq = eq;
   gas->s = s;

   //TMVOdb* odb = MakeOdb(mfe->fDB);
   gas->fV = eq->fOdbEqVariables; // odb->Chdir("Equipment/TpcGas/Variables", false);
   gas->fS = eq->fOdbEqSettings; // odb->Chdir("Equipment/TpcGas/Settings", false);
   //gas->fHS = odb->Chdir("History/Display/TPC/Flow", false);

   // Factors to translate MFC/MFM readings into sccm flows, constant for MFC 1 and 2 that handle pure Ar and CO2, variable for MFM, which handles mixture
   const double fArFact = 0.17334;
   const double fCO2Fact = 0.08667;
   double fOutFact = 0.5; // dummy

   mfe->RegisterRpcHandler(gas);
   mfe->SetTransitionSequence(-1, -1, -1, -1);

   while (!mfe->fShutdown) {
      std::vector<std::string> r;

      if (!s->fConnected) {
         eq->SetStatus("Connecting...", "white");

         int delay = 100;
         while (!mfe->fShutdown) {
            KOtcpError e = s->Connect();
            if (!e.error) {
               mfe->Msg(MINFO, "main", "Connected to %s:%s", name, port);
               bool have_shell = false;
               for (int i=0; i<10; i++) {
                  gas->Exch(" ", &r);
                  for (unsigned j=0; j<r.size(); j++) {
                     std::string::size_type pos = r[j].find("shell");
                     //printf("%d: [%s] pos %d %d %d\n", j, r[j].c_str(), (int)pos, std::string::npos, pos != std::string::npos);
                     if (pos != std::string::npos) {
                        have_shell = true;
                        break;
                     }
                  }
                  if (have_shell)
                     break;
                  mfe->PollMidas(1);
                  if (mfe->fShutdown)
                     break;
               }
               if (have_shell) {
                  mfe->Msg(MINFO, "main", "Have shell prompt");
                  eq->SetStatus("Connected...", "white");
               } else {
                  eq->SetStatus("Connected but no shell prompt", "red");
               }
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

         double start_time = mfe->GetTime();

         std::vector<int> digOut;
         double totflow = 0.;
         gas->Exch("cord do", &r);
         std::vector<int> cord_do = gas->parse(r);
         std::vector<int> readVals(5), sv_open;
         gas->fV->RIA("SV_open", &sv_open, true, 4);
         for(unsigned int i = 0; i <readVals.size(); i++){
            readVals[i] = bool(cord_do[0] & (0x1 << i));
         }
         sv_open = readVals;
         sv_open.resize(4);
         sv_open[2] = !sv_open[2];
         sv_open[3] = !sv_open[3];
         gas->fV->WIA("SV_open", sv_open);

         if (1) {
            gas->fS->RIA("do", &digOut,true,5); // Read ODB values for solenoid valves

            int doOdb = 0;
            for(unsigned int i = 0; i < digOut.size(); i++){
               int bit = 0x1 << i;
               doOdb |= int(digOut[i])*bit;
            }
            if(cord_do.size()){
               if(cord_do[0] != doOdb){
                  int bitdiff = cord_do[0] ^ doOdb;
                  std::vector<double> hv;
                  gas->fV->RDA("HV", &hv, false, 4);
                  if(hv.size() != 4){
                     mfe->Msg(MERROR, "main", "Missing HV values.");
                  } else {
                     if(hv[2] > 100 && (bitdiff & 0b111)){ // TPC related valves should not be switched under HV
                        mfe->Msg(MERROR, "main", "Cannot switch solenoid valves when TPC under HV: %.1f, setting ODB to current state", hv[2]);
                        gas->fS->WIA("do", readVals);
                     } else {
                        for(unsigned int i = 0; i < digOut.size(); i++){
                           
                           char cmd[64];
                           sprintf(cmd, "cowr do %d %d", i+1, int(digOut[i]));
                           std::vector<std::string> r;
                           mfe->Msg(MINFO, "main", cmd);
                           gas->Exch(cmd, &r);
                        }
                     }
                  }
               }
            } else {
               mfe->Msg(MERROR, "main", "Solenoid valve readback doesn't work.");
            }

            gas->Exch("cord do", &r);
            cord_do = gas->parse(r);
            for(unsigned int i = 0; i <readVals.size(); i++){
               readVals[i] = bool(cord_do[0] & (0x1 << i));
            }
            sv_open = readVals;
            sv_open.resize(4);
            sv_open[2] = !sv_open[2];
            sv_open[3] = !sv_open[3];
            gas->fV->WIA("SV_open", sv_open);

            doOdb = 0;
            for(unsigned int i = 0; i < digOut.size(); i++){
               int bit = 0x1 << i;
               doOdb |= int(digOut[i])*bit;
            }
            if(cord_do.size()){
               if(cord_do[0] != doOdb){
                  mfe->Msg(MERROR, "main", "Solenoid valve readback doesn't match ODB value: %x != %x", cord_do[0], doOdb);
               }
            } else {
               mfe->Msg(MERROR, "main", "Solenoid valve readback doesn't work.");
            }

            gas->fS->RD("flow", 0, &totflow, true);
            printf("flow %f, gUpdate %d\n", totflow, gUpdate);
            double co2perc = 1.23;
            gas->fS->RD("co2perc", 0, &co2perc, true);
            printf("co2perc %f, gUpdate %d\n", co2perc, gUpdate);
            if(co2perc > 100.){
               mfe->Msg(MERROR, "main", "ODB value for CO2 percentage larger than 100%%");
            } else if(gUpdate){
               double co2flow = totflow * co2perc/100.;
               double arflow = totflow - co2flow;

               const double co2max_flow = 1000.;
               const double armax_flow = 2000.;
               int co2flow_int = co2flow/co2max_flow * 0x7FFF;
               int arflow_int = arflow/armax_flow * 0x7FFF;

               const double totFact[] = {3.89031111111111, 4.23786666666667, 4.57469816272966, 4.91008375209369, 5.25735343359752, 5.57199329951513, 7.38488888888889};
               int facIndex = int(co2perc)/10;
               double interm = 0.1*(co2perc-facIndex*10.);
               if(facIndex > 5){
                  facIndex = 5;
                  interm = (co2perc-50.)/50.;
               }
               double factor = totFact[facIndex];
               assert(interm >= 0);
               if(interm > 0){
                  factor += interm*(totFact[facIndex+1]-factor);
               }

               fOutFact = 1./factor;

               // printf("co2flow %f, arflow %f, co2int %d, arint %d\n", co2flow, arflow, co2flow_int, arflow_int);
               char cmd[64];
               sprintf(cmd, "mfcwr ao 1 %d", arflow_int);
               std::vector<std::string> r;
               gas->Exch(cmd, &r);
               sprintf(cmd, "mfcwr ao 2 %d", co2flow_int);
               gas->Exch(cmd, &r);
            }
            gUpdate = 0;
         }

         gas->Exch("tc_rd slice_cfg", &r);
         std::vector<int> slice_cfg = gas->parse(r);

         gas->Exch("mfcrd ai", &r);
         std::vector<int> mfcrd_ai = gas->parse(r);

         gas->fV->WD("p_mbar", mfcrd_ai[7]*0.1139);

         gas->Exch("mfcrd ao", &r);
         std::vector<int> mfcrd_ao = gas->parse(r);

         gas->Exch("mfcrd do", &r);
         std::vector<int> mfcrd_do = gas->parse(r);

         gas->Exch("mfcrd docfg", &r);
         std::vector<int> mfcrd_docfg = gas->parse2(r);


         double end_time = mfe->GetTime();
         double read_time = end_time - start_time;

         if (!gas->s->fConnected) {
            gUpdate = true;        // If connection is lost, re-set all settings on next connect.
            mfe->Msg(MINFO, "main", "Connection problems: will re-set all gas settings on next hardware connection.");
            break;
         }

         std::vector<double> gas_flow;
         gas_flow.push_back(fArFact*double(mfcrd_ai[1]));
         gas_flow.push_back(fCO2Fact*double(mfcrd_ai[3]));
         gas_flow.push_back(fOutFact*double(mfcrd_ai[5]));
         gas_flow.push_back(gas_flow[0]+gas_flow[1]);
         gas_flow.push_back(totflow?(gas_flow[2]/gas_flow[3]):0);
         gas_flow.push_back(totflow?(gas_flow[3]/totflow):0);

         gas->fV->WD("read_time", read_time);
         gas->fV->WIA("slice_cfg", slice_cfg);
         gas->fV->WIA("mfcrd_ai", mfcrd_ai);
         gas->fV->WIA("mfcrd_ao", mfcrd_ao);
         gas->fV->WIA("mfcrd_do", mfcrd_do);
         gas->fV->WIA("mfcrd_docfg", mfcrd_docfg);
         gas->fV->WIA("cord_do", cord_do);
         gas->fV->WDA("gas_flow_sccm", gas_flow);

         //eq->SetStatus("Ok", "#00FF00");
         char stat[64];
         sprintf(stat,"Gas Flow in: %1.0fccm     Return: %1.0f%%",
                 gas_flow[3],gas_flow[4]*1.e2);
         eq->SetStatus(stat, "#00FF00");

         if (gas->fFastUpdate != 0) {
            if (time(NULL) > gas->fFastUpdate)
               gas->fFastUpdate = 0;
         }

         if (gas->fFastUpdate) {
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
