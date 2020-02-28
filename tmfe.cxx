/********************************************************************\

  Name:         tmfe.cxx
  Created by:   Konstantin Olchanski - TRIUMF

  Contents:     C++ MIDAS frontend

\********************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/time.h> // gettimeofday()
//#include <string>

#include "tmfe.h"

#include "midas.h"
#include "mrpc.h"

#define C(x) ((x).c_str())

TMFE::TMFE() // ctor
{
   fDB = 0;
   fOdbRoot = NULL;
   fShutdown = false;
}

TMFE::~TMFE() // dtor
{
   assert(!"TMFE::~TMFE(): destruction of the TMFE singleton is not permitted!");
}

TMFE* TMFE::Instance()
{
   if (!gfMFE)
      gfMFE = new TMFE();
   
   return gfMFE;
}

TMFeError TMFE::Connect(const char*progname, const char*hostname, const char*exptname)
{
   int status;
  
   char xhostname[HOST_NAME_LENGTH];
   char xexptname[NAME_LENGTH];
   
   /* get default from environment */
   status = cm_get_environment(xhostname, sizeof(xhostname), xexptname, sizeof(xexptname));
   assert(status == CM_SUCCESS);
   
   if (hostname)
      strlcpy(xhostname, hostname, sizeof(xhostname));
   
   if (exptname)
      strlcpy(xexptname, exptname, sizeof(xexptname));
   
   fHostname = xhostname;
   fExptname = xexptname;
   
   fprintf(stderr, "TMFE::Connect: Program \"%s\" connecting to experiment \"%s\" on host \"%s\"\n", progname, fExptname.c_str(), fHostname.c_str());
   
   int watchdog = DEFAULT_WATCHDOG_TIMEOUT;
   //int watchdog = 60*1000;
   
   status = cm_connect_experiment1((char*)fHostname.c_str(), (char*)fExptname.c_str(), (char*)progname, NULL, DEFAULT_ODB_SIZE, watchdog);
   
   if (status == CM_UNDEF_EXP) {
      fprintf(stderr, "TMidasOnline::connect: Error: experiment \"%s\" not defined.\n", fExptname.c_str());
      return TMFeError(status, "experiment is not defined");
   } else if (status != CM_SUCCESS) {
      fprintf(stderr, "TMidasOnline::connect: Cannot connect to MIDAS, status %d.\n", status);
      return TMFeError(status, "cannot connect");
   }

   status = cm_get_experiment_database(&fDB, NULL);
   if (status != CM_SUCCESS) {
      return TMFeError(status, "cm_get_experiment_database");
   }

   fOdbRoot = MakeOdb(fDB);
  
   return TMFeError();
}

TMFeError TMFE::SetWatchdogSec(int sec)
{
   if (sec == 0) {
      cm_set_watchdog_params(false, 0);
   } else {
      cm_set_watchdog_params(true, sec*1000);
   }
   return TMFeError();
}

TMFeError TMFE::Disconnect()
{
   fprintf(stderr, "TMFE::Disconnect: Disconnecting from experiment \"%s\" on host \"%s\"\n", fExptname.c_str(), fHostname.c_str());
   cm_disconnect_experiment();
   return TMFeError();
}

TMFeError TMFE::RegisterEquipment(TMFeEquipment* eq)
{
   fEquipments.push_back(eq);
   return TMFeError();
}

void TMFE::PollMidas(int msec)
{
   int status = cm_yield(msec);
   
   if (status == RPC_SHUTDOWN || status == SS_ABORT) {
      fShutdown = true;
      fprintf(stderr, "TMFE::PollMidas: cm_yield(%d) status %d, shutdown requested...\n", msec, status);
      //disconnect();
      //return false;
   }
}

void TMFE::MidasPeriodicTasks()
{
   cm_periodic_tasks();
}

void TMFE::Msg(int message_type, const char *filename, int line, const char *routine, const char *format, ...)
{
   char message[1024];
   //printf("format [%s]\n", format);
   va_list ap;
   va_start(ap, format);
   vsnprintf(message, sizeof(message)-1, format, ap);
   va_end(ap);
   //printf("message [%s]\n", message);
   cm_msg(message_type, filename, line, routine, "%s", message);
   cm_msg_flush_buffer();
}

double TMFE::GetTime()
{
   struct timeval tv;
   gettimeofday(&tv, NULL);
   return tv.tv_sec*1.0 + tv.tv_usec/1000000.0;
}

std::string TMFeRpcHandlerInterface::HandleRpc(const char* cmd, const char* args)
{
   return "";
}

void TMFeRpcHandlerInterface::HandleBeginRun()
{
}

void TMFeRpcHandlerInterface::HandleEndRun()
{
}

void TMFeRpcHandlerInterface::HandlePauseRun()
{
}

void TMFeRpcHandlerInterface::HandleResumeRun()
{
}

static INT rpc_callback(INT index, void *prpc_param[])
{
   const char* cmd  = CSTRING(0);
   const char* args = CSTRING(1);
   char* return_buf = CSTRING(2);
   int   return_max_length = CINT(3);

   cm_msg(MINFO, "rpc_callback", "--------> rpc_callback: index %d, max_length %d, cmd [%s], args [%s]", index, return_max_length, cmd, args);

   TMFE* mfe = TMFE::Instance();

   for (unsigned i=0; i<mfe->fRpcHandlers.size(); i++) {
      std::string r = mfe->fRpcHandlers[i]->HandleRpc(cmd, args);
      if (r.length() > 0) {
         //printf("Handler reply [%s]\n", C(r));
         strlcpy(return_buf, C(r), return_max_length);
         return RPC_SUCCESS;
      }
   }

   return_buf[0] = 0;
   return RPC_SUCCESS;
}

static INT tr_start(INT runno, char *errstr)
{
   cm_msg(MINFO, "tr_start", "tr_start");

   TMFE* mfe = TMFE::Instance();
   
   for (unsigned i=0; i<mfe->fEquipments.size(); i++) {
      mfe->fEquipments[i]->ZeroStatistics();
      mfe->fEquipments[i]->WriteStatistics();
   }

   for (unsigned i=0; i<mfe->fRpcHandlers.size(); i++) {
      mfe->fRpcHandlers[i]->HandleBeginRun();
   }

   return SUCCESS;
}

static INT tr_stop(INT runno, char *errstr)
{
   cm_msg(MINFO, "tr_stop", "tr_stop");

   TMFE* mfe = TMFE::Instance();
   for (unsigned i=0; i<mfe->fRpcHandlers.size(); i++) {
      mfe->fRpcHandlers[i]->HandleEndRun();
   }

   for (unsigned i=0; i<mfe->fEquipments.size(); i++) {
      mfe->fEquipments[i]->WriteStatistics();
   }


   return SUCCESS;
}

static INT tr_pause(INT runno, char *errstr)
{
   cm_msg(MINFO, "tr_pause", "tr_pause");

   TMFE* mfe = TMFE::Instance();
   for (unsigned i=0; i<mfe->fRpcHandlers.size(); i++) {
      mfe->fRpcHandlers[i]->HandlePauseRun();
   }

   return SUCCESS;
}

static INT tr_resume(INT runno, char *errstr)
{
   cm_msg(MINFO, "tr_resume", "tr_resume");

   TMFE* mfe = TMFE::Instance();
   for (unsigned i=0; i<mfe->fRpcHandlers.size(); i++) {
      mfe->fRpcHandlers[i]->HandleResumeRun();
   }

   return SUCCESS;
}

void TMFE::RegisterRpcHandler(TMFeRpcHandlerInterface* h)
{
   if (fRpcHandlers.size() == 0) {
      // for the first handler, register with MIDAS
      cm_register_function(RPC_JRPC, rpc_callback);
      cm_register_transition(TR_START, tr_start, 500);
      cm_register_transition(TR_STOP, tr_stop, 500);
      cm_register_transition(TR_PAUSE, tr_pause, 500);
      cm_register_transition(TR_RESUME, tr_resume, 500);
   }

   fRpcHandlers.push_back(h);
}

void TMFE::SetTransitionSequence(int start, int stop, int pause, int resume)
{
   if (start>=0)
      cm_set_transition_sequence(TR_START, start);
   if (stop>=0)
      cm_set_transition_sequence(TR_STOP, stop);
   if (pause>=0)
      cm_set_transition_sequence(TR_PAUSE, pause);
   if (resume>=0)
      cm_set_transition_sequence(TR_RESUME, resume);

   if (start==-1)
      cm_deregister_transition(TR_START);
   if (stop==-1)
      cm_deregister_transition(TR_STOP);
   if (pause==-1)
      cm_deregister_transition(TR_PAUSE);
   if (resume==-1)
      cm_deregister_transition(TR_RESUME);
}

TMFeCommon::TMFeCommon() // ctor
{
   EventID = 0;;
   TriggerMask = 0;
   Buffer = "SYSTEM";
   Type = 0;
   Source = 0;
   Format = "MIDAS";
   Enabled = true;
   ReadOn = 0;
   Period = 0;
   EventLimit = 0;
   NumSubEvents = 0;
   LogHistory = 0;
   //FrontendHost;
   //FrontendName;
   //Status;
   //StatusColor;
   Hidden = false;
};

TMFeError WriteToODB(const char* path, const TMFeCommon* c)
{
   HNDLE hDB, hKey;
   int status;
   status = cm_get_experiment_database(&hDB, NULL);
   if (status != CM_SUCCESS) {
      return TMFeError(status, "cm_get_experiment_database");
   }
   status = db_find_key(hDB, 0, path, &hKey);
   if (status == DB_NO_KEY) {
      status = db_create_key(hDB, 0, path, TID_KEY);
      if (status != DB_SUCCESS) {
         return TMFeError(status, "db_create_key");
      }
      status = db_find_key(hDB, 0, path, &hKey);
   }
   if (status != DB_SUCCESS) {
      printf("find status %d\n", status);
      return TMFeError(status, "db_find_key");
   }
   printf("WriteToODB: hDB %d, hKey %d\n", hDB, hKey);
   status = db_set_value(hDB, hKey, "Event ID", &c->EventID, 2, 1, TID_WORD);
   status = db_set_value(hDB, hKey, "Trigger mask", &c->TriggerMask, 2, 1, TID_WORD);
   status = db_set_value(hDB, hKey, "Buffer", c->Buffer.c_str(), 32, 1, TID_STRING);
   status = db_set_value(hDB, hKey, "Type", &c->Type, 4, 1, TID_INT);
   status = db_set_value(hDB, hKey, "Source", &c->Source, 4, 1, TID_INT);
   status = db_set_value(hDB, hKey, "Format", c->Format.c_str(), 8, 1, TID_STRING);
   status = db_set_value(hDB, hKey, "Enabled", &c->Enabled, 4, 1, TID_BOOL);
   status = db_set_value(hDB, hKey, "Read on", &c->ReadOn, 4, 1, TID_INT);
   status = db_set_value(hDB, hKey, "Period", &c->Period, 4, 1, TID_INT);
   status = db_set_value(hDB, hKey, "Event limit", &c->EventLimit, 8, 1, TID_DOUBLE);
   status = db_set_value(hDB, hKey, "Num subevents", &c->NumSubEvents, 4, 1, TID_DWORD);
   status = db_set_value(hDB, hKey, "Log history", &c->LogHistory, 4, 1, TID_INT);
   status = db_set_value(hDB, hKey, "Frontend host", c->FrontendHost.c_str(), 32, 1, TID_STRING);
   status = db_set_value(hDB, hKey, "Frontend name", c->FrontendName.c_str(), 32, 1, TID_STRING);
   status = db_set_value(hDB, hKey, "Frontend file name", c->FrontendFileName.c_str(), 256, 1, TID_STRING);
   status = db_set_value(hDB, hKey, "Status", c->Status.c_str(), 256, 1, TID_STRING);
   status = db_set_value(hDB, hKey, "Status color", c->StatusColor.c_str(), 32, 1, TID_STRING);
   status = db_set_value(hDB, hKey, "Hidden", &c->Hidden, 4, 1, TID_BOOL);
   printf("set status %d\n", status);
   return TMFeError();
}

TMFeError ReadFromODB(const char* path, const TMFeCommon*d, TMFeCommon* c)
{
   HNDLE hDB, hKey;
   int status;

   status = cm_get_experiment_database(&hDB, NULL);
   if (status != CM_SUCCESS) {
      return TMFeError(status, "cm_get_experiment_database");
   }

   status = db_find_key(hDB, 0, path, &hKey);
   if (status != DB_SUCCESS) {
      return TMFeError(status, "db_find_key");
   }

   printf("ReadFromODB: hDB %d, hKey %d\n", hDB, hKey);

   *c = *d; // FIXME

   return TMFeError();
}

TMFeEquipment::TMFeEquipment(const char* name) // ctor
{
   fName = name;
   fCommon = NULL;
   fDefaultCommon = NULL;
   fBuffer = 0;
   fSerial = 0;
   fStatEvents = 0;
   fStatBytes  = 0;
   fStatEpS = 0;
   fStatKBpS = 0;
   fStatLastTime = 0;
   fStatLastEvents = 0;
   fStatLastBytes = 0;
   fOdbEq = NULL;
   fOdbEqCommon = NULL;
   fOdbEqSettings = NULL;
   fOdbEqVariables = NULL;
}

TMFeError TMFeEquipment::Init(TMVOdb* odb, TMFeCommon* defaults)
{
   //
   // create ODB /eq/name/common
   //

   std::string path = "/Equipment/" + fName + "/Common";

   fDefaultCommon = defaults;
   fCommon = new TMFeCommon();

   TMFeError err = ReadFromODB(path.c_str(), defaults, fCommon);
   if (err.error) {
      err = WriteToODB(path.c_str(), fDefaultCommon);
      if (err.error) {
         return err;
      }

      err = ReadFromODB(path.c_str(), defaults, fCommon);
      if (err.error) {
         return err;
      }
   }

   fOdbEq = odb->Chdir((std::string("Equipment/") + fName).c_str(), true);
   fOdbEqCommon = fOdbEq->Chdir("Common", true);
   fOdbEqSettings = fOdbEq->Chdir("Settings", true);
   fOdbEqVariables = fOdbEq->Chdir("Variables", true);

   int status = bm_open_buffer(fCommon->Buffer.c_str(), DEFAULT_BUFFER_SIZE, &fBuffer);
   printf("open_buffer %d\n", status);
   if (status != BM_SUCCESS) {
      return TMFeError(status, "bm_open_buffer");
   }

   WriteStatistics();

   return TMFeError();
};

TMFeError TMFeEquipment::ZeroStatistics()
{
   fStatEvents = 0;
   fStatBytes = 0;
   fStatEpS = 0;
   fStatKBpS = 0;
   
   fStatLastTime = 0;
   fStatLastEvents = 0;
   fStatLastBytes = 0;

   return TMFeError();
}

TMFeError TMFeEquipment::WriteStatistics()
{
   HNDLE hDB;
   int status;

   status = cm_get_experiment_database(&hDB, NULL);
   if (status != CM_SUCCESS) {
      return TMFeError(status, "cm_get_experiment_database");
   }

   double now = ss_millitime()/1000.0;

   double elapsed = now - fStatLastTime;

   if (elapsed > 0.5 || fStatLastTime == 0) {
      fStatEpS = (fStatEvents - fStatLastEvents) / elapsed;
      fStatKBpS = (fStatBytes - fStatLastBytes) / elapsed / 1000.0;

      fStatLastTime = now;
      fStatLastEvents = fStatEvents;
      fStatLastBytes = fStatBytes;
   }
   
   status = db_set_value(hDB, 0, C("/Equipment/" + fName + "/Statistics/Events sent"), &fStatEvents, 8, 1, TID_DOUBLE);
   status = db_set_value(hDB, 0, C("/Equipment/" + fName + "/Statistics/Events per sec."), &fStatEpS, 8, 1, TID_DOUBLE);
   status = db_set_value(hDB, 0, C("/Equipment/" + fName + "/Statistics/kBytes per sec."), &fStatKBpS, 8, 1, TID_DOUBLE);

   return TMFeError();
}

TMFeError TMFeEquipment::ComposeEvent(char* event, int size)
{
   EVENT_HEADER* pevent = (EVENT_HEADER*)event;
   pevent->event_id = fCommon->EventID;
   pevent->trigger_mask = fCommon->TriggerMask;
   pevent->serial_number = fSerial;
   pevent->time_stamp = 0; // FIXME
   pevent->data_size = 0;
   return TMFeError();
}

TMFeError TMFeEquipment::SendData(const char* buf, int size)
{
   int status = bm_send_event(fBuffer, (const EVENT_HEADER*)buf, size, BM_WAIT);
   if (status == BM_CORRUPTED) {
      TMFE::Instance()->Msg(MERROR, "TMFeEquipment::SendData", "bm_send_event() returned %d, event buffer is corrupted, shutting down the frontend", status);
      TMFE::Instance()->fShutdown = true;
      return TMFeError(status, "bm_send_event: event buffer is corrupted, shutting down the frontend");
   } else if (status != BM_SUCCESS) {
      return TMFeError(status, "bm_send_event");
   }
   fStatEvents += 1;
   fStatBytes  += size;
   return TMFeError();
}

TMFeError TMFeEquipment::SendEvent(const char* event)
{
   fSerial++;
   return SendData(event, sizeof(EVENT_HEADER) + BkSize(event));
}

int TMFeEquipment::BkSize(const char* event)
{
   return bk_size((void*)(event + sizeof(EVENT_HEADER))); // FIXME: need const in prototype!
}

TMFeError TMFeEquipment::BkInit(char* event, int size)
{
   bk_init32(event + sizeof(EVENT_HEADER));
   return TMFeError();
}

void* TMFeEquipment::BkOpen(char* event, const char* name, int tid)
{
   void* ptr;
   bk_create(event + sizeof(EVENT_HEADER), name, tid, &ptr);
   return ptr;
}

TMFeError TMFeEquipment::BkClose(char* event, void* ptr)
{
   bk_close(event + sizeof(EVENT_HEADER), ptr);
   ((EVENT_HEADER*)event)->data_size = BkSize(event);
   return TMFeError();
}

TMFeError TMFeEquipment::SetStatus(char const* eq_status, char const* eq_color)
{
   HNDLE hDB;
   int status;

   status = cm_get_experiment_database(&hDB, NULL);
   if (status != CM_SUCCESS) {
      return TMFeError(status, "cm_get_experiment_database");
   }

   if (eq_status) {
      char s[256];
      strlcpy(s, eq_status, sizeof(s));
   
      status = db_set_value(hDB, 0, C("/Equipment/" + fName + "/Common/Status"), s, sizeof(s), 1, TID_STRING);
      if (status != DB_SUCCESS) {
         return TMFeError(status, "db_set_value(Common/Status)");
      }
   }

   if (eq_color) {
      char c[32];
      strlcpy(c, eq_color, sizeof(c));

      status = db_set_value(hDB, 0, C("/Equipment/" + fName + "/Common/Status color"), c, sizeof(c), 1, TID_STRING);
      if (status != DB_SUCCESS) {
         return TMFeError(status, "db_set_value(Common/Status color)");
      }
   }

   return TMFeError();
}

TMFeError TMFE::TriggerAlarm(const char* name, const char* message, const char* aclass)
{
   int status = al_trigger_alarm(name, message, aclass, message, AT_INTERNAL);

   if (status) {
      return TMFeError(status, "al_trigger_alarm");
   }

   return TMFeError();
}

TMFeError TMFE::ResetAlarm(const char* name)
{
   int status = al_reset_alarm(name);

   if (status) {
      return TMFeError(status, "al_reset_alarm");
   }

   return TMFeError();
}

// singleton instance
TMFE* TMFE::gfMFE = NULL;

//std::mutex* TMFE::gfMidasLock = NULL;

//std::mutex* TMFE::GetMidasLock() {
//   return gfMidasLock;
//}

//void TMFE::CreateMidasLock() {
//   assert(gfMidasLock == NULL);
//   gfMidasLock = new std::mutex;
//}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
