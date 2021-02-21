#include "feGEMClass.h"
#include "feGEMWorker.h"

#ifndef FEGEM_SUPERVISOR_
#define FEGEM_SUPERVISOR_

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
   std::string BuildFrontendName(const char* hostname);
   virtual const char* AddNewClient(const char* hostname);
   
   virtual void SetFEStatus()
   {
      size_t threads = RunningWorkers.size();
      std::string status = 
         "" + fMfe->fFrontendName + "@" + fMfe->fFrontendHostname +
         " [" + std::to_string(threads) + " thread";
      if (threads > 1)
         status += "s";
      status += "]";
      fEq->SetStatus(status.c_str(), "greenLight");
  }
};


#endif
