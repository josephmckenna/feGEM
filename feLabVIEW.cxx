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
   template<typename T>
   HistoryVariable(const LVBANK<T>* lvbank)
   {
      fCategory=lvbank->NAME.VARCATEGORY;
      fVarName=lvbank->NAME.VARNAME;
      UpdateFrequency=lvbank->HistoryRate;
      fLastUpdate=0;
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
      //If the data is from less that 'UpdateFrequency' ago
      if (lvbank->DATA.back()->CoarseTime < fLastUpdate+UpdateFrequency)
         return;
      fLastUpdate=lvbank->DATA.back()->CoarseTime;

      std::cout<<"NOT UPDATING ODB!!!WIP"<<std::endl;
   }
};

class HistoryLogger
{
public:
   MVOdb* link;
   std::vector<HistoryVariable*> fVariables;
   HistoryLogger()
   {
   }
   template<typename T>
   HistoryVariable* AddNewVariable(const LVBANK<T>* lvbank)
   {
      fVariables.push_back(new HistoryVariable(lvbank));
      //Add entry to ODB of what var we are loggoing
      //ODB write (feLabVIEW/host/category/varname)
      std::cout<<"NOT LOGGING WHAT VAR IS LOGGING TO ODB BECAUSE WIP!"<<std::endl;
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

   Myfe(TMFE* mfe, TMFeEquipment* eq) // ctor
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
      if (fEventBuf) {
         free(fEventBuf);
      }
      fEventBuf = (char*)malloc(fEventSize);
      char bind_port[100];
      sprintf(bind_port,"tcp://*:%d",port);
      std::cout<<"Binding to: "<<bind_port<<std::endl;
      int rc=zmq_bind (responder, bind_port);
      //int rc = zmq_bind (responder, "tcp://*:5555");
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
   const char* HandleBankArray()
   {
      std::cout<<"BANKARRAY:"<<fEventBuf[0]<<fEventBuf[1]<<fEventBuf[2]<<fEventBuf[3]<<std::endl;
	  uint32_t number_of_bytes, number_of_banks;
	  memcpy(&number_of_bytes, fEventBuf+4, 4);
	  memcpy(&number_of_banks, fEventBuf+8, 4);
      std::cout<<"NumberOfBytes"<<number_of_bytes<<std::endl;
      std::cout<<"NumberOfBanks"<<number_of_banks<<std::endl;
      if (number_of_bytes+16>fEventSize)
      {
         char error[100];
         sprintf(error,"ERROR: More bytes sent (%d) than MIDAS has assiged for buffer (%d)",number_of_bytes,fEventSize);
         return error;
      
      }
      return NULL;
   }
   const char* HandleBank()
   {
      std::cout<<"BANK:"<<fEventBuf[0]<<fEventBuf[1]<<fEventBuf[2]<<fEventBuf[3]<<std::endl;
      char DATATYPE[5];
      for (int i=0; i<4; i++)
         DATATYPE[i]=fEventBuf[i+4];
      DATATYPE[4]=0;
      std::cout<<DATATYPE<<std::endl;
      uint32_t number_of_bytes, number_of_banks;
      int offset=4+4+16+16+32+4+4;
      memcpy(&number_of_bytes, fEventBuf+offset, 4);
      memcpy(&number_of_banks, fEventBuf+offset+4, 4);
      std::cout<<"NumberOfBytes"<<number_of_bytes<<std::endl;
      std::cout<<"NumberOfBanks"<<number_of_banks<<std::endl;
      if (number_of_bytes+offset+16>fEventSize)
      {
         char error[100];
         sprintf(error,"ERROR: More bytes sent (%d) than MIDAS has assiged for buffer (%d)",number_of_bytes,fEventSize);
         return error;
      }
      //std::istream is();//ios_base::binary
      //std::stringstream is(fEventBuf);
      if (strcmp(DATATYPE,"DBLE")==0)
      {
         LVBANK<double> bank(fEventBuf);
         //bank<<(const char*)fEventBuf;]
         //std::istream is(&fEventBuf);
         //std::istream* s=std::istream::get(fEventBuf,fEventSize);
         //is>>bank;
                  //is.get(fEventBuf,fEventSize)>>bank;
         //pbuf>>bank;
         bank.print();
         logger.Update(&bank);
      } else if (strcmp(DATATYPE,"INT3")==0)
      {
         LVBANK<int32_t> bank;
         //bank<<fEventBuf;
         //is.get(fEventBuf,fEventSize)>>bank;
         bank.print();
         logger.Update(&bank);
      } else {
         std::cout<<"Unknown bank data type... "<<std::endl;
         exit(1);
      }
      

      return 0;
   }
   void AnnounceError(const char* error)
   {

   }
   void HandlePeriodic()
   {
      printf("periodic!\n");
      //char buf[256];
      //sprintf(buf, "buffered %d (max %d), dropped %d, unknown %d, max flushed %d", gUdpPacketBufSize, fMaxBuffered, fCountDroppedPackets, fCountUnknownPackets, fMaxFlushed);
      //fEq->SetStatus(buf, "#00FF00");
      //fEq->WriteStatistics();
      
      //char buffer [10];
      int read_status=zmq_recv (responder, fEventBuf, fEventSize, ZMQ_NOBLOCK);
      if (read_status<0) return;

      const char* error;
      printf ("Received (%c%c%c%c)\n",fEventBuf[0],fEventBuf[1],fEventBuf[2],fEventBuf[3]);
      if (strncmp(fEventBuf,"PYA1",4)==0) {
         std::cout<<"Python Bank Array found!"<<std::endl;
         error=HandleBankArray();
      } else if (strncmp(fEventBuf,"PYB1",4)==0) {
         std::cout<<"Python Bank found!"<<std::endl;
         error=HandleBank();
      } else {
         std::cout<<"Unknown data type just received... "<<std::endl;
         exit(1);
      }
      //zmq_msg_t* msg;
      //zmq_recvmsg (responder, msg, 0);
      //zmq_recv (responder, buffer, 10, ZMQ_NOBLOCK);
      //printf ("Received (%s)\n",msg);
      if (error)
      {
         zmq_send (responder, error, strlen(error), 0);
         AnnounceError(error);
         std::cout<<"ANNOUCE ERROR!"<<std::endl;
         exit(1);
      }
      else
      {
         zmq_send (responder, "DATA OK", 7, 0);
      }
      //Event finished... sanitise 
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
