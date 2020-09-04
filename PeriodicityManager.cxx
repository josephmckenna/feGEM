#include "PeriodicityManager.h"
//--------------------------------------------------
// PeriodicityManager Class
// A class to update the rate at which we poll the Periodic() function
// Data Packers send data once per second... 
// By default poll once per second, poll at 1/ Number of connections + 1 seconds
// Ie Match the number of connections and keep one Periodic() per second free for new connections
//--------------------------------------------------

PeriodicityManager::PeriodicityManager(TMFE* mfe,TMFeEquipment* eq)
{
   fPeriod=1000;
   fEq=eq;
   fMfe=mfe;
   fNumberOfConnections=1;
   fPeriodicWithData=0;
   fPeriodicWithoutData=0;
}

const char* PeriodicityManager::ProgramName(char* string)
{
   int length=strlen(string);
   for (int i=0; i<=length; i++)
   {
      if (strncmp((string+i),"PROGRAM:",8)==0)
      {
         return string+i+8;
      }
   }
   return "Badly formatted string";
}

void PeriodicityManager::AddRemoteCaller(char* prog)
{
   for (auto item: RemoteCallers)
   {
      if (strcmp(prog,item.c_str())==0)
      {
         std::cout<<"Restarted program detected ("<<ProgramName(prog)<<")! Total:"<<fNumberOfConnections<<std::endl;
         fMfe->Msg(MTALK, fEq->fName.c_str(), "Restart of program %s detected",ProgramName(prog));
         return;
      }
   }
   //Item not already in list
   ++fNumberOfConnections;
   std::cout<<"New connection detected ("<<ProgramName(prog)<<")! Total:"<<fNumberOfConnections<<std::endl;
   RemoteCallers.push_back(prog);
}

void PeriodicityManager::LogPeriodicWithData()
{
   fPeriodicWithData++;
   // Every 1000 events, check that we are have more polls that data being sent 
   // (UpdatePerodicity should keep this up to date)
   if (fPeriodicWithData%1000==0)
   {
      // fPeriodicWithData / fPeriodicWithoutData ~= fNumberOfConnections;
      double EstimatedConnections = (double)fPeriodicWithData / (double)fPeriodicWithoutData;
      double tolerance=0.1; //10 percent
      if ( EstimatedConnections > fNumberOfConnections*(1+tolerance) )
      {
         //The usage of the periodic tasks is beyond spec... perhaps a user didn't initialise connect properly
         fMfe->Msg(MTALK,
                  fEq->fName.c_str(), "%s periodic tasks are very busy... miss use of the LabVIEW library?",
                  fEq->fName.c_str()
                  );
         fMfe->Msg(MINFO,
                  fEq->fName.c_str(),
                  "Estimated connections:  %d periodics with data /  %d periodics without  > %d fNumberOfConnections",
                  fPeriodicWithData,
                  fPeriodicWithoutData,
                  fNumberOfConnections);
      }
      if ( EstimatedConnections < fNumberOfConnections*(1-tolerance) )
      {
         std::cout<<"Traffic low or inconsistance for fronent:"<<fMfe->fFrontendName.c_str()<<std::endl;
      }
   }
}

void PeriodicityManager::LogPeriodicWithoutData()
{
   fPeriodicWithoutData++;
}

void PeriodicityManager::UpdatePerodicity()
{
  std::cout<<fPeriod <<" > "<< 1000 <<"./"<< (double)fNumberOfConnections<< " +  1"<<std::endl;
   if (fPeriod > 1000./ ((double)fNumberOfConnections + 1))
   {
     fPeriod = 1000./ ((double)fNumberOfConnections + 1);
      std::cout<<"Periodicity increase to "<<fPeriod<<"ms"<<std::endl;
   }
   fPeriodicWithData=0;
   fPeriodicWithoutData=0;
}

void PeriodicityManager::ProcessMessage(GEMBANK<char>* bank)
{
   //std::cout<<(char*)&(bank->DATA[0].DATA)<<std::endl;
   if ((strncmp((char*)&(bank->DATA->DATA),"New labview connection from",27)==0) ||
       (strncmp((char*)&(bank->DATA->DATA),"New python connection from",26)==0))
   {
      size_t NameLength=1023;
      char ProgramName[NameLength+1];
      // Protect against a segfault if program name is very long:
      // trim at 'NameLength' from above
      if (bank->BlockSize-16<NameLength)
         NameLength=bank->BlockSize-16;
      snprintf(ProgramName,NameLength,"%s",(char*)&(bank->DATA[0].DATA));
      AddRemoteCaller(ProgramName);
      UpdatePerodicity();
   }
   return;
}
