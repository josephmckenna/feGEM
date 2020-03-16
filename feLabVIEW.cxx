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

#include <zmq.h>
#include "feLabVIEW.h"

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
      fCategory=lvbank->NAME.VARCATEGORY;
      fVarName=lvbank->NAME.VARNAME;
      UpdateFrequency=lvbank->HistoryRate;
      fLastUpdate=0;

      //Prepare ODB entry for variable
      MVOdb* OdbEq = mfe->fOdbRoot->Chdir((std::string("Equipment/") + fCategory).c_str(), true);
      fOdbEqVariables  = OdbEq->Chdir("Variables", true);
   }
   template<typename T>
   bool IsMatch(const LVBANK<T>* lvbank)
   {
      if (strcmp(fCategory.c_str(),lvbank->NAME.VARCATEGORY)!=0)
         return false;
      if (strcmp(fVarName.c_str(),lvbank->NAME.VARNAME)!=0)
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
      assert(lvbank->NAME.VARCATEGORY[15]==0);
      assert(lvbank->NAME.VARNAME[15]==0);
      
      //Store list of logged variables in Equipment settings
      char VarAndCategory[32];
      sprintf(VarAndCategory,"%s/%s",lvbank->NAME.VARCATEGORY,lvbank->NAME.VARNAME);
      fEq->fOdbEqSettings->WSAI("feVariables",fVariables.size(), VarAndCategory);
      fEq->fOdbEqSettings->WU32AI("DateAdded",(int)fVariables.size(), lvbank->GetFirstUnixTimestamp());
      
      //Push into list of monitored variables
      fVariables.push_back(new HistoryVariable(lvbank,fMfe));

      //Announce in control room new variable is logging
      char message[100];
      sprintf(message,"New variable %s in category %s being logged",lvbank->NAME.VARNAME,lvbank->NAME.VARCATEGORY);
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


class Myfe :
   public TMFeRpcHandlerInterface,
   public  TMFePeriodicHandlerInterface
{
public:
   TMFE* fMfe;
   TMFeEquipment* fEq;
   
   //ZeroMQ stuff
   
   void *context;
   void *responder;

   int port;

   int fEventSize;
   char* fEventBuf;

   HistoryLogger logger;

   Myfe(TMFE* mfe, TMFeEquipment* eq):logger(mfe,eq) // ctor
   {
      fMfe = mfe;
      fEq  = eq;
      //Default event size ok 10kb, will be overwritten by ODB entry in Init()
      fEventSize = 10000;
      fEventBuf  = NULL;

      context = zmq_ctx_new ();
      responder = zmq_socket (context, ZMQ_REP);
      port=5555;
   }

   ~Myfe() // dtor
   {
      if (fEventBuf) {
         free(fEventBuf);
         fEventBuf = NULL;
      }
   }

   void Init()
   {
      fEq->fOdbEqSettings->RI("event_size", &fEventSize, true);
      fEq->fOdbEqSettings->WS("feVariables","No variables logged yet...",32);
      fEq->fOdbEqSettings->WU32("DateAdded",0);
      if (fEventBuf) {
         free(fEventBuf);
      }
      fEventBuf = (char*)malloc(fEventSize);
      char bind_port[100];
      sprintf(bind_port,"tcp://*:%d",port);
      std::cout<<"Binding to: "<<bind_port<<std::endl;
      int rc=zmq_bind (responder, bind_port);
      assert (rc==0);
   }

   void SendEvent(double dvalue)
   {
      fEq->ComposeEvent(fEventBuf, fEventSize);
      fEq->BkInit(fEventBuf, fEventSize);

      double* ptr = (double*)fEq->BkOpen(fEventBuf, "test", TID_DOUBLE);
      *ptr = dvalue;
      fEq->BkClose(fEventBuf, ptr+1);

      fEq->SendEvent(fEventBuf);
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
      } else if (strncmp(ThisBank->NAME.DATATYPE,"U32",3)==0) {
         LVBANK<int32_t>* bank=(LVBANK<int32_t>*)buf;
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
      } else if (strncmp(ThisBank->NAME.DATATYPE,"CHAR",4)==0) {
         LVBANK<char>* bank=(LVBANK<char>*)buf;
         logger.Update(bank);
      } else {
         std::cout<<"Unknown bank data type... "<<std::endl;
         ThisBank->print();
         exit(1);
      }
      return;
   }

   const char* HandleBankArray(char * ptr)
   {
      LVBANKARRAY* array=(LVBANKARRAY*)ptr;
      if (array->GetTotalSize() > (uint32_t)fEventSize)
      {
         char error[100];
         sprintf(error,"ERROR: More bytes sent (%u) than MIDAS has assiged for buffer (%u)",array->BlockSize+ array->GetHeaderSize(),fEventSize);
         return error;
      }
      array->print();
      char *buf=(char*)&array->DATA[0];
      for (uint32_t i=0; i<array->NumberOfEntries; i++)
      {
         LVBANK<double>* bank=(LVBANK<double>*)buf;
         LogBank(buf);
         buf+=bank->GetHeaderSize()+bank->BlockSize*bank->NumberOfEntries;
      }
      return NULL;
   }
   const char* HandleBank(char * ptr)
   {
      //Use invalid data type to probe the header
      LVBANK<void*>* ThisBank=(LVBANK<void*>*)ptr;
      if (ThisBank->BlockSize+ThisBank->GetHeaderSize() > (uint32_t)fEventSize)
      {
         char error[100];
         sprintf(error,"ERROR: More bytes sent (%u) than MIDAS has assiged for buffer (%u)",ThisBank->BlockSize+ThisBank->GetHeaderSize(),fEventSize);
         return error;
      }
      LogBank(ptr);
      return NULL;
   }
   void AnnounceError(const char* error)
   {
      fMfe->Msg(MTALK, "feLabVIEW", error);
   }
   void HandlePeriodic()
   {
      printf("periodic!\n");
      //char buf[256];
      //sprintf(buf, "buffered %d (max %d), dropped %d, unknown %d, max flushed %d", gUdpPacketBufSize, fMaxBuffered, fCountDroppedPackets, fCountUnknownPackets, fMaxFlushed);
      //fEq->SetStatus(buf, "#00FF00");
      //fEq->WriteStatistics();
      
      //char buffer [10];

      fEq->ComposeEvent(fEventBuf, fEventSize);
      fEq->BkInit(fEventBuf, fEventSize);
      // ZeroMQ directly puts the data inside the data bank (aimed minimise copy operations)
      // there for we must define the MIDAS Bankname now, not after we know if its a LabVIEW
      // bank or LabVIEW Array LVB1 or LVA1
      char* ptr = (char*) fEq->BkOpen(fEventBuf, "LVD1", TID_STRUCT);
      
      int read_status=zmq_recv (responder, ptr, fEventSize, ZMQ_NOBLOCK);
      //No data to read... does quitting cause a memory leak? It seems not (tested with valgrind)
      if (read_status<0)
         return;
      int BankSize=0;

      const char* error;
      printf ("Received (%c%c%c%c)\n",ptr[0],ptr[1],ptr[2],ptr[3]);
      if (strncmp(ptr,"PYA1",4)==0 || strncmp(ptr,"LVA1",4)==0) {
         std::cout<<"Python / LabVIEW Bank Array found!"<<std::endl;
         LVBANKARRAY* bank=(LVBANKARRAY*)ptr;
         BankSize=bank->GetTotalSize();
         error=HandleBankArray(ptr);
      } else if (strncmp(ptr,"PYB1",4)==0 || strncmp(ptr,"LVB1",4)==0 ) {
         std::cout<<"Python / LabVIEW Bank found!"<<std::endl;
         LVBANK<void*>* bank=(LVBANK<void*>*)ptr;
         BankSize=bank->GetTotalSize();
         error=HandleBank(ptr);
      } else {
         std::cout<<"Unknown data type just received... "<<std::endl;
         exit(1);
      }
      if (error)
      {
         zmq_send (responder, error, strlen(error), 0);
         AnnounceError(error);
         exit(1);
      }
      else
      {
         zmq_send (responder, "DATA OK", 7, 0);
      }
      fEq->BkClose(fEventBuf, ptr+BankSize);
      fEq->SendEvent(fEventBuf);
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
   int max_event_size=0;
   // loop over the commandline options
   for (unsigned int i=1; i<args.size(); i++) 
   {
      const char* arg = args[i].c_str();
      //printf("argv[%d] is %s\n",i,arg);

      if (strncmp(arg,"--supervisor",12)==0) {
         std::cout<<"Supervisor mode not set up"<<std::endl;
         SupervisorMode=true;
      } else if (strncmp(arg,"--client",8)==0) {
         client = args[++i];
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
   std::string name = "feLabVIEW_";
   if (SupervisorMode)
      name+="Supervisor";
   else
      name+=client.c_str();

   TMFE* mfe = TMFE::Instance();

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

   Myfe* myfe = new Myfe(mfe,eq);
   myfe->fEventSize=max_event_size;
   mfe->RegisterRpcHandler(myfe);

   //mfe->SetTransitionSequenceStart(910);
   //mfe->SetTransitionSequenceStop(90);
   //mfe->DeregisterTransitionPause();
   //mfe->DeregisterTransitionResume();

   myfe->Init();

   mfe->RegisterPeriodicHandler(eq, myfe);

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
