//
// feGEM.cxx
//
// Frontend for two way communication to labVIEW (and or python)
// JTK McKENNA
//
#include "feGEMSupervisor.h"

//--------------------------------------------------
// Base class for LabVIEW frontend
// Child classes:
//    feGEM supervisor (recieves new connections and refers a host to a worker)
//    feGEM worker (one worker per host)
//--------------------------------------------------

feGEMSupervisor::feGEMSupervisor(TMFE* mfe, TMFeEquipment* eq): feGEMClass(mfe,eq,new AllowedHosts(mfe),SUPERVISOR) // ctor
{
   fMfe = mfe;
   fEq  = eq;
   //Default event size ok 10kb, will be overwritten by ODB entry in Init()
   fEventSize = 10000;
   fEventBuf  = NULL;

   HistoryVariable::gHistoryPeriod = 10;
   //So far... limit to 1000 frontend workers...
   fPortRangeStart = 13000;
   fPortRangeStop  = 13999;
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
   fPort=12345;
}

void feGEMSupervisor::Init()
{
   fEq->fOdbEqSettings->RI("event_size", &fEventSize, true);
   fEq->fOdbEqSettings->RI("port_range_start",&fPortRangeStart, true);
   fEq->fOdbEqSettings->RI("port_range_stop",&fPortRangeStop, true);
   fEq->fOdbEqSettings->RI("DefaultHistoryPeriod",&HistoryVariable::gHistoryPeriod,true);
   assert(fPort>0);
   fOdbWorkers=fEq->fOdbEqSettings->Chdir("WorkerList",true);
   
   //Load list of hosts that have logged to MIDAS at some point
   std::vector<std::string> hostlist={"local_host"};
   fOdbWorkers->RSA("HostName", &hostlist,true,0,64);
   //Loop over all hosts and 'clear' the frontend status
   for (const std::string& host: hostlist)
   {
      std::string feName = BuildFrontendName(host.c_str());
      std::string status = "feGEM thread not running";
      std::string odb_path="Equipment/" + feName + "/Common/Status";
      fMfe->fOdbRoot->WS(odb_path.c_str(),status.c_str());
      odb_path += " color";
      fMfe->fOdbRoot->WS(odb_path.c_str(),"mgray");
   }
   
   //fOdbWorkers->WS("HostName","",32);
   std::vector<uint32_t> DataAdded;
   fOdbWorkers->RU32A("DateAdded", &DataAdded, true, 1);
   //fOdbWorkers->RU16A("Port", NULL, true, 1);
   //fOdbWorkers->WU32("DateAdded",0);
   //fOdbWorkers->WU32("Port",0);
   if (fEventBuf) {
      free(fEventBuf);
   }
   fEventBuf = (char*)malloc(fEventSize);
   char bind_port[100];
   sprintf(bind_port,"tcp://*:%d",fPort);
   std::cout<<"Binding to: "<<bind_port<<std::endl;
   //int rc=zmq_bind (responder, bind_port);
   address.sin_family = AF_INET; 
   address.sin_addr.s_addr = INADDR_ANY; 
   address.sin_port = htons( fPort ); 
    // Forcefully attaching socket to the port 5555
   if (bind(server_fd, (struct sockaddr *)&address,  
                              sizeof(address))<0) 
   { 
      std::cout<<"Bind failed"<<std::endl;
      exit(1); 
   }  
   //assert (rc==0);
   TCP_thread=std::thread(&feGEMClass::Run,this);
}

int feGEMSupervisor::FindHostInWorkerList(const char* hostname)
{
   std::vector<std::string> hostlist={"local_host"};
   fOdbWorkers->RSA("HostName", &hostlist,true,0,64);
   int size=hostlist.size();
   for (int i=0; i<size; i++)
   {
      std::cout<<i<<":"<<hostlist.at(i).c_str()<<std::endl;
      if (strcmp(hostlist.at(i).c_str(),hostname)==0)
      {
         std::cout<<"Match found!"<<std::endl;
         return i;
      }
   }
   fOdbWorkers->WSAI("HostName",size,hostname);
   fOdbWorkers->WU32AI("DateAdded",size,(uint32_t)std::time(0));
   std::cout<<"No Match... return size:"<<size<<std::endl;
   return size;
}

uint16_t feGEMSupervisor::AssignPortForWorker(uint workerID)
{
   std::vector<uint16_t> list;
   std::cout<<"WorkerID:"<<workerID<<std::endl;
   fOdbWorkers->RU16A("Port", &list,true,0);
   if (workerID>=list.size())
   {
      int port=fPort+workerID+1;
      fOdbWorkers->WU16AI("Port",workerID,port);
      return port;
   }
   else
   {
      return list.at(workerID);
   }
}

std::string feGEMSupervisor::BuildFrontendName(const char* hostname)
{
   std::string name = "feGEM_";
   name+=hostname;
   if (name.size()>(31 - 4))
      {
         fMfe->Msg(MERROR, name.c_str(), "Frontend name [%s] too long. Perhaps shorten hostname", name.c_str());
         std::string tmp=name;
         name.clear();
         for (int i=0; i<(31 - 4); i++)
         {
            name+=tmp[i];
         }
         fMfe->Msg(MERROR, name.c_str(), "Frontend name [%s] too long. Shortenening hostname to [%s]", tmp.c_str(), name.c_str());
         //exit(1);
      }
   return name;
}
const char* feGEMSupervisor::AddNewClient(const char* hostname)
{
   std::cout<<"Adding host:"<<hostname<<std::endl;
   std::cout<<"Check list of workers"<<std::endl;
   int WorkerNo=FindHostInWorkerList(hostname);
   int port=AssignPortForWorker(WorkerNo);
   std::cout<<"Assign port "<<port<< " for worker "<<WorkerNo<<std::endl;
   if (!WorkerIsRunning(WorkerNo))
   {
      WorkerStarted(WorkerNo);
      //allowed_hosts->AddHost(hostname);
      std::string name = BuildFrontendName(hostname);
      TMFE* mfe=fMfe;
      
      TMFeCommon *common = new TMFeCommon();
      common->EventID = 1;
      common->LogHistory = 1;
      TMFeEquipment* worker_eq = new TMFeEquipment(mfe, name.c_str(), common);
      worker_eq->Init();
      //worker_eq->SetStatus("Starting...", "white");
      worker_eq->ZeroStatistics();
      worker_eq->WriteStatistics();
      mfe->RegisterEquipment(worker_eq);
      feGEMWorker* workerfe = new feGEMWorker(mfe,worker_eq,allowed_hosts,hostname);
      workerfe->fPort=port;
      mfe->RegisterRpcHandler(workerfe);
      workerfe->Init(fEq->fOdbEqSettings);
      mfe->RegisterPeriodicHandler(worker_eq, workerfe);
      //mfe->StartRpcThread();
      //mfe->StartPeriodicThread();
      //mfe->StartPeriodicThreads();
      SetFEStatus();
      return "New Frontend started";
   }
   return "Frontend already running";
}
