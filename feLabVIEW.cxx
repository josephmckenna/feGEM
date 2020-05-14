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

#include <list>
#include <mutex>
//Thread safe class to monitor host permissions
class AllowedHosts
{
   class Host{
      public:
      const std::string HostName;
      int RejectionCount;
      std::chrono::time_point<std::chrono::system_clock> LastContact;
      Host(const char* hostname): HostName(hostname)
      {
         RejectionCount=0;
         LastContact=std::chrono::high_resolution_clock::now();
      }
      double TimeSince(std::chrono::time_point<std::chrono::system_clock> t)
      {
         return std::chrono::duration<double, std::milli>(t-LastContact).count();
      }
     double TimeSinceLastContact()
       {
         return TimeSince(std::chrono::high_resolution_clock::now());
      }
      bool operator==(const char* hostname) const
      {
         return (strcmp(HostName.c_str(),hostname)==0);
      }
      bool operator==(const Host & rhs) const
      {   
         return HostName==rhs.HostName;
      }
      void print()
      {
         std::cout<<"HostName:\t"<<HostName<<"\n";
         std::cout<<"RejectionCount:\t"<<RejectionCount<<"\n";
         std::cout<<"Last rejection:\t"<< TimeSinceLastContact()*1000.<<"s ago"<<std::endl;
      }
   };
   private:
   std::mutex list_lock;
   //Allowed hosts:
   std::vector<Host> white_list;
   //Hosts with questioned behaviour
   std::list<Host> grey_list;
   //Banned hosts:
   std::vector<Host> black_list;
   const int cool_down_time; //ms
   const int retry_limit;
   
   MVOdb* fOdbEqSettings;
   
   public:
   AllowedHosts(TMFE* mfe): cool_down_time(1000), retry_limit(10)
   {
      //Set cooldown time to 10 seconds
      //Set retry limit to 10
      fOdbEqSettings=mfe->fOdbRoot->Chdir((std::string("Equipment/") + mfe->fFrontendName + std::string("/Settings")).c_str() );
      std::vector<std::string> list;
      
      fOdbEqSettings->RSA("allowed_hosts", &list,true);
      for (auto host: list)
         white_list.push_back(Host(host.c_str()));
      
      fOdbEqSettings->RSA("black_listed_hosts", &list,true);
      for (auto host: list)
         black_list.push_back(Host(host.c_str()));
   }
   void PrintRejection(TMFE* mfe,const char* hostname)
   {
      for (auto & host: black_list)
      {
         if (host==hostname)
         {
            if (host.RejectionCount<2*retry_limit)
               mfe->Msg(MERROR, "tryAccept", "rejecting connection from unallowed host \'%s\'", hostname);
            if (host.RejectionCount==2*retry_limit)
               mfe->Msg(MERROR, "tryAccept", "rejecting connection from unallowed host \'%s\'. This message will now be suppressed", hostname); 
            host.RejectionCount++;
         }
      }
      
   }
   bool IsAllowed(const char* hostname)
   {
      if (IsWhiteListed(hostname))
         return true;
      if (IsBlackListed(hostname))
         return false;
      if (IsGreyListed(hostname))
         return true;  
   }
   bool IsWhiteListed(const char* hostname)
   {
      //std::cout<<"Looking for host:"<<hostname<<std::endl;
      std::lock_guard<std::mutex> lock(list_lock);
      for (auto host: white_list)
      {
         //host.print();
         if (host==hostname)
            return true;
      }
      return false;
   }
   //Allow this host:
   bool AddHost(const char* hostname)
   {
      if (!IsWhiteListed(hostname))
      {
         {
         std::lock_guard<std::mutex> lock(list_lock);
         white_list.push_back(Host(hostname));
         }
         std::cout<<"DAVE"<<hostname<<std::endl;
         fOdbEqSettings->WSAI("allowed_hosts",(int)white_list.size(), hostname);
         //True for new item added
         return true;
      }
      //False, item not added (already in list)
      return false;
   }
   //Ban this host:
   bool BlackList(const char* hostname)
   {
      if (!IsBlackListed(hostname))
      {
         {
         std::lock_guard<std::mutex> lock(list_lock);
         black_list.push_back(Host(hostname));
         fOdbEqSettings->WSAI("black_listed_hosts",black_list.size() -1, hostname );
         }
         return true;
      }
      return false;
   }

   private:
   bool IsBlackListed(const char* hostname)
   {
      const std::lock_guard<std::mutex> lock(list_lock);
      if (!black_list.size()) return false;
      for (auto & host: black_list)
      //for(std::vector<Host>::iterator host = black_list.begin(); host != black_list.end(); ++host) 
      {
         if (host==hostname)
            return true;
      }
      return false;
   }
   bool IsGreyListed(const char* hostname)
   {
      const std::lock_guard<std::mutex> lock(list_lock);
      for (auto& host: grey_list)
      {
         if (host==hostname)
         {
            host.print();
            std::cout<<"Rejection count:"<<host.RejectionCount<<std::endl;
            if (host.RejectionCount>retry_limit)
            {
               std::cout<<"Black listing "<<hostname<<std::endl;
               black_list.push_back(host);
               grey_list.remove(host);
            }
            
            std::chrono::time_point<std::chrono::system_clock> time_now=std::chrono::high_resolution_clock::now();
            std::cout<<host.TimeSince(time_now) << ">"<<cool_down_time<<std::endl;
            if (host.TimeSince(time_now)>cool_down_time)
            {
               std::cout<<"I've seen this host before, but "<<host.TimeSince(time_now)/1000. <<" seconds a long time ago"<<std::endl;
               host.LastContact=time_now;
               host.RejectionCount++;
               return true;
            }
            else
            {
               std::cout<<"This host has tried to connect too recently"<<std::endl;
               return false;
            }
         }
      }
      //This is the first time a host has tried to connect:
      grey_list.push_back(Host(hostname));
      return true;
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
      if (!UpdateFrequency)
         return;
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

class feLabVIEWClass :
   public TMFeRpcHandlerInterface,
   public TMFePeriodicHandlerInterface
{
public:
   TMFE* fMfe;
   TMFeEquipment* fEq;
   
   enum feLabVIEWClassEnum{SUPERVISOR,WORKER,INVALID};
   const int feLabVIEWClassType;
   //TCP stuff
   int server_fd;
   struct sockaddr_in address;
   int addrlen = sizeof(address);
   int fPort;

   int fEventSize;
   int lastEventSize; //Used to monitor any changes to fEventSize
   char* fEventBuf;

   //Periodic task query items (sould only be send from worker class... not yet limited)
   RunStatusType RunStatus;
   int RUNNO;
   AllowedHosts* allowed_hosts;
   MessageHandler message;
   PeriodicityManager periodicity;
   HistoryLogger logger;
   feLabVIEWClass(TMFE* mfe, TMFeEquipment* eq , AllowedHosts* hosts, int type ):
      feLabVIEWClassType(type),
      message(mfe), 
      periodicity(mfe,eq), 
      logger(mfe,eq)
   {
      allowed_hosts=hosts;
   }

   virtual std::pair<int,bool> FindHostInWorkerList(const char* hostname) { assert(0); return {-1,false}; };
   virtual int AssignPortForWorker(uint workerID) { assert(0); return -1; };
   virtual const char* AddNewClient(const char* hostname) { assert(0); return NULL; };

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
   void HandleStrBank(LVBANK<char>* bank)
   {
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
      else if (strncmp(bank->NAME.VARNAME,"GET_STATUS",10)==0)
      {
         char buf[20];
         switch (RunStatus)
         {
            case Unknown:
               sprintf(buf,"RunStatus:UNKNOWN");
            case Running:
               sprintf(buf,"RunStatus:RUNNING");
            case Stopped:
               sprintf(buf,"RunStatus:STOPPED");
            default:
               //JSON format already
               message.QueueData(buf);
         }
         return;
      }
      else if (strncmp(bank->NAME.VARNAME,"GET_EVENT_SIZE",14)==0)
      {
         char buf[80]={0};
         //JSON format already
         sprintf(buf,"EventSize:%d",fEventSize);
         std::cout<<buf<<std::endl;
         message.QueueData(buf);
         return;
      }
      else if (strncmp(bank->NAME.VARNAME,"SET_EVENT_SIZE",14)==0)
      {
         assert(feLabVIEWClassType==WORKER);
         std::cout<<"Updating event size:"<<(char*)&bank->DATA->DATA<<std::endl;
         fEventSize=atoi((char*)&bank->DATA->DATA);
         if (fEventSize<10000)
         {
            fMfe->Msg(MTALK, "feLabVIEW", "Minimum event size can not be less that 10 kilo bytes");
            fEventSize=10000;
         }
         fEq->fOdbEqSettings->WI("event_size", fEventSize);
         std::cout<<"Event size updated to:"<<fEventSize<<std::endl;
         return;
      }
      else if (strncmp(bank->NAME.VARNAME,"START_FRONTEND",14)==0)
      {
         assert(feLabVIEWClassType==SUPERVISOR);
         message.QueueData(AddNewClient(bank->DATA->DATA));
         return;
      }
      else if (strncmp(bank->NAME.VARNAME,"ALLOW_HOST",14)==0)
      {
         allowed_hosts->AddHost(bank->DATA->DATA);
         return;
      }
      else if (strncmp(bank->NAME.VARNAME,"GIVE_ME_ADDRESS",15)==0)
      {
         assert(feLabVIEWClassType==SUPERVISOR);
         message.QueueData("SendToAddress:alphamidastest8");
         return;
      }
      else if (strncmp(bank->NAME.VARNAME,"GIVE_ME_PORT",14)==0)
      {
         assert(feLabVIEWClassType==SUPERVISOR);
         std::pair<int,bool> WorkerNo=FindHostInWorkerList(bank->DATA->DATA);
         assert(WorkerNo.second=true); //Assert the frontend thread is running
         int port=AssignPortForWorker(WorkerNo.first);
         char log_to_port[80];
         sprintf(log_to_port,"SendToPort:%u",port);
         //std::cout<<"SEND TO PORT:"<<port<<std::endl;
         std::cout<<log_to_port<<std::endl;
         message.QueueData(log_to_port);
      }
      else
      {
         std::cout<<"String not understood!"<<std::endl;
         bank->print();
      }
      logger.Update(bank);
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
         return HandleStrBank(bank);
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
         //perror("accept"); 
         //exit(EXIT_FAILURE); 
         return;
      }

      //Security: Check if host in in allowed host lists
      char hostname[200];
      int name_status=getnameinfo((struct sockaddr *)&address, addrlen,
                       hostname, 200,NULL,NULL,0);
      bool allowed=false;
      if (feLabVIEWClassType==WORKER)
      {
         //Only white listed hosts allow on worker
         allowed=allowed_hosts->IsWhiteListed(hostname);
      }
      else if (feLabVIEWClassType==SUPERVISOR)
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
      ((LVBANKARRAY*)fEventBuf)->ClearHeader();
      ((LVBANK<void*>*)fEventBuf)->ClearHeader();
      bool legal_message=false;
      //Prepare the MIDAS bank so that we can directly write into from the TCP buffer
      fEq->ComposeEvent(fEventBuf, fEventSize);
      fEq->BkInit(fEventBuf, fEventSize);
      // We place the data inside the data bank (aimed minimise move/copy operations for speed)
      // therefor we must define the MIDAS Bankname now, not after we know if its a LabVIEW
      // bank or LabVIEW Array LVB1 or LVA1
      char* ptr = (char*) fEq->BkOpen(fEventBuf, "LVD1", TID_STRUCT);
      int read_status=0;
      int position=0;

      int BankSize=-1;
      // Get the first chunk of the message (must be atleast the header of the data coming)
      // The header of a LVBANK is 88 bytes
      // The header of a LVBANKARRAY is 32 bytes
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
            fMfe->Msg(MTALK, "feLabVIEW", message);
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
      if (strncmp(ptr,"PYA1",4)==0 || strncmp(ptr,"LVA1",4)==0)
      {
         LVBANKARRAY* bank=(LVBANKARRAY*)ptr;
         BankSize=bank->GetTotalSize();
      }
      else if (strncmp(ptr,"PYB1",4)==0 || strncmp(ptr,"LVB1",4)==0)
      {
         LVBANK<void*>* bank=(LVBANK<void*>*)ptr;
         BankSize=bank->GetTotalSize();
      }   //std::cout<<"BankSize:"<<BankSize<<std::endl;
      else
      {
         cm_msg(MTALK, "feLabVIEW", "Host %s is sending malformed data... black listing...", hostname);
         //std::cout<<"Black listing host!"<<std::endl;
         allowed_hosts->BlackList(hostname);
         legal_message=false;
         close(new_socket);
         return;
      }
      
      //The header looks ok, lets get the whole thing LVBANK / LVBANKARRAY
      while (position<BankSize)
      {
         read_status= read( new_socket , ptr+position, BankSize-position);
         if (!read_status) sleep(0.1);
         position+=read_status;
         if (--max_reads == 0)
         {
            char message[100];
            sprintf(message,"TCP Read timeout getting LVBANKARRAY");
            fMfe->Msg(MTALK, "feLabVIEW", message);
            return;
         } 
      }
      //std::cout<<BankSize<<"=="<<position<<std::endl;
      if (BankSize==position)
         legal_message=true;
      assert(BankSize==position);
      read_status=position;
      
      //Process what we have read into the MIDAS bank
      //printf ("[%s] Received %c%c%c%c (%d bytes)",fEq->fName.c_str(),ptr[0],ptr[1],ptr[2],ptr[3],read_status);
      if (strncmp(ptr,"PYA1",4)==0 || strncmp(ptr,"LVA1",4)==0) {
         //std::cout<<"["<<fEq->fName.c_str()<<"] Python / LabVIEW Bank Array found!"<<std::endl;
         HandleBankArray(ptr); //Iterates over array with HandleBank()
      } else if (strncmp(ptr,"PYB1",4)==0 || strncmp(ptr,"LVB1",4)==0 ) {
         //std::cout<<"["<<fEq->fName.c_str()<<"] Python / LabVIEW Bank found!"<<std::endl;
         HandleBank(ptr);
      } else {
         std::cout<<"["<<fEq->fName.c_str()<<"] Unknown data type just received... "<<std::endl;
         message.QueueError("Unknown data type just received... ");
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
      printf ("[%s] Handled %c%c%c%c (%d bytes) in %fms\n",fEq->fName.c_str(),ptr[0],ptr[1],ptr[2],ptr[3],read_status,handlingtime.count());

      char buf[100];
      sprintf(buf,"DATA OK");
      message.QueueMessage(buf);
      sprintf(buf,"MIDASTime:%f", handlingtime  .count());
      message.QueueData(buf);
      bool KillFrontend=message.HaveErrors();
      std::string reply=message.ReadMessageQueue();
      send(new_socket, reply.c_str(), reply.size(), 0 );
      shutdown(new_socket,SHUT_RD);
      close(new_socket);
      //zmq_send (responder, reply.c_str(), reply.size(), 0);
      if (KillFrontend)
         exit(1);
      return;
   }
};

class feLabVIEWWorker :
   public feLabVIEWClass
{
   public:

   feLabVIEWWorker(TMFE* mfe, TMFeEquipment* eq, AllowedHosts* hosts): feLabVIEWClass(mfe,eq,hosts,WORKER)
   {
      fMfe = mfe;
      fEq  = eq;
      //Default event size ok 10kb, will be overwritten by ODB entry in Init()
      fEventSize = 10000;
      fEventBuf  = NULL;

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
      fMfe->fOdbRoot->RI("Runinfo/Run number", &RUNNO);
      //int period=1000;
      //fEq->fOdbEqCommon->RI("Period",&period);
      //fEq->fCommon->Period=period;
   }

   ~feLabVIEWWorker() // dtor
   {
      if (fEventBuf) {
         free(fEventBuf);
         fEventBuf = NULL;
      }
      //zmq_close(responder);
   }


   void Init()
   {
      fEq->fOdbEqSettings->RI("event_size", &fEventSize, true);
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
      std::cout<<"Binding to: "<<bind_port<<std::endl;
      //int rc=zmq_bind (responder, bind_port);
      address.sin_family = AF_INET; 
      address.sin_addr.s_addr = INADDR_ANY; 
      address.sin_port = htons( fPort ); 

       // Forcefully attaching socket to the port fPort
      if (bind(server_fd, (struct sockaddr *)&address,  
                                 sizeof(address))<0) 
      { 
         //perror("bind failed"); 
         exit(1); 
      }
      //assert (rc==0);
   }

};

class feLabVIEWSupervisor :
   public feLabVIEWClass
{
public:
   MVOdb* fOdbWorkers;

   int fPortRangeStart;
   int fPortRangeStop;

   feLabVIEWSupervisor(TMFE* mfe, TMFeEquipment* eq): feLabVIEWClass(mfe,eq,new AllowedHosts(mfe),SUPERVISOR) // ctor
   {
      fMfe = mfe;
      fEq  = eq;
      //Default event size ok 10kb, will be overwritten by ODB entry in Init()
      fEventSize = 10000;
      fEventBuf  = NULL;

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

      fPort=5555;

      int period=10;
      fEq->fOdbEqCommon->WI("Period",period);
      fEq->fCommon->Period=period;

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
      //int rc=zmq_bind (responder, bind_port);
      address.sin_family = AF_INET; 
      address.sin_addr.s_addr = INADDR_ANY; 
      address.sin_port = htons( fPort ); 

       // Forcefully attaching socket to the port 5555
      if (bind(server_fd, (struct sockaddr *)&address,  
                                 sizeof(address))<0) 
      { 
         //perror("bind failed"); 
         exit(1); 
      }  
      //assert (rc==0);
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
   virtual std::pair<int,bool> FindHostInWorkerList(const char* hostname)
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
            return {i,true};
         }
      }
      fOdbWorkers->WSAI("HostName",size,hostname);
      std::cout<<"No Match... return size:"<<size<<std::endl;
      return {size,false};
   }
   virtual int AssignPortForWorker(uint workerID)
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
   virtual const char* AddNewClient(const char* hostname)
   {
      std::cout<<"Check list of workers"<<std::endl;
      std::pair<int,bool> WorkerNo=FindHostInWorkerList(hostname);
      int port=AssignPortForWorker(WorkerNo.first);
      std::cout<<"Assign port "<<port<< " for worker "<<WorkerNo.first<<std::endl;
      if (WorkerNo.second==false)
      {
         allowed_hosts->AddHost(hostname);

         std::string name = "fe";
         name+="LV_";
         name+=hostname;

         TMFE* mfe=fMfe;
         if (name.size()>31)
         {
            mfe->Msg(MERROR, "feLabVIEW", "Frontend name [%s] too long. Perhaps shorten hostname", name.c_str());
            exit(1);
         }

         TMFeCommon *common = new TMFeCommon();
         common->EventID = 1;
         common->LogHistory = 1;

         TMFeEquipment* worker_eq = new TMFeEquipment(mfe, name.c_str(), common);
         worker_eq->Init();

         worker_eq->SetStatus("Starting...", "white");
         worker_eq->ZeroStatistics();
         worker_eq->WriteStatistics();
         mfe->RegisterEquipment(worker_eq);
         feLabVIEWWorker* workerfe = new feLabVIEWWorker(mfe,worker_eq,allowed_hosts);
         workerfe->fPort=port;
         mfe->RegisterRpcHandler(workerfe);
         workerfe->Init();
         mfe->RegisterPeriodicHandler(worker_eq, workerfe);

         //mfe->StartRpcThread();
         //mfe->StartPeriodicThread();
         //mfe->StartPeriodicThreads();
         worker_eq->SetStatus("Started", "white");
         return "FrontendStatus:New Frontend started";
      }
      return "FrontendStatus:Frontend already running";
   }
  
};

static void usage()
{
   fprintf(stderr, "Usage: feLabview.exe\n");
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

   bool SupervisorMode=true;
   std::string client="NULL";
   int port          =5555;
   int max_event_size=0;
   // loop over the commandline options
   for (unsigned int i=1; i<args.size(); i++) 
   {
      const char* arg = args[i].c_str();
      //printf("argv[%d] is %s\n",i,arg);

      if (strncmp(arg,"--client",8)==0) {
         client = args[++i];
         SupervisorMode=false;
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
   {
      std::cout<<"Starting in supervisor mode"<<std::endl;
      name+="LabVIEW";
   }
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
      //Probably broken
      feLabVIEWWorker* myfe = new feLabVIEWWorker(mfe,eq,new AllowedHosts(mfe));
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
