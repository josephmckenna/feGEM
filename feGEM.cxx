//
// feGEM.cxx
//
// Frontend for two way communication to labVIEW (and or python)
// JTK McKENNA
//
#include "feGEM.h"

//--------------------------------------------------
// Base class for LabVIEW frontend
// Child classes:
//    feGEM supervisor (recieves new connections and refers a host to a worker)
//    feGEM worker (one worker per host)
//--------------------------------------------------


std::string feGEMClass::HandleRpc(const char* cmd, const char* args)
{
   fMfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);
   return "OK";
}

void feGEMClass::HandleBeginRun()
{
   fMfe->Msg(MINFO, "HandleBeginRun", "Begin run!");
   fMfe->fOdbRoot->RI("Runinfo/Run number", &RUNNO);
   fMfe->fOdbRoot->RU32("Runinfo/Start Time binary", &RUN_START_T);
   //Stop time gets reset to 0 (1/1/1970) at start run
   //fMfe->fOdbRoot->RU32("Runinfo/Stop Time binary", &RUN_STOP_T);
   //fEq->SetStatus("Running", "#00FF00");
   RunStatus=Running;
}

void feGEMClass::HandleEndRun()
{
   fMfe->Msg(MINFO, "HandleEndRun", "End run!");
   fMfe->fOdbRoot->RI("Runinfo/Run number", &RUNNO);
   fMfe->fOdbRoot->RU32("Runinfo/Start Time binary", &RUN_START_T);
   fMfe->fOdbRoot->RU32("Runinfo/Stop Time binary", &RUN_STOP_T);
   //fEq->SetStatus("Stopped", "#00FF00");
   RunStatus=Stopped;
}
void feGEMClass::SetFEStatus()
{
   std::string status = "Rate limit set: " + std::to_string(fEventSize/1000) + "KiB/s";
   fEq->SetStatus(status.c_str(), "greenLight");
}

void feGEMClass::HandleCommandBank(const GEMDATA<char>* bank,const char* command, const char* hostname)
{
   
   //Add 'Commands' that can be sent to feGEM, they either change something or get a response back.


   //Banks associated with a new commection (1/4)
   if (strncmp(command,"START_FRONTEND",14)==0)
   {
      assert(feGEMClassType==SUPERVISOR);
      message.QueueData("FrontendStatus",AddNewClient(&bank->DATA[0]));
      return;
   }
      //Banks associated with a new commection (2/4)
   else if (strncmp(command,"ALLOW_HOST",14)==0)
   {
      if (!allowed_hosts->SelfRegistrationIsAllowed())
      {
         fMfe->Msg(MINFO, "feGEM", "LabVIEW host name %s tried to register its self on allowed host list, but self registration is disabled",hostname);
         return;
      }
      if (strcmp(bank->DATA,hostname)!=0)
      {
         fMfe->Msg(MTALK, "feGEM", "LabVIEW host name %s does not match DNS lookup %s",bank->DATA,hostname);
         allowed_hosts->AddHost(hostname);
      }
      allowed_hosts->AddHost(bank->DATA);
      return;
   }
   //Banks associated with a new commection (3/4)
   //Send the address for data to be logged to, assumes client uses the DNS
   else if (strncmp(command,"GIVE_ME_ADDRESS",15)==0)
   {
      assert(feGEMClassType==SUPERVISOR);
      message.QueueData("SendToAddress",thisHostname.c_str());
      return;
   }
   //Banks associated with a new commection (4/4)
   else if (strncmp(command,"GIVE_ME_PORT",14)==0)
   {
      assert(feGEMClassType==SUPERVISOR);
      int WorkerNo=FindHostInWorkerList(bank->DATA);
      int port=AssignPortForWorker(WorkerNo);
      char log_to_port[80];
      sprintf(log_to_port,"%u",port);
      //std::cout<<"SEND TO PORT:"<<port<<std::endl;
      std::cout<<log_to_port<<std::endl;
      message.QueueData("SendToPort",log_to_port);
      return;
   }

   else if (strncmp(command,"ALLOW_SSH_TUNNEL",16)==0) //VARNAME is only 15 char long
   {
      if (!allowed_hosts->SelfRegistrationIsAllowed())
      {
         fMfe->Msg(MINFO, "feGEM", "LabVIEW host name %s tried to register its self on allowed host list, but self registration is disabled",hostname);
         return;
      }
      // We do not know the name of the host yet... they will reconnect on a new port...
      // and, for example, lxplus might have a new alias)
      allowed_hosts->AddHost(hostname);
      return;
   }
   //Note all devices have their time correctly set (Many CRIO's)
   //Note: Some devices don't have their own clocks... (arduino)... so lets support them too!
   else if (strncmp(command,"CHECK_TIME_SYNC",15)==0)
   {
      using namespace std::chrono;
      milliseconds ms = duration_cast< milliseconds >(
         system_clock::now().time_since_epoch()
      );
      char buf[80];
      sprintf(buf,"%f",(double)ms.count()/1000.+2082844800.);
      std::cout<<"Time stamp for comparison:"<<buf<<std::endl;
      message.QueueData("LV_TIME_NOW",buf);
      return;
   }
   //Enable verbose JSON replies. DebugMode is tracked with an int for future flexibility
   else if (strncmp(command,"ENABLE_DEBUG_MODE",17)==0)
   {
      assert(feGEMClassType==WORKER);
      fDebugMode = 1;
      return;
   }
   else if (strncmp(command,"DISABLE_DEBUG_MODE",18)==0)
   {
      assert(feGEMClassType==WORKER);
      fDebugMode = 0;
      return;
   }
   //Commonly used command, every connection is going to ask for the MIDAS every buffer size
   else if (strncmp(command,"GET_EVENT_SIZE",14)==0)
   {
      char buf[80]={0};
      //Format event size to string for JSON
      sprintf(buf,"%d",fEventSize);
      message.QueueData("EventSize",buf);
      return;
   }
   else if (strncmp(command,"GET_RUNNO",9)==0)
   {
      char buf[20]={0};
      //Format RUNNO to string for JSON
      sprintf(buf,"%d",RUNNO);
      message.QueueData("RunNumber",buf);
      return;
   }
   else if (strncmp(command,"GET_RUN_START_T",15)==0)
   {
      char buf[20]={0};
      sprintf(buf,"%u",RUN_START_T);
      message.QueueData("RunStartTime",buf);
      return;
   }
   else if (strncmp(command,"GET_RUN_STOP_T",14)==0)
   {
      char buf[20]={0};
      sprintf(buf,"%u",RUN_STOP_T);
      message.QueueData("RunStopTime",buf);
      return;
   }
   else if (strncmp(command,"GET_STATUS",10)==0)
   {
      switch (RunStatus)
      {
         case Unknown:
            message.QueueData("RunStatus","UNKNOWN");
            break;
         case Running:
            message.QueueData("RunStatus","RUNNING");
            break;
         case Stopped:
            message.QueueData("RunStatus","STOPPED");
            break;
      }
      return;
   }
   else if (strncmp(command,"SET_EVENT_SIZE",14)==0)
   {
      assert(feGEMClassType==WORKER);
      std::cout<<"Updating event size:"<<(char*)&bank->DATA<<std::endl;
      int new_size=atoi((char*)&bank->DATA);
      if (new_size>fEventSize)
         fEventSize=new_size;
      if (fEventSize<10000)
      {
         fMfe->Msg(MTALK, "feGEM", "Minimum event size can not be less that 10 kilo bytes");
         fEventSize=10000;
      }
      fEq->fOdbEqSettings->WI("event_size", fEventSize);
      std::cout<<"Event size updated to:"<<fEventSize<<std::endl;
      SetFEStatus();
      return;
   }

   else
   {
      std::cout<<"Command not understood!"<<std::endl;
      bank->print(strlen((char*)&bank->DATA),2,0,true);
   }
   return;
}

void feGEMClass::HandleStrArrayBank(GEMBANK<char>* bank,const char* hostname)
{
   if (strncmp(bank->NAME.VARCATEGORY,"CLIENT_INFO",14)==0)
   {
      std::vector<std::string> array;
      
      bool last_char_was_null = true;
      uint32_t entries = bank->BlockSize - bank->DATA[0].GetHeaderSize();
      
      for (uint32_t i=0; i<entries; i++)
      {
         if (last_char_was_null && bank->DATA[0].DATA[i])
         {
            array.push_back(&(bank->DATA[0].DATA[i]));
            last_char_was_null = false;
         }
         if (!bank->DATA[0].DATA[i])
         {
            last_char_was_null = true;
         }
      }
      

      
      std::cout<<"Writing \"";
      size_t max_len=0;
      for (std::string line: array)
      {
         std::cout<<line.c_str()<<std::endl;
         if (line.size() > max_len )
            max_len=line.size();
       }
      std::cout<<"\" into "<<bank->NAME.VARNAME<< " in equipment settings"<<std::endl;
      fEq->fOdbEqSettings->WSA(bank->NAME.VARNAME, array,max_len+1);
      return;
   }
   else
   {
      std::cout<<"String array not understood!"<<std::endl;
      bank->print();
   }
}

void feGEMClass::HandleStrBank(GEMBANK<char>* bank,const char* hostname)
{
   //bank->print();

   if (strncmp(bank->NAME.VARNAME,"COMMAND",7)==0)
   {
      const char* command=bank->NAME.EquipmentType;
      for (uint32_t i=0; i<bank->NumberOfEntries; i++)
      {
         HandleCommandBank(bank->GetDataEntry(i),command,hostname);
      }
      return;
   }
   else if (strncmp(bank->NAME.VARNAME,"TALK",4)==0)
   {
      bank->print();
      fMfe->Msg(MTALK, fEq->fName.c_str(), (char*)bank->DATA->DATA);
      //if (strncmp(bank->NAME.VARCATEGORY,"THISHOST",8)==0)
      //{
      periodicity.ProcessMessage(bank);
         //}
      return;
   }
   else if (strncmp(bank->NAME.VARCATEGORY,"CLIENT_INFO",14)==0)
   {
      std::cout<<"Writing \""<<bank->DATA[0].DATA<<"\" into "<<bank->NAME.VARNAME<< " in equipment settings"<<std::endl;
      fEq->fOdbEqSettings->WS(bank->NAME.VARNAME, bank->DATA[0].DATA);
      return;
   }
   else if (strncmp(bank->NAME.VARCATEGORY,"SETTINGS_FILE",13)==0)
   {
      //The settings files doesn't need to go to midas... skip logger.Update
      if (strncmp(bank->NAME.VARNAME,"SAVE",4)==0)
         return SettingsDataBase->SaveSettingsFile(bank,&message);
      if (strncmp(bank->NAME.VARNAME,"LOAD",4)==0)
      {
         int offset=0;
         //Use offset: LOAD-2 means LOAD two revisions ago
         if (bank->NAME.VARNAME[4])
            offset=atoi(&bank->NAME.VARNAME[4]);
         return SettingsDataBase->LoadSettingsFile(bank,&message,offset);
      }
      if (strncmp(bank->NAME.VARNAME,"LIST",4)==0)
      {
         assert("IMPLEMENT ME");
         return SettingsDataBase->ListSettingsFile(bank,&message);
      }
   }
  
   
   //Put every UTF-8 character into a string and send it as JSON
   else
   {
      std::cout<<"String not understood!"<<std::endl;
      bank->print();
   }
   logger.Update(bank);
}

void feGEMClass::LogBank(const char* buf, const char* hostname)
{
   GEMBANK<void*>* ThisBank=(GEMBANK<void*>*)buf;
   if (fDebugMode)
   {
      char buf[200];
      sprintf(buf,
              "Received: %.16s/%.16s - %dx ( %d x %.4s ) [%d bytes]",
              ThisBank->NAME.VARCATEGORY,
              ThisBank->NAME.VARNAME,
              ThisBank->NumberOfEntries,
              ThisBank->GetFirstDataEntry()->GetEntries(ThisBank->BlockSize),
              ThisBank->NAME.DATATYPE,
              ThisBank->GetTotalSize()
              );
      message.QueueMessage(buf);
   }
   //std::cout<<ThisBank->NAME.VARNAME<<std::endl;
   if (strncmp(ThisBank->NAME.DATATYPE,"DBL",3)==0) {
      GEMBANK<double>* bank=(GEMBANK<double>*)buf;
      //bank->print();
      logger.Update(bank);
   } else if (strncmp(ThisBank->NAME.DATATYPE,"FLT",3)==0) {
      GEMBANK<float>* bank=(GEMBANK<float>*)buf;
      logger.Update(bank);
   } else if (strncmp(ThisBank->NAME.DATATYPE,"BOOL",4)==0) {
      GEMBANK<bool>* bank=(GEMBANK<bool>*)buf;
      logger.Update(bank);
   //Not supported by ODB
   /*} else if (strncmp(ThisBank->NAME.DATATYPE,"I64",3)==0) {
      GEMBANK<int64_t>* bank=(GEMBANK<int64_t>*)buf;
      logger.Update(bank);
   } else if (strncmp(ThisBank->NAME.DATATYPE,"U64",3)==0) {
      GEMBANK<uint64_t>* bank=(GEMBANK<uint64_t>*)buf;
      logger.Update(bank);*/
   } else if (strncmp(ThisBank->NAME.DATATYPE,"I32",3)==0) {
      GEMBANK<int32_t>* bank=(GEMBANK<int32_t>*)buf;
      logger.Update(bank);
   } else if (strncmp(ThisBank->NAME.DATATYPE,"U32",4)==0) {
      GEMBANK<uint32_t>* bank=(GEMBANK<uint32_t>*)buf;
      //bank->print();
      logger.Update(bank);
   //Not supported by ODB
   /*} else if (strncmp(ThisBank->NAME.DATATYPE,"I16",3)==0) {
      GEMBANK<int16_t>* bank=(GEMBANK<int16_t>*)buf;
      logger.Update(bank);*/
   } else if (strncmp(ThisBank->NAME.DATATYPE,"U16",3)==0) {
      GEMBANK<uint16_t>* bank=(GEMBANK<uint16_t>*)buf;
      logger.Update(bank);
   /*} else if (strncmp(ThisBank->NAME.DATATYPE,"I8",2)==0) {
      GEMBANK<int8_t>* bank=(GEMBANK<int8_t>*)buf;
      logger.Update(bank);
   } else if (strncmp(ThisBank->NAME.DATATYPE,"U8",2)==0) {
      GEMBANK<uint8_t>* bank=(GEMBANK<uint8_t>*)buf;
      logger.Update(bank);*/
   } else if (strncmp(ThisBank->NAME.DATATYPE,"STRA",4)==0) {
      GEMBANK<char>* bank=(GEMBANK<char>*)buf;
      HandleStrArrayBank(bank,hostname);
      return;
   } else if (strncmp(ThisBank->NAME.DATATYPE,"STR",3)==0) {
      GEMBANK<char>* bank=(GEMBANK<char>*)buf;
      HandleStrBank(bank,hostname);
      return;
   } else {
      std::cout<<"Unknown bank data type... "<<std::endl;
      ThisBank->print();
      exit(1);
   }
   return;
}

int feGEMClass::HandleBankArray(const char * ptr,const char* hostname)
{
   int NGEMBanks = 0;
   GEMBANKARRAY* array=(GEMBANKARRAY*)ptr;
   if (array->GetTotalSize() > (uint32_t)fEventSize)
   {
      char error[100];
      sprintf(error,"ERROR: [%s] More bytes sent (%u) than MIDAS has assiged for buffer (%u)",
                     fEq->fName.c_str(),
                     array->BlockSize + array->GetHeaderSize(),
                     fEventSize);
      message.QueueError(fEq->fName.c_str(),error);
      return -1;
   }
   //array->print();
   char *buf=(char*)&array->DATA[0];
   for (uint32_t i=0; i<array->NumberOfEntries; i++)
   {
      GEMBANK<double>* bank=(GEMBANK<double>*)buf;
      NGEMBanks += HandleBank(buf, hostname);
      buf+=bank->GetHeaderSize()+bank->BlockSize*bank->NumberOfEntries;
   }
   return NGEMBanks;
}

int feGEMClass::HandleBank(const char * ptr,const char* hostname)
{
   //Use invalid data type to probe the header
   GEMBANK<void*>* ThisBank=(GEMBANK<void*>*)ptr;
   //ThisBank->print();
   if (ThisBank->BlockSize+ThisBank->GetHeaderSize() > (uint32_t)fEventSize)
   {
      char error[100];
      sprintf(error,"ERROR: [%s] More bytes sent (%u) than MIDAS has assiged for buffer (%d)",
                     fEq->fName.c_str(),
                     ThisBank->GetTotalSize(),
                     fEventSize);
      message.QueueError(fEq->fName.c_str(),error);
      return -1;
   }
   LogBank(ptr,hostname);
   return ThisBank->NumberOfEntries;
}
void feGEMClass::Run()
{
   //std::cout<<"Run..."<<std::endl;
   while (!fMfe->fShutdownRequested)
   {
      ServeHost();
      //std::cout<<"Sleeping: "<<periodicity.GetWaitPeriod()<<"ms"<<std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(periodicity.GetWaitPeriod()));
   }
}
void feGEMClass::ServeHost()
{
   //
   //if (fPort!=5555)
   //printf("Thread %s, periodic!\n", TMFE::GetThreadId().c_str());
   //std::cout<<"periodic (port:"<<fPort<<")"<<std::endl;
   std::chrono::time_point<std::chrono::system_clock> timer_start=std::chrono::high_resolution_clock::now();
   //Check if we need to change the MIDAS event buffer size
   if (lastEventSize!=fEventSize)
   {
      std::cout<<"fEventSize updated! Flushing buffer"<<std::endl;
         if (fEventBuf) {
            free(fEventBuf);
         }
         fEventBuf = (char*)malloc(fEventSize);
         std::cout<<"Event buffer re-initialised "<<std::endl;
   }
   lastEventSize=fEventSize;
   //Listen for TCP connections
   if (listen(server_fd, 3) < 0) 
   { 
      perror("listen"); 
      exit(EXIT_FAILURE); 
   } 
   
   int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
   if (new_socket<0) 
   { 
      periodicity.LogPeriodicWithoutData();
      //perror("accept"); 
      //exit(EXIT_FAILURE); 
      return;
   }
   //Security: Check if host in in allowed host lists
   char hostname[200];
   char servname[200];
   int name_status=getnameinfo((struct sockaddr *)&address, addrlen,
                    hostname, 200,servname,200,0);
   //Returned 0 on success
   if (name_status)
      return;
   bool allowed=false;
   if (feGEMClassType==WORKER)
   {
      //Only white listed hosts allow on worker
      allowed=allowed_hosts->IsAllowed(hostname);
      //allowed=allowed_hosts->IsWhiteListed(hostname);
   }
   else if (feGEMClassType==SUPERVISOR)
   {
      //Allow grey listed hosts on supervisor
      allowed=allowed_hosts->IsAllowed(hostname);
   }
   if (!allowed)
   {
      allowed_hosts->PrintRejection(fMfe,hostname);
      close(new_socket);
      return;
   }
   //Make sure header memory is clean
   ((GEMBANKARRAY*)fEventBuf)->ClearHeader();
   ((GEMBANK<void*>*)fEventBuf)->ClearHeader();
   bool legal_message=false;
   //Prepare the MIDAS bank so that we can directly write into from the TCP buffer
   fEq->ComposeEvent(fEventBuf, fEventSize);
   fEq->BkInit(fEventBuf, fEventSize);
   // We place the data inside the data bank (aimed minimise move/copy operations for speed)
   // therefor we must define the MIDAS Bankname now, not after we know if its a Generic 
   // Equipment Manager bank or Generic Equipment Manager Array GEB1 or GEA1
   char* ptr = (char*) fEq->BkOpen(fEventBuf, "GEM1", TID_STRUCT);
   int read_status=0;
   int position=0;
   int BankSize=-1;
   // Get the first chunk of the message (must be atleast the header of the data coming)
   // The header of a GEMBANK is 88 bytes
   // The header of a GEMBANKARRAY is 32 bytes
   // So... the minimum data we need for GetTotalSize() to work is 88 bytes
   int max_reads=100000;
   //std::vector<std::sting>={"LVA1","LVB1","PYA1","PYA1"};
   while (read_status<88)
   {
      read_status= read( new_socket , ptr+position, fEventSize-position);
      //std::cout<<"read:"<<read_status<<"\t"<<position<<std::endl;
      position+=read_status;
      if (!position) break; //Nothing to read... 
      if (--max_reads == 0)
      {
         char message[100];
         sprintf(message,"TCP Read timeout getting bank header");
         fMfe->Msg(MTALK, "feGEM", message);
         return;
      }
      //sleep(1);
   }
   // No data to read... does quitting cause a memory leak? It seems not (tested with valgrind)
   if (read_status<=0)
   {
      periodicity.LogPeriodicWithoutData();
      return;
   }
   else
   {
      fEq->WriteStatistics();
      periodicity.LogPeriodicWithData();
   }
   
   max_reads=10000;
   
   //We have the header... check for compliant data type and get the total size (BankSize)
   if (strncmp(ptr,"GEA1",4)==0 )
   {
      GEMBANKARRAY* bank=(GEMBANKARRAY*)ptr;
      BankSize=bank->GetTotalSize();
   }
   else if (strncmp(ptr,"GEB1",4)==0 )
   {
      GEMBANK<void*>* bank=(GEMBANK<void*>*)ptr;
      BankSize=bank->GetTotalSize();
   }   //std::cout<<"BankSize:"<<BankSize<<std::endl;
   else
   {
      cm_msg(MTALK, "feGEM", "Host %s is sending malformed data... black listing...", hostname);
      //std::cout<<"Black listing host!"<<std::endl;
      allowed_hosts->BanHost(hostname);
      legal_message=false;
      close(new_socket);
      return;
   }
   
   //The header looks ok, lets get the whole thing GEMBANK / GEMBANKARRAY
   while (position<BankSize)
   {
      read_status= read( new_socket , ptr+position, BankSize-position);
      if (!read_status) sleep(0.1);
      position+=read_status;
      if (--max_reads == 0)
      {
         char message[100];
         sprintf(message,"TCP Read timeout getting GEMBANKARRAY, got %d bytes, expected %d",position,BankSize);
         fMfe->Msg(MTALK, "feGEM", message);
         return;
      } 
   }
   //std::cout<<BankSize<<"=="<<position<<std::endl;
   if (BankSize==position)
      legal_message=true;
   assert(BankSize==position);
   read_status=position;
   int nbanks=0;
   //Process what we have read into the MIDAS bank
   //printf ("[%s] Received %c%c%c%c (%d bytes)",fEq->fName.c_str(),ptr[0],ptr[1],ptr[2],ptr[3],read_status);
   if (strncmp(ptr,"GEA1",4)==0 ) {
      //std::cout<<"["<<fEq->fName.c_str()<<"] Python / LabVIEW Bank Array found!"<<std::endl;
      nbanks=HandleBankArray(ptr,hostname); //Iterates over array with HandleBank()
   } else if (strncmp(ptr,"GEB1",4)==0 ) {
      //std::cout<<"["<<fEq->fName.c_str()<<"] Python / LabVIEW Bank found!"<<std::endl;
      nbanks=HandleBank(ptr,hostname);
   } else {
      std::cout<<"["<<fEq->fName.c_str()<<"] Unknown data type just received... "<<std::endl;
      message.QueueError(fEq->fName.c_str(),"Unknown data type just received... ");
      for (int i=0; i<20; i++)
         std::cout<<ptr[i];
      exit(1);
   }
   if (!legal_message)
   {
      close(new_socket);
      return;
   }
   fEq->BkClose(fEventBuf, ptr+BankSize);
   fEq->SendEvent(fEventBuf);
   std::chrono::time_point<std::chrono::system_clock> timer_stop=std::chrono::high_resolution_clock::now();
   std::chrono::duration<double, std::milli> handlingtime=timer_stop - timer_start;
   //std::cout<<"["<<fEq->fName.c_str()<<"] Handling time: "<<handlingtime.count()*1000 <<"ms"<<std::endl;
   printf ("[%s] Handled %c%c%c%c %d banks (%d bytes) in %fms",fEq->fName.c_str(),ptr[0],ptr[1],ptr[2],ptr[3],nbanks,read_status,handlingtime.count());
   periodicity.AddBanksProcessed(nbanks);
   if (fDebugMode)
      printf(" (debug mode on)");
   printf("\n");
   char buf[100];
   sprintf(buf,"DATA OK");
   message.QueueMessage(buf);
   bool KillFrontend=message.HaveErrors();
   std::vector<char> reply=message.ReadMessageQueue(handlingtime.count());
   //for (char c: reply)
   //   std::cout<<c;
   std::cout<<"Sending "<<reply.size()<<" bytes"<<std::endl;
   //send(new_socket, reply.c_str(), reply.size(), 0 );
   send(new_socket, (char*)&(reply[0]), reply.size(), 0 );
   
   read( new_socket, NULL,0);
   close(new_socket);
   
   shutdown(new_socket,SHUT_RD);
   
   //zmq_send (responder, reply.c_str(), reply.size(), 0);
   if (KillFrontend)
      exit(1);
   return;
}


feGEMWorker::feGEMWorker(TMFE* mfe, TMFeEquipment* eq, AllowedHosts* hosts, int debugMode): feGEMClass(mfe,eq,hosts,WORKER,debugMode)
{
   fMfe = mfe;
   fEq  = eq;
   
   //Default event size ok 10kb, will be overwritten by ODB entry in Init()
   fEventSize = 10000;
   fEventBuf  = NULL;
   SetFEStatus();

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
      std::string name = "feGEM_";
      name+=hostname;
      TMFE* mfe=fMfe;
      if (name.size()>(31 - 4))
      {
         mfe->Msg(MERROR, name.c_str(), "Frontend name [%s] too long. Perhaps shorten hostname", name.c_str());
         std::string tmp=name;
         name.clear();
         for (int i=0; i<(31 - 4); i++)
         {
            name+=tmp[i];
         }
         mfe->Msg(MERROR, name.c_str(), "Frontend name [%s] too long. Shortenening hostname to [%s]", tmp.c_str(), name.c_str());
         //exit(1);
      }
      TMFeCommon *common = new TMFeCommon();
      common->EventID = 1;
      common->LogHistory = 1;
      TMFeEquipment* worker_eq = new TMFeEquipment(mfe, name.c_str(), common);
      worker_eq->Init();
      //worker_eq->SetStatus("Starting...", "white");
      worker_eq->ZeroStatistics();
      worker_eq->WriteStatistics();
      mfe->RegisterEquipment(worker_eq);
      feGEMWorker* workerfe = new feGEMWorker(mfe,worker_eq,allowed_hosts);
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

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
