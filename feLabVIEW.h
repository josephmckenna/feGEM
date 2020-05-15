#include <chrono>  // for high_resolution_clock
#include <iostream>
#include <stdlib.h>
#include <math.h> 
#include <vector>
#include <stdlib.h>   
#include <cstring>
#include <deque>
#include <cstring>
#include <fstream>

#include <stdio.h>
#include <signal.h> // SIGPIPE
#include <assert.h> // assert()
#include <stdlib.h> // malloc()
#include "midas.h"
#include "tmfe.h"

template <class T>
T change_endian(T in)
{
    char* const p = reinterpret_cast<char*>(&in);
    for (size_t i = 0; i < sizeof(T) / 2; ++i)
        std::swap(p[i], p[sizeof(T) - i - 1]);
    return in;
}

std::string SanitiseBankString(const char* input, int assert_size=0)
{
   //std::cout<<input;
   const int input_size=strlen(input);
   int output_size=input_size;
   if (assert_size)
      output_size=assert_size;
   std::string output;
   output.resize(output_size);
   
   int i,j;
   for (i = 0, j = 0; i<input_size; i++,j++)
   {
      if (isalnum(input[i]) || input[i]=='_' || input[i]=='-')
      {
         output[j]=input[i];
      }
      else
         j--;
      if (j>=output_size)
         break;
   }
   output[j]=0;
   if (assert_size)
      output[assert_size]=0;
   //std::cout<<"\t\t"<<output.c_str()<<std::endl;
   return output;
}

//Data as transmitted
template<typename T>
struct LVDATA {
   //LabVIEW formatted time... (128bit)
   //Note! LabVIEW timestamp is Big Endian...
   //(i64) seconds since the epoch 01/01/1904 00:00:00.00 UTC (using the Gregorian calendar and ignoring leap seconds),
   int64_t CoarseTime;
   //(u64) positive fractions of a second 
   uint64_t FineTime;
   T DATA[];
   uint32_t GetHeaderSize() const
   {
      return sizeof(CoarseTime)
             + sizeof(FineTime);
   }
   uint32_t GetEntries(uint32_t size) const
   {
      return (size-GetHeaderSize())/sizeof(T);
   }
   void print(uint32_t size, bool LittleEndian, bool IsString=false)
   {
      std::cout<<"   Coarse Time:"<<GetLabVIEWCoarseTime()<<std::endl;
      std::cout<<"   Unix Time:"<<GetUnixTimestamp()<<std::endl;;
      std::cout<<"   Fine Time:"<<GetLabVIEWFineTime()<<std::endl;
      uint32_t data_points=GetEntries(size);
      std::cout<<"   size:"<<data_points<<std::endl;
      if (IsString)
      {
         std::cout<<"DATA:";
         for (int i=0; i<data_points; i++)
         {
            if (DATA[i])
               std::cout<<DATA[i];
            else
               std::cout<<"NULL";
         }
         std::cout<<std::endl;
         return;
      }
      if (LittleEndian)
         for (int i=0; i<data_points; i++)
            if (DATA[i])
               std::cout<<"   DATA["<<i<<"]="<<DATA[i]<<std::endl;
            else
               std::cout<<"   DATA["<<i<<"]="<<"NULL"<<std::endl;
            
      else
         for (int i=0; i<data_points; i++)
            std::cout<<"   DATA["<<i<<"]="<<change_endian(DATA[i])<<std::endl;
   }
   //LabVIEW timestamp is Big Endian... convert when reading, store as orignal data (BigEndian)
   const int64_t GetLabVIEWCoarseTime() const
   {
      return change_endian(CoarseTime);
   }
   const uint64_t GetLabVIEWFineTime() const
   {
      return change_endian(FineTime);
   }
   const int64_t GetUnixTimestamp() const 
   {
      return change_endian(CoarseTime)-2082844800;
   }
};

//Data is held in memory on node
struct BANK_TITLE {
   char BANK[4]={0}; //LVB1
   char DATATYPE[4]={0}; //DBLE, UINT, INT6, INT3, INT1, INT8, CHAR
   char VARCATEGORY[16]={0};
   char VARNAME[16]={0};
   void print() const
   {
      printf("  BANK:%.4s\n",BANK);
      printf("  DATATYPE:%.4s\n",DATATYPE);
      printf("  Variable:%.16s/%.16s\n",SanitiseBankString(VARCATEGORY,sizeof(VARCATEGORY)).c_str(),SanitiseBankString(VARNAME,sizeof(VARNAME)).c_str());
   }
};
enum LVEndianess { BigEndian, NativeEndian, LittleEndian};
template<typename T>
struct LVBANK {
   BANK_TITLE NAME;
   char EquipmentType[32];
   uint32_t HistoryRate;
   uint32_t DataEndianess;
   uint32_t BlockSize;
   uint32_t NumberOfEntries;
   LVDATA<T> DATA[];
   void printheader() const
   {
      NAME.print();
      std::cout<<"  EquipmentType:"<<EquipmentType<<std::endl;
      std::cout<<"  HistoryRate:"<<HistoryRate<<std::endl;
      std::cout<<"  DataEndianess:"<<DataEndianess<<std::endl;
      std::cout<<"  BlockSize:"<<BlockSize<<std::endl;
      std::cout<<"  NumberOfEntries:"<<NumberOfEntries<<std::endl;
   }
   void print() const
   {
      printheader();
      bool IsString=false;
      if (strncmp(NAME.DATATYPE,"STR",3)==0)
         IsString=true;
      for (int i=0; i<NumberOfEntries; i++)
      {
         char* buf=(char*)&DATA;
         LVDATA<T>* data=(LVDATA<T>*)buf;
         data->print(BlockSize, (DataEndianess==LVEndianess::LittleEndian),IsString);
         buf+=BlockSize;
      }
   }
   std::string GetCategoryName() const
   {
      return SanitiseBankString(NAME.VARCATEGORY,sizeof(NAME.VARCATEGORY));
   }
   std::string GetVarName() const
   {
      return SanitiseBankString(NAME.VARNAME,sizeof(NAME.VARNAME));
   }
   uint32_t GetHeaderSize()
   {
      return sizeof(NAME)
             + sizeof(EquipmentType)
             + sizeof(HistoryRate)
             + sizeof(DataEndianess)
             + sizeof(BlockSize)
             + sizeof(NumberOfEntries);
   }
   uint32_t GetTotalSize()
   {
      return GetHeaderSize()+BlockSize*NumberOfEntries;
   }
   void ClearHeader()
   {
      //Char arrays are NULL terminated... so NULL the first character
      NAME.BANK[0]=0;
      NAME.DATATYPE[0]=0;
      NAME.VARCATEGORY[0]=0;
      NAME.VARNAME[0]=0;
      EquipmentType[0]=0;
      HistoryRate=0;
      DataEndianess=-1;
      BlockSize=-1;
      NumberOfEntries=-1;
   }
   const LVDATA<T>* GetFirstDataEntry() const
   {
      return &DATA[0];
   }
   const LVDATA<T>* GetLastDataEntry() const
   {
      char* ptr=(char*)&DATA;
      ptr+=BlockSize*(NumberOfEntries-1);
      return (LVDATA<T>*)ptr;
   }
   const int64_t GetFirstUnixTimestamp() const 
   {
      return GetFirstDataEntry()->GetUnixTimestamp();
   }
   const int64_t GetLastUnixTimestamp() const 
   {
      return GetLastDataEntry()->GetUnixTimestamp();
   }

};

struct LVBANKARRAY {
   char BANK[4];
   uint32_t BankArrayID;
   uint32_t BlockSize;
   uint32_t NumberOfEntries;
   //Block of data of unknown type;
   char* DATA[];
   uint32_t GetHeaderSize()
   {
      return sizeof(BANK)+sizeof(BankArrayID)+sizeof(BlockSize)+sizeof(NumberOfEntries);
   }
   uint32_t GetTotalSize()
   {
      //std::cout<<"Header size:"<<GetHeaderSize()<<std::endl;
      //std::cout<<"Block size: "<<BlockSize<<std::endl;
      return GetHeaderSize()+BlockSize;
   }
   void ClearHeader()
   {
      for (int i=0; i<4; i++)
         BANK[i]=0;
      BankArrayID=-1;
      BlockSize=-1;
      NumberOfEntries=-1;
   }
   void print()
   {
      printf("-------------------------\n");
      printf("BANK:%.4s\n",BANK);
      printf("BankArrayID:%u\n",BankArrayID);
      printf("BlockSize:%u\n",BlockSize);
      printf("NumberOfEntries:%u\n",NumberOfEntries);
      printf("-------------------------\n");
   }
};

#include "msystem.h"


class MessageHandler
{
   private:
      TMFE* fMfe;
      std::vector<std::string> MessageForLabVIEWQueue;
      std::vector<std::string> ErrorForLabVIEWQueue;
      int TotalText;
   public:
   MessageHandler(TMFE* mfe);
   ~MessageHandler();
   bool HaveErrors() {return ErrorForLabVIEWQueue.size();};
   void QueueData(const char* name, const char* msg, int length=-1);
   void QueueMessage(const char* msg);
   void QueueError(const char* err);
   std::string ReadMessageQueue();
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
   AllowedHosts(TMFE* mfe);
   void PrintRejection(TMFE* mfe,const char* hostname);
   bool IsAllowed(const char* hostname);
   bool IsWhiteListed(const char* hostname);
   //Allow this host:
   bool AddHost(const char* hostname);
   //Ban this host:
   bool BlackList(const char* hostname);
   private:
   bool IsBlackListed(const char* hostname);
   bool IsGreyListed(const char* hostname);
   
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
   HistoryVariable(const LVBANK<T>* lvbank, TMFE* mfe );
   template<typename T>
   bool IsMatch(const LVBANK<T>* lvbank);
   template<typename T>
   void Update(const LVBANK<T>* lvbank);
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
   HistoryLogger(TMFE* mfe,TMFeEquipment* eq);
   ~HistoryLogger();
   template<typename T>
   HistoryVariable* AddNewVariable(const LVBANK<T>* lvbank);
   template<typename T>
   HistoryVariable* Find(const LVBANK<T>* lvbank, bool AddIfNotFound);
   template<typename T>
   void Update(const LVBANK<T>* lvbank);
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
   PeriodicityManager(TMFE* mfe,TMFeEquipment* eq);
   const char* ProgramName(char* string);
   void AddRemoteCaller(char* prog);
   void LogPeriodicWithData();
   void LogPeriodicWithoutData();
   void UpdatePerodicity();
   void ProcessMessage(LVBANK<char>* bank);
};

class feLabVIEWClass :
   public TMFeRpcHandlerInterface,
   public TMFePeriodicHandlerInterface
{
public:
   TMFE* fMfe;
   TMFeEquipment* fEq;
   enum RunStatusType{Unknown,Running,Stopped};
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
   ~feLabVIEWClass() // dtor
   {
      if (fEventBuf) {
         free(fEventBuf);
         fEventBuf = NULL;
      }
   }
   virtual std::pair<int,bool> FindHostInWorkerList(const char* hostname) { assert(0); return {-1,false}; };
   virtual int AssignPortForWorker(uint workerID) { assert(0); return -1; };
   virtual const char* AddNewClient(const char* hostname) { assert(0); return NULL; };

   std::string HandleRpc(const char* cmd, const char* args);
   void HandleBeginRun();
   void HandleEndRun();
   void HandleStrBank(LVBANK<char>* bank);
   void LogBank(const char* buf);
   void HandleBankArray(char * ptr);
   void HandleBank(char * ptr);

   void HandlePeriodic();
};



class feLabVIEWWorker :
   public feLabVIEWClass
{
   public:

   feLabVIEWWorker(TMFE* mfe, TMFeEquipment* eq, AllowedHosts* hosts);
   void Init();

};

class feLabVIEWSupervisor :
   public feLabVIEWClass
{
public:
   MVOdb* fOdbWorkers;

   int fPortRangeStart;
   int fPortRangeStop;

   feLabVIEWSupervisor(TMFE* mfe, TMFeEquipment* eq);

   void Init();

   virtual std::pair<int,bool> FindHostInWorkerList(const char* hostname);
   virtual int AssignPortForWorker(uint workerID);
   virtual const char* AddNewClient(const char* hostname);
  
};