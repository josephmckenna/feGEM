//
// feGEM.cxx
//
// Frontend for two way communication to labVIEW (and or python)
// JTK McKENNA
//
#include "feGEMWorker.h"

//--------------------------------------------------
// Base class for LabVIEW frontend
// Child classes:
//    feGEM supervisor (recieves new connections and refers a host to a worker)
//    feGEM worker (one worker per host)
//--------------------------------------------------

feGEMWorker::feGEMWorker(TMFE* mfe, TMFeEquipment* eq, AllowedHosts* hosts, const char* client_hostname, int debugMode): feGEMClass(mfe,eq,hosts,WORKER,debugMode)
{
   fMfe = mfe;
   fEq  = eq;
   logger.SetClientHostname(client_hostname);
   //Default event size ok 10kb, will be overwritten by ODB entry in Init()
   fEventSize = 10000;
   fEventBuf  = NULL;
   SetFEStatus();
   
   //Default size limit on elog posts is 1MB
   fMaxElogPostSize = 1000000;

   // Creating socket file descriptor 
   server_fd = socket(AF_INET, SOCK_STREAM, 0);
   fcntl(server_fd, F_SETFL, O_NONBLOCK);
   if (server_fd==0)
      exit(1);
   // Forcefully attaching socket to the port 5555
   int opt = 1;
   if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                               &opt, sizeof(opt))) 
   { 
      //perror("setsockopt"); 
      exit(1); 
   }

   RunStatus=Unknown;
   RUNNO=-1;
   RUN_START_T=0;
   RUN_STOP_T=0;
   fMfe->fOdbRoot->RI("Runinfo/Run number", &RUNNO);
   fMfe->fOdbRoot->RU32("Runinfo/Start Time binary", &RUN_START_T);
   fMfe->fOdbRoot->RU32("Runinfo/Stop Time binary", &RUN_STOP_T);

   //int period=1000;
   //fEq->fOdbEqCommon->RI("Period",&period);
   //fEq->fCommon->Period=period;
}
void feGEMWorker::Init(MVOdb* supervisor_settings_path)
{
   fOdbSupervisorSettings=supervisor_settings_path;
   fMfe->Msg(MINFO,fEq->fName.c_str(),"Initialising %s...",fEq->fName.c_str());

   //Set the path of feGEM as the default path for settings database
   const int bufsize=200;
   char buf[bufsize]={0};
   readlink("/proc/self/exe",buf,bufsize);

   //Remove everything after last forwardslash (ie /feGEM.exe)
   for (int i=bufsize; i>0; i--)
   {
      if (buf[i]=='/')
      {
         buf[i]='\0';
         break;
      }
   }

   std::string SettingsFileDatabasePath=buf;
   SettingsFileDatabasePath+="/SettingsDatabase";
   fOdbSupervisorSettings->RS("settings_cache_path", &SettingsFileDatabasePath,true);
   std::cout<<"Save"<<SettingsFileDatabasePath<<std::endl;
   SettingsDataBase=new SettingsFileDatabase(SettingsFileDatabasePath.c_str());

   fEq->fOdbEqSettings->RI("event_size", &fEventSize, true);

   //Get (and set defaults if needed) the elog settings and limits
   fEq->fOdbEqSettings->RI("elog_post_size_limit",&fMaxElogPostSize, true);
   fOdbSupervisorSettings->RS("elog_hostname",&elogHost, true);

   SetFEStatus();

   lastEventSize=fEventSize;
   fEq->fOdbEqSettings->WS("feVariables","No variables logged yet...",32);
   fEq->fOdbEqSettings->WU32("DateAdded",0);
   assert(fPort>0);
   if (fEventBuf) {
      free(fEventBuf);
   }
   fEventBuf = (char*)malloc(fEventSize);
   char bind_port[100];
   sprintf(bind_port,"tcp://*:%d",fPort);
   std::cout<<"Binding to2: "<<bind_port<<std::endl;
   address.sin_family = AF_INET; 
   address.sin_addr.s_addr = INADDR_ANY; 
   address.sin_port = htons( fPort ); 
    // Forcefully attaching socket to the port fPort
   if (bind(server_fd, (struct sockaddr *)&address,  
                              sizeof(address))<0) 
   { 
      std::cout<<"Failed to bind"<<std::endl; 
      exit(1); 
   }
   std::cout<<"Bound"<<std::endl;
   //assert (rc==0);
   TCP_thread=std::thread(&feGEMClass::Run,this);
}
