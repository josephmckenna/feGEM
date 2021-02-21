#ifndef FEGEM_CLASS_
#define FEGEM_CLASS_

#include <thread>

#include "midas.h"
#include "tmfe.h"
#include "msystem.h"

#include "GEM_BANK.h"
#include "MessageHandler.h"
#include "AllowedHosts.h"
#include "HistoryLogger.h"
#include "PeriodicityManager.h"
#include "SettingsFileDatabase.h"

class feGEMClass :
   public TMFeRpcHandlerInterface,
   public TMFePeriodicHandlerInterface
{
public:
   TMFE* fMfe;
   TMFeEquipment* fEq;
   enum RunStatusType{Unknown,Running,Stopped};
   enum feGEMClassEnum{SUPERVISOR,WORKER,INVALID};
   const int feGEMClassType;
   //TCP stuff
   int server_fd;
   struct sockaddr_in address;
   int addrlen = sizeof(address);
   int fPort;
   std::thread TCP_thread;
   //TCP read rate:
   PeriodicityManager periodicity;
   
   int fEventSize;
   int lastEventSize; //Used to monitor any changes to fEventSize
   char* fEventBuf;
   //JSON Verbosity control
   int fDebugMode;

   std::string thisHostname;
   
   //Elog posting settings
   std::string elogHost = "";
   int fMaxElogPostSize; //Used to put a safetly limit on the size of an elog post
   
   std::chrono::time_point<std::chrono::system_clock> LastStatusUpdate;

   //Periodic task query items (sould only be send from worker class... not yet limited)
   RunStatusType RunStatus;
   int RUNNO;
   uint32_t RUN_START_T;
   uint32_t  RUN_STOP_T;

   SettingsFileDatabase* SettingsDataBase=NULL;

   // Network security
   AllowedHosts* allowed_hosts;

   
   // JSON reply manager
   MessageHandler message;

   HistoryLogger logger;
   feGEMClass(TMFE* mfe, TMFeEquipment* eq , AllowedHosts* hosts, int type, int debugMode = 0 );
   ~feGEMClass();

   virtual int FindHostInWorkerList(const char* hostname) { assert(0); return -1; };
   virtual uint16_t AssignPortForWorker(uint workerID) { assert(0); return 0; };
   virtual const char* AddNewClient(const char* hostname) { assert(0); return NULL; };

   std::string HandleRpc(const char* cmd, const char* args);
   void HandleBeginRun();
   void HandleEndRun();
   void HandleStrBank(GEMBANK<char>* bank, const char* hostname);
   void HandleFileBank(GEMBANK<char>* bank,const char* hostname);
   void HandleStrArrayBank(GEMBANK<char>* bank);
   void HandleCommandBank(const GEMDATA<char>* bank,const char* command,const char* hostname);
   void PostElog(GEMBANK<char>* bank, const char* hostname);
   void LogBank(const char* buf,const char* hostname);
   int HandleBankArray(const char * ptr,const char* hostname);
   int HandleBank(const char * ptr,const char* hostname);

   void SetFEStatus(int seconds_since_last_post = 0);

   void HandlePeriodic() {};
   void ServeHost();
   void Run();
};

#endif
