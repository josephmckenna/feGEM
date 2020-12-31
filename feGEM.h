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
   feGEMClass(TMFE* mfe, TMFeEquipment* eq , AllowedHosts* hosts, int type, int debugMode = 0 ):
      feGEMClassType(type),
      periodicity(mfe,eq),
      message(mfe),
      logger(mfe,eq)
   {
      allowed_hosts = hosts;
      fDebugMode = debugMode;
      char hostname[100];
      gethostname(hostname,100);
      //Store as std::string in this class
      thisHostname = hostname;
      //Hostname must be known!
      assert(thisHostname.size()>0);
   }
   ~feGEMClass() // dtor
   {
      TCP_thread.join();
      if (fEventBuf) {
         free(fEventBuf);
         fEventBuf = NULL;
      }
   }
   virtual int FindHostInWorkerList(const char* hostname) { assert(0); return -1; };
   virtual uint16_t AssignPortForWorker(uint workerID) { assert(0); return 0; };
   virtual const char* AddNewClient(const char* hostname) { assert(0); return NULL; };

   std::string HandleRpc(const char* cmd, const char* args);
   void HandleBeginRun();
   void HandleEndRun();
   void HandleStrBank(GEMBANK<char>* bank,const char* hostname);
   void HandleStrArrayBank(GEMBANK<char>* bank,const char* hostname);
   void HandleCommandBank(const GEMDATA<char>* bank,const char* command,const char* hostname);
   void LogBank(const char* buf,const char* hostname);
   int HandleBankArray(const char * ptr,const char* hostname);
   int HandleBank(const char * ptr,const char* hostname);

   void SetRateStatus();

   void HandlePeriodic() {};
   void ServeHost();
   void Run();
};


class feGEMWorker :
   public feGEMClass
{
   public:
   MVOdb* fOdbSupervisorSettings;
   feGEMWorker(TMFE* mfe, TMFeEquipment* eq, AllowedHosts* hosts, int debugMode = 0);
   void Init(MVOdb* supervisor_settings_path);

};

class feGEMSupervisor :
   public feGEMClass
{
public:
   MVOdb* fOdbWorkers;

   int fPortRangeStart;
   int fPortRangeStop;

   feGEMSupervisor(TMFE* mfe, TMFeEquipment* eq);

   void Init();
private:
   std::vector<uint> RunningWorkers;
public:
   bool WorkerIsRunning(uint workerID)
   {
      for (auto& id: RunningWorkers)
      {
         if (id==workerID)
         {
            std::cout<<"Working is already runnning"<<std::endl;
            return true;
         }
      }
      std::cout<<"Working is not yet runnning"<<std::endl;
      return false;
   };
   void WorkerStarted(uint workerID) { RunningWorkers.push_back(workerID); };
   virtual int FindHostInWorkerList(const char* hostname);
   virtual uint16_t AssignPortForWorker(uint workerID);
   virtual const char* AddNewClient(const char* hostname);
  
};
