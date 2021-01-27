#ifndef _GEMEVENT_
#define _GEMEVENT_
// TObject class to write a feGEM BANK to a ROOT tree
#include "GEM_BANK.h"
//ROOT headers:
#include "TObject.h"
//std lib
#include "assert.h"
class TStoreGEMEventHeader: public TObject
{
    private:
       //Maybe make all this const...
       std::string BANK;
       std::string DATATYPE;
       std::string VARCATEGORY;
       std::string VARNAME;
       std::string EquipmentType;
       uint16_t HistorySettings;
       uint16_t HistoryPeriod;
       uint16_t TimestampEndianness;
       uint16_t DataEndianness;
       uint32_t BlockSize;
       uint32_t NumberOfEntries;
   public:
   TStoreGEMEventHeader();
   TStoreGEMEventHeader(GEMBANK<void*> *bank);
   virtual ~TStoreGEMEventHeader();
   ClassDef(TStoreGEMEventHeader,1)
};

class TLVTimestamp: public TObject
{

   public:
      //LabVIEW formatted time... (128bit)
      //Note! LabVIEW timestamp is Big Endian...
      //(i64) seconds since the epoch 01/01/1904 00:00:00.00 UTC (using the Gregorian calendar and ignoring leap seconds),
      int64_t Seconds;
      //(u64) positive fractions of a second 
      uint64_t SubSecondFraction;
   public:
   TLVTimestamp(bool Now=false);
   virtual ~TLVTimestamp();
   TLVTimestamp(const TLVTimestamp& ts);
   TLVTimestamp& operator=(const LVTimestamp& ts);
   TLVTimestamp& operator=(const TLVTimestamp& ts);
   ClassDef(TLVTimestamp,1);
};

template <class T>
class TStoreGEMData: public TObject
{
    private:
       //Variable order hasn't been memory layout optimised
       TLVTimestamp RawLabVIEWtimestamp;
       double RawLabVIEWAsUNIXTime;
       uint16_t TimestampEndianness;
       uint16_t DataEndianness;
       uint32_t MIDASTime;
       int runNumber;
       double RunTime;
       std::vector<T> data;
    public:
    TStoreGEMData();
    void Set(const GEMDATA<T>* gemdata,const int BlockSize,
       const uint16_t _TimestampEndianness, const uint16_t _DataEndianness,
       const uint32_t _MIDASTime, const double _RunTime, const int runNumber);
    double GetLVTimestamp() const { return RawLabVIEWAsUNIXTime; }
    double GetRunTime() const { return RunTime; }
    int GetRunNumber() const { return runNumber; }
    T GetArrayEntry(int i) const { return data.at(i); }
    virtual ~TStoreGEMData();
    ClassDef(TStoreGEMData<T>,1);
};



#endif