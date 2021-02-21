#include "feGEMClass.h"
#include "feGEMWorker.h"
#include "feGEMSupervisor.h"

static void usage()
{
   fprintf(stderr, "Usage: feGEM.exe\n");
   
   exit(1);
}

int main(int argc, char* argv[])
{
   setbuf(stdout, NULL);
   setbuf(stderr, NULL);

   signal(SIGPIPE, SIG_IGN);

   std::vector<std::string> args;
   for (int i=0; i<argc; i++) {
      if (strncmp(argv[i],"-h",2)==0)
         usage(); // does not return
      args.push_back(argv[i]);
   }

   bool SupervisorMode=true;
   std::string client="NULL";
   int port          =12345;
   int max_event_size=0;

   // loop over the commandline options
   for (unsigned int i=1; i<args.size(); i++) 
   {
      const char* arg = args[i].c_str();
      //printf("argv[%d] is %s\n",i,arg);

      if (strncmp(arg,"--client",8)==0) {
         client = args[++i];
         SupervisorMode=false;
      } else if (strncmp(arg,"--max_event_size",16)==0) {
         max_event_size= atoi(args[++i].c_str()); 
      } else {
         usage();
      }

   }
   if (!SupervisorMode && strcmp(client.c_str(),"NULL")==0)
   {
      std::cout<<"No client named to connect to..."<<std::endl;
      usage();
   }
   std::string name = "fe";
   if (SupervisorMode)
   {
      std::cout<<"Starting in supervisor mode"<<std::endl;
      name+="GEM";
   }
   else
   {
      name+="GEM_";
      name+=client.c_str();
   }

   TMFE* mfe = TMFE::Instance();
   if (name.size()>31)
   {
      mfe->Msg(MERROR,"feGEM", "Frontend name [%s] too long. Perhaps shorten hostname", name.c_str());
   }
   TMFeError err = mfe->Connect(name.c_str(), __FILE__);
   if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
   }

   //mfe->SetWatchdogSec(0);

   TMFeCommon *common = new TMFeCommon();
   common->EventID = 1;
   common->LogHistory = 1;
   //common->Buffer = "SYSTEM";
   
   TMFeEquipment* eq = new TMFeEquipment(mfe, name.c_str(), common);

   

   eq->Init();
   //If not default setting, update ODB
   if (max_event_size!=0)
      eq->fOdbEqSettings->WI("event_size", max_event_size);
   eq->fOdbEqSettings->RI("supervisor_port", &port, true);
   //eq->SetStatus("Starting...", "white");
   eq->ZeroStatistics();
   eq->WriteStatistics();

   mfe->RegisterEquipment(eq);

   if (SupervisorMode)
   {
      feGEMSupervisor* myfe= new feGEMSupervisor(mfe,eq);
      myfe->fPort=port;
      mfe->RegisterRpcHandler(myfe);
      std::cout<<"Initialise"<<std::endl;
      myfe->Init();
      std::cout<<"Register periodic"<<std::endl;
      mfe->RegisterPeriodicHandler(eq, myfe);
      std::cout<<"Register done"<<std::endl;
   }
   else
   {
      //Probably broken
      feGEMWorker* myfe = new feGEMWorker(mfe,eq,new AllowedHosts(mfe),client.c_str());
      myfe->fPort=port;
      mfe->RegisterRpcHandler(myfe);
      myfe->Init(eq->fOdbEqSettings);
      mfe->RegisterPeriodicHandler(eq, myfe);
   }
   //mfe->SetTransitionSequenceStart(910);
   //mfe->SetTransitionSequenceStop(90);
   //mfe->DeregisterTransitionPause();
   //mfe->DeregisterTransitionResume();

   //eq->SetStatus(name.c_str(), "greenLight");

   while (!mfe->fShutdownRequested) {
      mfe->PollMidas(10);
   }

   mfe->Disconnect();

   return 0;
}
