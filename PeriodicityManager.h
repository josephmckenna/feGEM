#ifndef _PERIODICITY_MANAGER_
#define _PERIODICITY_MANAGER_
#include "tmfe.h"
#include "GEM_BANK.h"

class PeriodicityManager
{
   private:
   int fPeriod;
   int fNumberOfConnections;
   std::vector<std::string> RemoteCallers;
   TMFeEquipment* fEq;
   TMFE* fMfe;
   int fPeriodicWithData;
   int fPeriodicWithoutData;

   int fGEMBankEvents;
   double fGEMStatLastTime;
   

   MVOdb* fOdbStatistics; 

   //Used to track how much time it has been since we saw data
   std::chrono::time_point<std::chrono::system_clock> TimeOfLastData;

   public:
   PeriodicityManager(TMFE* mfe,TMFeEquipment* eq);
   const char* ProgramName(char* string);
   void AddRemoteCaller(char* prog);
   void LogPeriodicWithData();
   void LogPeriodicWithoutData();

   void AddBanksProcessed(int nbanks);
   void WriteGEMBankStatistics();

   void UpdatePerodicity();
   void ProcessMessage(GEMBANK<char>* bank);
   const int GetWaitPeriod() { return fPeriod; }
   
   double SecondsSinceData()
   {
        std::chrono::time_point<std::chrono::system_clock> timer_start = 
           std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> TimeSinceLastStatusUpdate = 
           timer_start - TimeOfLastData;
        return TimeSinceLastStatusUpdate.count();
   }
};

#endif
