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

enum LVEndianess { BigEndian, NativeEndian, LittleEndian};
template <class T>
T change_endian(T in)
{
    char* const p = reinterpret_cast<char*>(&in);
    for (size_t i = 0; i < sizeof(T) / 2; ++i)
        std::swap(p[i], p[sizeof(T) - i - 1]);
    return in;
}

class LVTimestamp {
   public:
      //LabVIEW formatted time... (128bit)
      //Note! LabVIEW timestamp is Big Endian...
      //(i64) seconds since the epoch 01/01/1904 00:00:00.00 UTC (using the Gregorian calendar and ignoring leap seconds),
      int64_t Seconds;
      //(u64) positive fractions of a second 
      uint64_t SubSecondFraction;
   public:
   LVTimestamp(bool Now=false)
   {
      if (!Now) return;

      using namespace std::chrono;

      system_clock::time_point tp = system_clock::now();
      system_clock::duration dtn = tp.time_since_epoch();
                
      Seconds=dtn.count() * system_clock::period::num / system_clock::period::den;//+2082844800;
      
      double fraction=(double)(dtn.count() - Seconds*system_clock::period::den / system_clock::period::num)/system_clock::period::den;
      SubSecondFraction=fraction*(double)((uint64_t)-1);
      //Convert from UNIX time (seconds since 1970) to LabVIEW time (seconds since 01/01/1904 )
      Seconds+=2082844800;

      //LabVIEW timestamp is big endian... conform...
      Seconds=change_endian(Seconds);
      SubSecondFraction=change_endian(SubSecondFraction);
      //print();
   }
   void print()
   {
      std::cout<<"LV Seconds:\t"<<Seconds<<std::endl;
      std::cout<<"Unix Seconds\t"<<Seconds-2082844800<<std::endl;
      std::cout<<"Subfrac:\t"<<SubSecondFraction<<std::endl;
   }
};

//Data as transmitted
template<typename T>
class LVDATA {
   public:
   //LabVIEW formatted time... (128bit)
   LVTimestamp timestamp;
   T DATA[];
   void print(uint32_t size, uint16_t TimestampEndianness, uint16_t DataEndianness, bool IsString);
   uint32_t GetHeaderSize() const            { return sizeof(timestamp); }
   uint32_t GetEntries(uint32_t size) const  { return (size-GetHeaderSize())/sizeof(T);      }
   
   //LabVIEW timestamp is Big Endian... convert when reading, store as orignal data (BigEndian)
   const int64_t GetLabVIEWCoarseTime(uint16_t Endianess) const 
   { 
      if (Endianess!=LittleEndian)
         return change_endian(timestamp.Seconds);
      else
         return timestamp.Seconds;
   }
   const uint64_t GetLabVIEWFineTime(uint16_t Endianess) const
   {
      if (Endianess!=LittleEndian)
         return change_endian(timestamp.SubSecondFraction);
      else
         return timestamp.SubSecondFraction;
   }
   const int64_t GetUnixTimestamp(uint16_t Endianess) const
   {
      if (Endianess!=LittleEndian)
         return change_endian(timestamp.Seconds)-2082844800;
      else
         return timestamp.Seconds-2082844800;
   }
};

//Data is held in memory on node
class BANK_TITLE {
   public:
   char BANK[4]={0}; //LVB1
   char DATATYPE[4]={0}; //DBLE, UINT, INT6, INT3, INT1, INT8, CHAR
   char VARCATEGORY[16]={0};
   char VARNAME[16]={0};
   char EquipmentType[32]={0};
   void print() const;
   std::string SanitiseBankString(const char* input, int assert_size=0) const;
   std::string GetCategoryName() const  { return SanitiseBankString(VARCATEGORY,sizeof(VARCATEGORY));     }
   std::string GetVariableName() const  { return SanitiseBankString(VARNAME,sizeof(VARNAME));             }
   std::string GetEquipmentType() const { return SanitiseBankString(EquipmentType,sizeof(EquipmentType)); }
};

template<typename T>
class LVBANK {
   public:
   BANK_TITLE NAME;
   uint16_t HistorySettings;
   uint16_t HistoryPeriod;
   uint16_t TimestampEndianness;
   uint16_t DataEndianness;
   uint32_t BlockSize;
   uint32_t NumberOfEntries;
   LVDATA<T> DATA[];

   void printheader() const;
   void print() const;
   std::string GetCategoryName() const  { return NAME.GetCategoryName();  }
   std::string GetVariableName() const  { return NAME.GetVariableName();  }
   std::string GetEquipmentType() const { return NAME.GetEquipmentType(); }
   uint32_t GetHeaderSize();
   uint32_t GetTotalSize(); //Size including header
   void ClearHeader();
   const LVDATA<T>* GetFirstDataEntry() const;
   const int64_t GetFirstUnixTimestamp() const { return GetFirstDataEntry()->GetUnixTimestamp(TimestampEndianness); }
   const LVDATA<T>* GetLastDataEntry() const;
   const int64_t GetLastUnixTimestamp() const  { return GetLastDataEntry()->GetUnixTimestamp(TimestampEndianness);  }
};

class LVBANKARRAY {
   public:
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
      return GetHeaderSize()+BlockSize;
   }
   void ClearHeader();
   void print();
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
      Host(const char* hostname);
      double TimeSince(std::chrono::time_point<std::chrono::system_clock> t);
      double TimeSinceLastContact();
      bool operator==(const char* hostname) const { return (strcmp(HostName.c_str(),hostname)==0); }
      bool operator==(const Host & rhs) const     { return HostName==rhs.HostName;                 }
      void print();
   };
   private:
   std::mutex list_lock;
   //Allowed hosts:
   std::vector<Host> white_list;
   //Allowed hosts in testing mode (no ODB operations)
   std::vector<Host> virtual_white_list;
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
   HistoryVariable(const LVBANK<T>* lvbank, TMFE* mfe,TMFeEquipment* eq );
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
   void WriteODB(std::vector<uint16_t>& data)
   {
      fOdbEqVariables->WU16A(fVarName.c_str(),data);
   }
   void WriteODB(std::vector<uint32_t>& data)
   {
      fOdbEqVariables->WU32A(fVarName.c_str(),data);
   }
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
   uint32_t RUN_START_T;
   uint32_t  RUN_STOP_T;
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
   void HandleStrBank(LVBANK<char>* bank,const char* hostname);
   void LogBank(const char* buf,const char* hostname);
   int HandleBankArray(const char * ptr,const char* hostname);
   int HandleBank(const char * ptr,const char* hostname);

   void HandlePeriodic();
};


int gHistoryPeriod;
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