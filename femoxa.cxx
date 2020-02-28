// femoxa.cxx
//
// MIDAS frontend for MOXA box

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
#include "mjson.h"

#define C(x) ((x).c_str())

#if 0
static int odbReadArraySize(TMFE* mfe, const char*name)
{
   int status;
   HNDLE hdir = 0;
   HNDLE hkey;
   KEY key;

   status = db_find_key(mfe->fDB, hdir, (char*)name, &hkey);
   if (status != DB_SUCCESS)
      return 0;

   status = db_get_key(mfe->fDB, hkey, &key);
   if (status != DB_SUCCESS)
      return 0;

   return key.num_values;
}

static int odbResizeArray(TMFE* mfe, const char*name, int tid, int size)
{
   int oldSize = odbReadArraySize(mfe, name);

   if (oldSize >= size)
      return oldSize;

   int status;
   HNDLE hkey;
   HNDLE hdir = 0;

   status = db_find_key(mfe->fDB, hdir, (char*)name, &hkey);
   if (status != SUCCESS) {
      mfe->Msg(MINFO, "odbResizeArray", "Creating \'%s\'[%d] of type %d", name, size, tid);
      
      status = db_create_key(mfe->fDB, hdir, (char*)name, tid);
      if (status != SUCCESS) {
         mfe->Msg(MERROR, "odbResizeArray", "Cannot create \'%s\' of type %d, db_create_key() status %d", name, tid, status);
         return -1;
      }
         
      status = db_find_key (mfe->fDB, hdir, (char*)name, &hkey);
      if (status != SUCCESS) {
         mfe->Msg(MERROR, "odbResizeArray", "Cannot create \'%s\', db_find_key() status %d", name, status);
         return -1;
      }
   }
   
   mfe->Msg(MINFO, "odbResizeArray", "Resizing \'%s\'[%d] of type %d, old size %d", name, size, tid, oldSize);

   status = db_set_num_values(mfe->fDB, hkey, size);
   if (status != SUCCESS) {
      mfe->Msg(MERROR, "odbResizeArray", "Cannot resize \'%s\'[%d] of type %d, db_set_num_values() status %d", name, size, tid, status);
      return -1;
   }
   
   return size;
}

double OdbGetValue(TMFE* mfe, const std::string& eqname, const char* varname, int i, int nch)
{
   std::string path;
   path += "/Equipment/";
   path += eqname;
   path += "/Settings/";
   path += varname;

   char bufn[256];
   sprintf(bufn,"[%d]", nch);

   double v = 0;
   int size = sizeof(v);

   int status = odbResizeArray(mfe, C(path), TID_DOUBLE, nch);

   if (status < 0) {
      return 0;
   }

   char bufi[256];
   sprintf(bufi,"[%d]", i);

   status = db_get_value(mfe->fDB, 0, C(path + bufi), &v, &size, TID_DOUBLE, TRUE);

   return v;
}
#endif

class Moxa: public TMFeRpcHandlerInterface
{
public:
   TMFE* mfe = NULL;
   TMFeEquipment* eq = NULL;
   KOtcpConnection* s = NULL;

#if 0
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
#endif

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
         
   void WD(const char* name, const double v)
   {
      if (mfe->fShutdown)
         return;
      
      std::string path;
      path += "/Equipment/";
      path += eq->fName;
      path += "/Variables/";
      path += name;
      int status = db_set_value(mfe->fDB, 0, C(path), &v, sizeof(v), 1, TID_DOUBLE);
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
      //printf("Write ODB %s Variables %s: %s\n", C(path), name, v);
      int status = db_set_value(mfe->fDB, 0, C(path), &v[0], sizeof(double)*v.size(), v.size(), TID_DOUBLE);
      if (status != DB_SUCCESS) {
         printf("WVD: db_set_value status %d\n", status);
      }
   }

   double WRead(const char* equipment, const char* name, int idx)
   {
      if (mfe->fShutdown)
         return -1.;
      
      std::string path;
      path += "/Equipment/";
      path += equipment;
      path += "/Variables/";
      path += name;
      path += "[";
      path += std::to_string(idx);
      path += "]";
      int size = sizeof(double);
      double value;
      int status = db_get_value(mfe->fDB, 0, C(path), &value, &size, TID_DOUBLE, FALSE);
      if (status != DB_SUCCESS) 
         {
            printf("WRead: db_get_value status %d\n", status);
            return -2.;
         }
      return value;
   }

   void Read()
   {
      s->fConnectTimeoutMilliSec = 5*1000;
      s->fReadTimeoutMilliSec = 5*1000;
      s->fWriteTimeoutMilliSec = 5*1000;

      KOtcpError e;

      std::vector<std::string> headers;
      headers.push_back("Accept: vdn.dac.v1");
      
      // Read DI
      // curl -X GET --header "Accept: vdn.dac.v1" http://moxa01/api/slot/0/io/di

      std::vector<std::string> reply_headers_di;
      std::string reply_body_di;

      e = s->HttpGet(headers, "/api/slot/0/io/di", &reply_headers_di, &reply_body_di);

      if (e.error) {
         eq->SetStatus("http error reading DI", "red");
         mfe->Msg(MERROR, "Read", "HttpGet() error %s", e.message.c_str());
         return;
      }

      //printf("di_json: %s\n", reply_body_di.c_str());

      WR("DI", reply_body_di.c_str());

      MJsonNode* di = MJsonNode::Parse(reply_body_di.c_str());
      //di->Dump();

      std::vector<double> di_counter_value;

      di_counter_value.push_back(di->FindObjectNode("io")->FindObjectNode("di")->GetArray()->at(0)->FindObjectNode("diCounterValue")->GetInt());

      printf("di_counter_value %f\n", di_counter_value[0]);

      delete di;

      WVD("di_counter_value", di_counter_value);

      // Read AI
      // curl -X GET --header "Accept: vdn.dac.v1" http://moxa01/api/slot/0/io/ai

      std::vector<std::string> reply_headers_ai;
      std::string reply_body_ai;

      e = s->HttpGet(headers, "/api/slot/0/io/ai", &reply_headers_ai, &reply_body_ai);

      if (e.error) {
         eq->SetStatus("http error reading AI", "red");
         mfe->Msg(MERROR, "Read", "HttpGet() error %s", e.message.c_str());
         return;
      }

      //printf("ai_json: %s\n", reply_body_ai.c_str());

      WR("AI", reply_body_ai.c_str());

      MJsonNode* ai = MJsonNode::Parse(reply_body_ai.c_str());
      //ai->Dump();

      std::vector<double> ai_raw;
      std::vector<double> ai_scaled;

      for (int i=0; i<4; i++) {
         ai_raw.push_back(ai->FindObjectNode("io")->FindObjectNode("ai")->GetArray()->at(i)->FindObjectNode("aiValueRaw")->GetInt());
         ai_scaled.push_back(ai->FindObjectNode("io")->FindObjectNode("ai")->GetArray()->at(i)->FindObjectNode("aiValueScaled")->GetDouble());
      }

      for (unsigned i=0; i<ai_raw.size(); i++) {
         printf("ai %d raw %f, scaled %f\n", i, ai_raw[i], ai_scaled[i]);
      }

      delete ai;

      WVD("ai_raw", ai_raw);
      WVD("ai_scaled", ai_scaled);

      WD("flow_H2O", di_counter_value[0]*0.0421);
      WD("T_H2O", ai_scaled[0]*1000. - 269.5);
      WD("h_H2O", ai_scaled[1]*3.80828 -28.3691);
      WD("p_vac", ai_scaled[2]*229.83);
      WD("p_back", ai_scaled[3]*229.83);

      e = s->Close();
      if (e.error) {
         //eq->SetStatus("Cannot connect", "red");
         mfe->Msg(MERROR, "Read", "Cannot disconnect from %s:%s, Close() error %s", s->fHostname.c_str(), s->fService.c_str(), e.message.c_str());
         return;
      }

      double cp_h20 = 4186.; // specific heat of water;
      double flow_cal = 0.0343;
      double deltaT = WRead("TMB01", "Cooling avgT", 15);
      deltaT /= 60.;
      double fQtherm = cp_h20*deltaT*di_counter_value[0]*flow_cal;
      //      std::string Qtherm = "Cooling Thermal Power " + std::to_string(fQtherm) + "[W]";
      char Qtherm[64];
      sprintf(Qtherm,"Thermal Cooling Power %1.0f[W]",fQtherm);
      eq->SetStatus(Qtherm, "#00FF00");
   }

   std::string HandleRpc(const char* cmd, const char* args)
   {
      mfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);
      return "OK";
   }
};

int main(int argc, char* argv[])
{
   setbuf(stdout, NULL);
   setbuf(stderr, NULL);

   signal(SIGPIPE, SIG_IGN);

   TMFE* mfe = TMFE::Instance();

   TMFeError err = mfe->Connect(C(std::string("femoxa01")));
   if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
   }

   //mfe->SetWatchdogSec(0);

   TMFeCommon *eqc = new TMFeCommon();
   eqc->EventID = 4;
   eqc->FrontendName = "femoxa01";
   eqc->LogHistory = 1;
   
   TMFeEquipment* eq = new TMFeEquipment("TpcCooling");
   eq->Init(mfe->fOdbRoot, eqc);
   eq->SetStatus("Starting...", "white");

   mfe->RegisterEquipment(eq);

   KOtcpConnection* s = new KOtcpConnection("moxa01", "http");

   class Moxa* moxa = new Moxa;

   moxa->mfe = mfe;
   moxa->eq = eq;
   moxa->s = s;

   mfe->RegisterRpcHandler(moxa);
   mfe->SetTransitionSequence(-1, -1, -1, -1);

   while (!mfe->fShutdown) {

      moxa->Read();
         
      for (int i=0; i<10; i++) {
         mfe->PollMidas(1000);
         if (mfe->fShutdown)
            break;
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
