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

   public:
   PeriodicityManager(TMFE* mfe,TMFeEquipment* eq);
   const char* ProgramName(char* string);
   void AddRemoteCaller(char* prog);
   void LogPeriodicWithData();
   void LogPeriodicWithoutData();
   void UpdatePerodicity();
   void ProcessMessage(GEMBANK<char>* bank);
   const int GetWaitPeriod() { return fPeriod; }
};

#endif
