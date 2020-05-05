//
// feLabVIEW.cxx
//
// Frontend for two way communication to labVIEW
// JTK McKENNA
//

#include <stdio.h>
#include <signal.h> // SIGPIPE
#include <assert.h> // assert()
#include <stdlib.h> // malloc()
#include <cstring>
#include <iostream>
#include "midas.h"
#include "tmfe.h"


#include "feLabVIEW.h"

#include "msystem.h"
enum RunStatusType{Unknown,Running,Stopped};
class MessageHandler
{
   private:
      TMFE* fMfe;
      std::vector<std::string> MessageForLabVIEWQueue;
      std::vector<std::string> ErrorForLabVIEWQueue;
      int TotalText;
   public:
   MessageHandler(TMFE* mfe)
   {
      fMfe=mfe;
      TotalText=0;
   }
   ~MessageHandler()
   {
      if (MessageForLabVIEWQueue.size())
      {
         std::cout<<"WARNING: Messages not flushed:"<<std::endl;
         for (auto msg: MessageForLabVIEWQueue)
            std::cout<< msg<<std::endl;
      }
      if (ErrorForLabVIEWQueue.size())
      {
         std::cout<<"ERROR: Errors not flushed:"<<std::endl;
         for (auto err: ErrorForLabVIEWQueue)
            std::cout<<err<<std::endl;
      }  
   }
   bool HaveErrors()
   {
      return ErrorForLabVIEWQueue.size();
   }
   void QueueData(const char* msg)
   {
      int len=strlen(msg);
      MessageForLabVIEWQueue.push_back(std::string("\"")+msg+std::string("\""));
      TotalText+=len+2;
   }
   void QueueMessage(const char* msg)
   {
      int len=strlen(msg);
      //Quote marks in messages must have escape characters! (JSON requirement)
      for (int i=1; i<len; i++)
         if (msg[i]=='\"')
            assert(msg[i-1]=='\\');
      MessageForLabVIEWQueue.push_back(std::string("\"msg:") + msg + std::string("\""));
      TotalText+=len+6;
   }
   void QueueError(const char* err)
   {
      int len=strlen(err);
      //Quote marks in errors must have escape characters! (JSON requirement)
      for (int i=1; i<len; i++)
         if (err[i]=='"')
            assert(err[i-1]=='\\');
      fMfe->Msg(MTALK, "feLabVIEW", err);
      ErrorForLabVIEWQueue.push_back(std::string("\"err:") + err + std::string("\""));
      TotalText+=len+6;
   }
   std::string ReadMessageQueue()
   {
      //Build basic JSON string ["msg:This is a message to LabVIEW","err:This Is An Error Message"]
      std::string msg;
      msg.reserve(TotalText+MessageForLabVIEWQueue.size()+ErrorForLabVIEWQueue.size()+1);
      msg+="[";
      int i=0;
      for (auto Message: MessageForLabVIEWQueue)
      {
         if (i++>0)
            msg+=",";
         msg+=Message;
      }
      MessageForLabVIEWQueue.clear();
      for (auto Error: ErrorForLabVIEWQueue)
      {
         if (i++>0)
            msg+=",";
         msg+=Error;
      }
      ErrorForLabVIEWQueue.clear();
      msg+="]";
      return msg;
   }
};

#include <chrono>
class HistoryVariable
{
   public:
   std::string fCategory;
   std::string fVarName;
   int64_t fLastUpdate; //Converted to UXIXTime
   int UpdateFrequency;
   MVOdb* fOdbEqVariables; 
   template<typename T>
   HistoryVariable(const LVBANK<T>* lvbank, TMFE* mfe )
   {
      fCategory=lvbank->GetCategoryName();
      fVarName=lvbank->GetVarName();
      UpdateFrequency=lvbank->HistoryRate;
      fLastUpdate=0;

      //Prepare ODB entry for variable
      MVOdb* OdbEq = NULL;
      if (strncmp(lvbank->GetCategoryName().c_str(),"THISHOST",8)==0)
      {
         OdbEq = mfe->fOdbRoot->Chdir((std::string("Equipment/") + mfe->fFrontendName).c_str(), true);
      }
      else
      {
         OdbEq = mfe->fOdbRoot->Chdir((std::string("Equipment/") + fCategory).c_str(), true);
      }
      fOdbEqVariables  = OdbEq->Chdir("Variables", true);
   }
   template<typename T>
   bool IsMatch(const LVBANK<T>* lvbank)
   {
      if (strcmp(fCategory.c_str(),lvbank->GetCategoryName().c_str())!=0)
         return false;
      if (strcmp(fVarName.c_str(),lvbank->GetVarName().c_str())!=0)
         return false;
      return true;
   }
   template<typename T>
   void Update(const LVBANK<T>* lvbank)
   {
      const LVDATA<T>* data=lvbank->GetLastDataEntry();
      //std::cout <<data->GetUnixTimestamp() <<" <  " <<fLastUpdate + UpdateFrequency <<std::endl;
      if (data->GetUnixTimestamp() < fLastUpdate + UpdateFrequency)
         return;
      fLastUpdate=data->GetUnixTimestamp();
      const int data_entries=data->GetEntries(lvbank->BlockSize);
      std::vector<T> array(data_entries);
      for (int i=0; i<data_entries; i++)
         array[i]=data->DATA[i];
      WriteODB(array);
   }
   private:
   void WriteODB(std::vector<bool>& data)
   {
      fOdbEqVariables->WBA(fVarName.c_str(),data);
   }
   void WriteODB(std::vector<int>& data)
   {
      fOdbEqVariables->WIA(fVarName.c_str(),data);
   }
   void WriteODB(std::vector<double>& data)
   {
      fOdbEqVariables->WDA(fVarName.c_str(),data);
   }
   void WriteODB(std::vector<float>& data)
   {
      fOdbEqVariables->WFA(fVarName.c_str(),data);
   }
   void WriteODB(std::vector<char>& data)
   {
      //Note: Char arrays not supported... but std::string is supported...
      const int data_entries=data.size();
      std::vector<std::string> array(data_entries);
      size_t max_size=0;
      for (int i=0; i<data_entries; i++)
      {
         array[i]=data[i];
         if (array[i].size()>max_size)
             max_size=array[i].size();
      }
      fOdbEqVariables->WSA(fVarName.c_str(),array,max_size);
   }
   /*void WriteODB(std::vector<int16_t>& data)
   {
      fOdbEqVariables->WU16A(fVarName.c_str(),data);
   }*/
   /*void WriteODB(std::vector<int32_t>& data)
   {
      fOdbEqVariables->WU32A(fVarName.c_str(),data)
   }*/

};

class HistoryLogger
{
public:
   TMFE* fMfe;
   TMFeEquipment* fEq;
   std::vector<HistoryVariable*> fVariables;
   HistoryLogger(TMFE* mfe,TMFeEquipment* eq)
   {
      fEq=eq;
      fMfe=mfe;
   }
   ~HistoryLogger()
   {
      //I do not own fMfe or fEq
      for (auto* var: fVariables)
         delete var;
      fVariables.clear();
   }
   template<typename T>
   HistoryVariable* AddNewVariable(const LVBANK<T>* lvbank)
   {
      //Assert that category and var name are null terminated
      //assert(lvbank->NAME.VARCATEGORY[15]==0);
      //assert(lvbank->NAME.VARNAME[15]==0);
      
      //Store list of logged variables in Equipment settings
      char VarAndCategory[32];
      sprintf(VarAndCategory,"%s/%s",
                       lvbank->GetCategoryName().c_str(),
                       lvbank->GetVarName().c_str());
      fEq->fOdbEqSettings->WSAI("feVariables",fVariables.size(), VarAndCategory);
      fEq->fOdbEqSettings->WU32AI("DateAdded",(int)fVariables.size(), lvbank->GetFirstUnixTimestamp());
      
      //Push into list of monitored variables
      fVariables.push_back(new HistoryVariable(lvbank,fMfe));

      //Announce in control room new variable is logging
      char message[100];
      sprintf(message,"New variable [%s] in category [%s] being logged",lvbank->GetVarName().c_str(),lvbank->GetCategoryName().c_str());
      fMfe->Msg(MTALK, "feLabVIEW", message);

      //Return pointer to this variable so the history can be updated by caller function
      return fVariables.back();
   }
   template<typename T>
   HistoryVariable* Find(const LVBANK<T>* lvbank, bool AddIfNotFound=true)
   {
      HistoryVariable* FindThis=NULL;
      //Find HistoryVariable that matches 
      for (auto var: fVariables)
      {
         if (var->IsMatch(lvbank))
         {
            FindThis=var;
            break;
         }
      }
      //If no match found... create one
      if (!FindThis && AddIfNotFound)
      {
         FindThis=AddNewVariable(lvbank);
      }
      return FindThis;
   }
   template<typename T>
   void Update(const LVBANK<T>* lvbank)
   {
      HistoryVariable* UpdateThis=Find(lvbank,true);
      UpdateThis->Update(lvbank);
   }
};

class PeriodicityManager
{
   private:
   int fNumberOfConnections;
   std::vector<std::string> RemoteCallers;
   TMFeEquipment* fEq;
   TMFE* fMfe;
   int fPeriodicWithData;
   int fPeriodicWithoutData;

   public:
   PeriodicityManager(TMFE* mfe,TMFeEquipment* eq)
   {
      fEq=eq;
      fMfe=mfe;
      fNumberOfConnections=1;
      fPeriodicWithData=0;
      fPeriodicWithoutData=0;
   }
   const char* ProgramName(char* string)
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
   void AddRemoteCaller(char* prog)
   {
      for (auto item: RemoteCallers)
      {
         if (strcmp(prog,item.c_str())==0)
         {
            std::cout<<"Restarted program detected ("<<ProgramName(prog)<<")! Total:"<<fNumberOfConnections<<std::endl;
            fMfe->Msg(MTALK, "feLabVIEW", "Restart of program %s detected",ProgramName(prog));
            return;
         }
      }
      //Item not already in list
      ++fNumberOfConnections;
      std::cout<<"New connection detected ("<<ProgramName(prog)<<")! Total:"<<fNumberOfConnections<<std::endl;
      RemoteCallers.push_back(prog);
   }
   void LogPeriodicWithData()
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
                     "feLabVIEW", "%s periodic tasks are very busy... miss use of the LabVIEW library?",
                     fMfe->fFrontendName.c_str()
                     );
            fMfe->Msg(MINFO,
                     "feLabVIEW",
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
   void LogPeriodicWithoutData()
   {
      fPeriodicWithoutData++;
   }
   void UpdatePerodicity()
   {
      std::cout<<fEq->fCommon->Period <<" > "<< 1000./ (double)fNumberOfConnections<<std::endl;
      if (fEq->fCommon->Period > 1000./ (double)fNumberOfConnections)
      {
         fEq->fCommon->Period = 1000./ (double)fNumberOfConnections;
         std::cout<<"Periodicity increase to "<<fEq->fCommon->Period<<"ms"<<std::endl;
      }
      fPeriodicWithData=0;
      fPeriodicWithoutData=0;
   }
   void ProcessMessage(LVBANK<char>* bank)
   {
      //std::cout<<(char*)&(bank->DATA[0].DATA)<<std::endl;
      if ((strncmp((char*)&(bank->DATA->DATA),"New labview connection from",27)==0) ||
          (strncmp((char*)&(bank->DATA->DATA),"New python connection from",26)==0))
      {
         char ProgramName[100];
         snprintf(ProgramName,bank->BlockSize-16,"%s",(char*)&(bank->DATA[0].DATA));
         AddRemoteCaller(ProgramName);
         UpdatePerodicity();
      }
      return;
   }
};

class feLabVIEWWorker :
   public TMFeRpcHandlerInterface,
   public  TMFePeriodicHandlerInterface
{
public:
   TMFE* fMfe;
   TMFeEquipment* fEq;
   
   //ZeroMQ stuff
   
   void *context;
   void *responder;

   int fPort;

   int fEventSize;
   char* fEventBuf;

   RunStatusType RunStatus;
   int RUNNO;

   HistoryLogger logger;
   MessageHandler message;
   PeriodicityManager periodicity;

   feLabVIEWWorker(TMFE* mfe, TMFeEquipment* eq):logger(mfe,eq), message(mfe), periodicity(mfe,eq) // ctor
   {
      fMfe = mfe;
      fEq  = eq;
      //Default event size ok 10kb, will be overwritten by ODB entry in Init()
      fEventSize = 10000;
      fEventBuf  = NULL;

      context = zmq_ctx_new ();
      responder = zmq_socket (context, ZMQ_REP);

      RunStatus=Unknown;
      RUNNO=-1;
      fMfe->fOdbRoot->RI("Runinfo/Run number", &RUNNO);
      int period=1000;
      fEq->fOdbEqCommon->RI("Period",&period);
      eq->fCommon->Period=period;
   }

   ~feLabVIEWWorker() // dtor
   {
      if (fEventBuf) {
         free(fEventBuf);
         fEventBuf = NULL;
      }
      zmq_close(responder);
      zmq_ctx_destroy(context);
   }


   void Init()
   {
      fEq->fOdbEqSettings->RI("event_size", &fEventSize, true);
      fEq->fOdbEqSettings->WS("feVariables","No variables logged yet...",32);
      fEq->fOdbEqSettings->WU32("DateAdded",0);
      assert(fPort>0);
      if (fEventBuf) {
         free(fEventBuf);
      }
      fEventBuf = (char*)malloc(fEventSize);
      char bind_port[100];
      sprintf(bind_port,"tcp://*:%d",fPort);
      std::cout<<"Binding to: "<<bind_port<<std::endl;
      int rc=zmq_bind (responder, bind_port);
      assert (rc==0);
   }

   std::string HandleRpc(const char* cmd, const char* args)
   {
      fMfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);
      return "OK";
   }

   void HandleBeginRun()
   {
      fMfe->Msg(MINFO, "HandleBeginRun", "Begin run!");
      fMfe->fOdbRoot->RI("Runinfo/Run number", &RUNNO);
      fEq->SetStatus("Running", "#00FF00");
      RunStatus=Running;
   }

   void HandleEndRun()
   {
      fMfe->Msg(MINFO, "HandleEndRun", "End run!");
      fMfe->fOdbRoot->RI("Runinfo/Run number", &RUNNO);
      fEq->SetStatus("Stopped", "#00FF00");
      RunStatus=Stopped;
   }
   void LogBank(const char* buf)
   {
      LVBANK<void*>* ThisBank=(LVBANK<void*>*)buf;
      if (strncmp(ThisBank->NAME.DATATYPE,"DBL",3)==0) {
         LVBANK<double>* bank=(LVBANK<double>*)buf;
         //bank->print();
         logger.Update(bank);
      } else if (strncmp(ThisBank->NAME.DATATYPE,"FLT",3)==0) {
         LVBANK<float>* bank=(LVBANK<float>*)buf;
         logger.Update(bank);
      //Not supported by ODB
      /*} else if (strncmp(ThisBank->NAME.DATATYPE,"I64",3)==0) {
         LVBANK<int64_t>* bank=(LVBANK<int64_t>*)buf;
         logger.Update(bank);
      } else if (strncmp(ThisBank->NAME.DATATYPE,"U64",3)==0) {
         LVBANK<uint64_t>* bank=(LVBANK<uint64_t>*)buf;
         logger.Update(bank);*/
      } else if (strncmp(ThisBank->NAME.DATATYPE,"I32",3)==0) {
         LVBANK<int32_t>* bank=(LVBANK<int32_t>*)buf;
         logger.Update(bank);
      } else if (strncmp(ThisBank->NAME.DATATYPE,"UI32",4)==0) {
         LVBANK<int32_t>* bank=(LVBANK<int32_t>*)buf;
         //bank->print();
         logger.Update(bank);
      //Not supported by ODB
      /*} else if (strncmp(ThisBank->NAME.DATATYPE,"I16",3)==0) {
         LVBANK<int16_t>* bank=(LVBANK<int16_t>*)buf;
         logger.Update(bank);
      } else if (strncmp(ThisBank->NAME.DATATYPE,"U16",3)==0) {
         LVBANK<uint16_t>* bank=(LVBANK<uint16_t>*)buf;
         logger.Update(bank);
      } else if (strncmp(ThisBank->NAME.DATATYPE,"I8",2)==0) {
         LVBANK<int8_t>* bank=(LVBANK<int8_t>*)buf;
         logger.Update(bank);
      } else if (strncmp(ThisBank->NAME.DATATYPE,"U8",2)==0) {
         LVBANK<uint8_t>* bank=(LVBANK<uint8_t>*)buf;
         logger.Update(bank);*/
      } else if (strncmp(ThisBank->NAME.DATATYPE,"STR",3)==0) {
         LVBANK<char>* bank=(LVBANK<char>*)buf;
         if (strncmp(bank->NAME.VARNAME,"TALK",4)==0)
         {
            bank->print();
            fMfe->Msg(MTALK, "feLabVIEW", (char*)bank->DATA->DATA);
            if (strncmp(bank->NAME.VARCATEGORY,"THISHOST",8)==0)
            {
               periodicity.ProcessMessage(bank);
            }
            return;
         }
         else if (strncmp(bank->NAME.VARNAME,"GET_RUNNO",9)==0)
         {
            char buf[20]={0};
            //JSON format already
            sprintf(buf,"RunNumber:%d",RUNNO);
            message.QueueData(buf);
            return;
         }
         else if (strncmp(bank->NAME.VARNAME,"GET_STATUS",10)==0) {
            char buf[20];
            switch (RunStatus)
            {
               case Unknown:
                  sprintf(buf,"STATUS:UNKNOWN");
               case Running:
                  sprintf(buf,"STATUS:RUNNING");
               case Stopped:
                  sprintf(buf,"STATUS:STOPPED");
               default:
                  //JSON format already
                  message.QueueData(buf);
            }
            return;
      }
         logger.Update(bank);
      } else {
         std::cout<<"Unknown bank data type... "<<std::endl;
         ThisBank->print();
         exit(1);
      }
      return;
   }

   void HandleBankArray(char * ptr)
   {
      LVBANKARRAY* array=(LVBANKARRAY*)ptr;
      if (array->GetTotalSize() > (uint32_t)fEventSize)
      {
         char error[100];
         sprintf(error,"ERROR: [%s] More bytes sent (%u) than MIDAS has assiged for buffer (%u)",
                        fEq->fName.c_str(),
                        array->BlockSize + array->GetHeaderSize(),
                        fEventSize);
         message.QueueError(error);
         return;
      }
      //array->print();
      char *buf=(char*)&array->DATA[0];
      for (uint32_t i=0; i<array->NumberOfEntries; i++)
      {
         LVBANK<double>* bank=(LVBANK<double>*)buf;
         LogBank(buf);
         buf+=bank->GetHeaderSize()+bank->BlockSize*bank->NumberOfEntries;
      }
      return;
   }
   void HandleBank(char * ptr)
   {
      //Use invalid data type to probe the header
      LVBANK<void*>* ThisBank=(LVBANK<void*>*)ptr;
      //ThisBank->print();
      if (ThisBank->BlockSize+ThisBank->GetHeaderSize() > (uint32_t)fEventSize)
      {
         char error[100];
         sprintf(error,"ERROR: [%s] More bytes sent (%u) than MIDAS has assiged for buffer (%d)",
                        fEq->fName.c_str(),
                        ThisBank->GetTotalSize(),
                        fEventSize);
         message.QueueError(error);
         return;
      }
      LogBank(ptr);
      return;
   }

   void HandlePeriodic()
   {
      //printf("periodic!\n");
      std::chrono::time_point<std::chrono::system_clock> timer_start=std::chrono::high_resolution_clock::now();
      //char buf[256];
      //sprintf(buf, "buffered %d (max %d), dropped %d, unknown %d, max flushed %d", gUdpPacketBufSize, fMaxBuffered, fCountDroppedPackets, fCountUnknownPackets, fMaxFlushed);
      //fEq->SetStatus(buf, "#00FF00");


      fEq->ComposeEvent(fEventBuf, fEventSize);
      fEq->BkInit(fEventBuf, fEventSize);
      // ZeroMQ directly puts the data inside the data bank (aimed minimise copy operations)
      // there for we must define the MIDAS Bankname now, not after we know if its a LabVIEW
      // bank or LabVIEW Array LVB1 or LVA1
      char* ptr = (char*) fEq->BkOpen(fEventBuf, "LVD1", TID_STRUCT);

      int read_status=zmq_recv (responder, ptr, fEventSize, ZMQ_NOBLOCK);
      //No data to read... does quitting cause a memory leak? It seems not (tested with valgrind)
      if (read_status<0)
      {
         periodicity.LogPeriodicWithoutData();
         return;
      }
      else
      {
         fEq->WriteStatistics();
         periodicity.LogPeriodicWithData();
      }

      int BankSize=0;
      //printf ("[%s] Received %c%c%c%c (%d bytes)",fEq->fName.c_str(),ptr[0],ptr[1],ptr[2],ptr[3],read_status);
      if (strncmp(ptr,"PING",4)==0) {
         std::cout<<"PING!!!"<<std::endl;
         zmq_send(responder, "PONG", 4, 0);
         return;
      } else if (strncmp(ptr,"PYA1",4)==0 || strncmp(ptr,"LVA1",4)==0) {
         //std::cout<<"["<<fEq->fName.c_str()<<"] Python / LabVIEW Bank Array found!"<<std::endl;
         LVBANKARRAY* bank=(LVBANKARRAY*)ptr;
         BankSize=bank->GetTotalSize();
         HandleBankArray(ptr);
      } else if (strncmp(ptr,"PYB1",4)==0 || strncmp(ptr,"LVB1",4)==0 ) {
         //std::cout<<"["<<fEq->fName.c_str()<<"] Python / LabVIEW Bank found!"<<std::endl;
         LVBANK<void*>* bank=(LVBANK<void*>*)ptr;
         BankSize=bank->GetTotalSize();
         HandleBank(ptr);
      } else if (strncmp(ptr,"GIVE_ME_EVENT_SIZE",18)==0) {
            char message[32]={0};
            sprintf(message,"[\"EventSize:%d\"]",fEventSize);
            zmq_send(responder, message, strlen(message), 0);
            return;
      } else {
         std::cout<<"["<<fEq->fName.c_str()<<"] Unknown data type just received... "<<std::endl;
         message.QueueError("Unknown data type just received... ");
         for (int i=0; i<20; i++)
            std::cout<<ptr[i];
         exit(1);
      }
      fEq->BkClose(fEventBuf, ptr+BankSize);
      fEq->SendEvent(fEventBuf);
      std::chrono::time_point<std::chrono::system_clock> timer_stop=std::chrono::high_resolution_clock::now();
      std::chrono::duration<double> handlingtime=timer_stop - timer_start;
      //std::cout<<"["<<fEq->fName.c_str()<<"] Handling time: "<<handlingtime.count()*1000 <<"ms"<<std::endl;
      printf ("[%s] Handled %c%c%c%c (%d bytes) in %fms\n",fEq->fName.c_str(),ptr[0],ptr[1],ptr[2],ptr[3],read_status,handlingtime.count()*1000);
      
      char buf[100];
      sprintf(buf,"DATA OK (Processed in %fms)",1000*handlingtime.count());
      message.QueueMessage(buf);

      bool KillFrontend=message.HaveErrors();
      std::string reply=message.ReadMessageQueue();
      zmq_send (responder, reply.c_str(), reply.size(), 0);
      if (KillFrontend)
         exit(1);
      return;
   }
};

class feLabVIEWSupervisor :
   public TMFeRpcHandlerInterface,
   public  TMFePeriodicHandlerInterface
{
public:
   TMFE* fMfe;
   TMFeEquipment* fEq;
   
   //ZeroMQ stuff
   
   void *context;
   void *responder;

   int fPort;

   int fEventSize;
   char* fEventBuf;

   MVOdb* fOdbWorkers; 
  
   int fPortRangeStart;
   int fPortRangeStop;

   feLabVIEWSupervisor(TMFE* mfe, TMFeEquipment* eq) // ctor
   {
      fMfe = mfe;
      fEq  = eq;
      //Default event size ok 10kb, will be overwritten by ODB entry in Init()
      fEventSize = 10000;
      fEventBuf  = NULL;

      //So far... limit to 1000 frontend workers...
      fPortRangeStart = 13000;
      fPortRangeStop  = 13999;

      context = zmq_ctx_new ();
      responder = zmq_socket (context, ZMQ_REP);
      fPort=5555;
   }

   ~feLabVIEWSupervisor() // dtor
   {
      if (fEventBuf) {
         free(fEventBuf);
         fEventBuf = NULL;
      }
   }

   void Init()
   {

      fEq->fOdbEqSettings->RI("event_size", &fEventSize, true);
      fEq->fOdbEqSettings->RI("port_range_start",&fPortRangeStart, true);
      fEq->fOdbEqSettings->RI("port_range_stop",&fPortRangeStop, true);
      assert(fPort>0);
      fOdbWorkers=fEq->fOdbEqSettings->Chdir("WorkerList",true);
      fOdbWorkers->WS("HostName","",32);
      fOdbWorkers->WU32("DateAdded",0);
      fOdbWorkers->WU32("Port",0);

      if (fEventBuf) {
         free(fEventBuf);
      }
      fEventBuf = (char*)malloc(fEventSize);
      char bind_port[100];
      sprintf(bind_port,"tcp://*:%d",fPort);
      std::cout<<"Binding to: "<<bind_port<<std::endl;
      int rc=zmq_bind (responder, bind_port);
      assert (rc==0);
   }

   std::string HandleRpc(const char* cmd, const char* args)
   {
      fMfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);
      return "OK";
   }

   void HandleBeginRun()
   {
      fMfe->Msg(MINFO, "HandleBeginRun", "Begin run!");
      fEq->SetStatus("Running", "#00FF00");
   }

   void HandleEndRun()
   {
      fMfe->Msg(MINFO, "HandleEndRun", "End run!");
      fEq->SetStatus("Stopped", "#00FF00");
   }
   int FindHostInWorkerList(const char* hostname)
   {
      std::vector<std::string> hostlist;
      fOdbWorkers->RSA("HostName", &hostlist);
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
      std::cout<<"No Match... return size:"<<size<<std::endl;
      return size;
   }
   int AssignPortForWorker(uint workerID)
   {
      std::vector<uint32_t> list;
      fOdbWorkers->RU32A("Port", &list);
      if (workerID>=list.size())
      {
         int port=fPort+workerID+1;
         fOdbWorkers->WU32AI("Port",workerID,port);
         return port;
      }
      else
      {
         return list.at(workerID);
      }
   }
   const char* AddNewClient(const char* hostname)
   {
      std::cout<<"Check list of workers"<<std::endl;
      #if 1
      int WorkerNo=FindHostInWorkerList(hostname);
      int port=AssignPortForWorker(WorkerNo);
      std::cout<<"Assign port "<<port<< " for worker "<<WorkerNo<<std::endl;
      #else
      int port=5556;
      int WorkerNo=0;
      #endif
      void *local_context = zmq_ctx_new ();
      void *pinger = zmq_socket (local_context, ZMQ_REQ);
      char bind_port[100];
      sprintf(bind_port,"tcp://*:%d",port);
      std::cout<<"Binding to: "<<bind_port<<std::endl;
      int rc=zmq_bind (pinger, bind_port);
      if (rc!=0)
      {
         std::cout<<"Binding failed! The frontend is probably running... wahoo"<<std::endl;
      } else {
         zmq_unbind (pinger, bind_port);
         char command[100];
         sprintf(command,"./feLabVIEW.exe --client %s --port %u &> test-%u.log",hostname,port,WorkerNo);
         std::cout<<"Running command:" << command<<std::endl;
         ss_system(command);
         zmq_close(pinger);
         zmq_ctx_destroy(local_context);
         return "New Frontend started";
      }
      return "Frontend already running";
   }
   void HandlePeriodic()
   {
      //printf("periodic!\n");
      //std::chrono::time_point<std::chrono::system_clock> timer_start=std::chrono::high_resolution_clock::now();
      
      int read_status=zmq_recv (responder, fEventBuf, fEventSize, ZMQ_NOBLOCK);
      
      //No data to read... does quitting cause a memory leak? It seems not (tested with valgrind)
      if (read_status<0)
         return;
      //We use this as a char array... add terminating character at end of read
      fEventBuf[read_status]=0;
      std::cout<<fEventBuf<<std::endl;
      printf ("[%s] Supervisor received (%c%c%c%c)\n",fEq->fName.c_str(),fEventBuf[0],fEventBuf[1],fEventBuf[2],fEventBuf[3]);
      if (strncmp(fEventBuf,"START_FRONTEND",14)==0)
      {
         char hostname[100];
         sprintf(hostname,"%s",&fEventBuf[15]);
         //Trim the hostname at the first '.'
         for (int i=0; i<100; i++)
         {
            if (hostname[i]=='.')
            {
               hostname[i]=0;
               break;
            }
         }
         const char* response=AddNewClient(hostname);
         zmq_send (responder, response, strlen(response), 0);
         return;
      } else if (strncmp(fEventBuf,"GIVE_ME_ADDRESS",15)==0) {
         char hostname[100];
         sprintf(hostname,"%s",&fEventBuf[16]);
         //Trim the hostname at the first '.'
         for (int i=0; i<100; i++)
         {
            if (hostname[i]=='.')
            {
               hostname[i]=0;
               break;
            }
         }
         std::cout<<hostname<<std::endl;
         for (int i=0; i<100; i++)
         {
            if (hostname[i]=='.')
            {
               hostname[i]=0;
               break;
            }
         }
         char log_to_address[100];
         //sprintf(log_to_address,"%s","tcp://127.0.0.1:5556");
         int WorkerNo=FindHostInWorkerList(hostname);
         int port=AssignPortForWorker(WorkerNo);
         sprintf(log_to_address,"tcp://alphamidastest8:%u",port);
         std::cout<<"SEND DATA TO ADDRESS:"<<log_to_address<<std::endl;
         zmq_send (responder, log_to_address, strlen(log_to_address), 0);
         return;
      } else {
         std::cout<<"Unknown message just received: "<<std::endl;
         for (int i=0;i<50; i++)
            std::cout<<fEventBuf[i];
         exit(1);
      }
/*      std::chrono::time_point<std::chrono::system_clock> timer_stop=std::chrono::high_resolution_clock::now();
      std::chrono::duration<double> handlingtime=timer_stop - timer_start;
      std::cout<<"Handling time: "<<handlingtime.count()<<std::endl;
      if (error)
      {
         zmq_send (responder, error, strlen(error), 0);
         fMfe->Msg(MTALK, "feLabVIEW", error);
         exit(1);
      }
      else
      {
         char message[100];
         sprintf(message,"DATA OK (Processed in %fms)",1000*handlingtime.count());
         zmq_send (responder, message, strlen(message), 0);
      }*/
      return;
   }
};

static void usage()
{
   fprintf(stderr, "Usage: feLabview --supervisor\n");
   fprintf(stderr, "Usage: feLabview --client hostname\n");
   
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

   bool SupervisorMode=false;
   std::string client="NULL";
   int port          =5555;
   int max_event_size=0;
   // loop over the commandline options
   for (unsigned int i=1; i<args.size(); i++) 
   {
      const char* arg = args[i].c_str();
      //printf("argv[%d] is %s\n",i,arg);

      if (strncmp(arg,"--supervisor",12)==0) {
         std::cout<<"Starting in supervisor mode"<<std::endl;
         SupervisorMode=true;
      } else if (strncmp(arg,"--client",8)==0) {
         client = args[++i];
      } else if (strncmp(arg,"--port",8)==0) {
         port = atoi(args[++i].c_str());
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
      name+="LabVIEW_supervisor";
   else
   {
      name+="LV_";
      name+=client.c_str();
   }

   TMFE* mfe = TMFE::Instance();
   if (name.size()>31)
   {
      mfe->Msg(MERROR, "feLabVIEW", "Frontend name [%s] too long. Perhaps shorten hostname", name.c_str());
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

   eq->SetStatus("Starting...", "white");
   eq->ZeroStatistics();
   eq->WriteStatistics();

   mfe->RegisterEquipment(eq);

   if (SupervisorMode)
   {
      feLabVIEWSupervisor* myfe= new feLabVIEWSupervisor(mfe,eq);
      myfe->fPort=port;
      mfe->RegisterRpcHandler(myfe);
      myfe->Init();
      mfe->RegisterPeriodicHandler(eq, myfe);
   }
   else
   {
      feLabVIEWWorker* myfe = new feLabVIEWWorker(mfe,eq);
      myfe->fPort=port;
      mfe->RegisterRpcHandler(myfe);
      myfe->Init();
      mfe->RegisterPeriodicHandler(eq, myfe);
   }
   //mfe->SetTransitionSequenceStart(910);
   //mfe->SetTransitionSequenceStop(90);
   //mfe->DeregisterTransitionPause();
   //mfe->DeregisterTransitionResume();

   eq->SetStatus("Started...", "white");

   while (!mfe->fShutdownRequested) {
      mfe->PollMidas(10);
   }

   mfe->Disconnect();

   return 0;
}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
