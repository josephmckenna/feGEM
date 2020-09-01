#ifndef _GEM_BANK_
#define _GEM_BANK_

#include <vector>
#include <iostream>
#include <chrono>
#include <string.h>

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
class GEMDATA {
   public:
   //LabVIEW formatted time... (128bit)
   LVTimestamp timestamp;
   T DATA[];
   void print(uint32_t size, uint16_t TimestampEndianness, uint16_t DataEndianness, bool IsString) const;
   operator T*() { return &DATA[0]; }
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
   std::string GetType() const          { return SanitiseBankString(DATATYPE,4);                          }
   std::string GetCategoryName() const  { return SanitiseBankString(VARCATEGORY,sizeof(VARCATEGORY));     }
   std::string GetVariableName() const  { return SanitiseBankString(VARNAME,sizeof(VARNAME));             }
   std::string GetEquipmentType() const { return SanitiseBankString(EquipmentType,sizeof(EquipmentType)); }
};

template<typename T>
class GEMBANK {
   public:
   BANK_TITLE NAME;
   uint16_t HistorySettings;
   uint16_t HistoryPeriod;
   uint16_t TimestampEndianness;
   uint16_t DataEndianness;
   uint32_t BlockSize;
   uint32_t NumberOfEntries;
   GEMDATA<T> DATA[];

   void printheader() const;
   void print() const;
   std::string GetType() const          { return NAME.GetType();          }
   std::string GetCategoryName() const  { return NAME.GetCategoryName();  }
   std::string GetVariableName() const  { return NAME.GetVariableName();  }
   std::string GetEquipmentType() const { return NAME.GetEquipmentType(); }
   uint32_t GetHeaderSize();
   uint32_t GetTotalSize(); //Size including header
   void ClearHeader();

   const GEMDATA<T>* GetFirstDataEntry() const { return &DATA[0]; }
   const GEMDATA<T>* GetDataEntry(uint32_t i) const
   {
      char* ptr=(char*)&DATA;
      ptr+=BlockSize*(i);
      return (GEMDATA<T>*)ptr;
   }
   const GEMDATA<T>* GetLastDataEntry() const { return GetDataEntry(NumberOfEntries-1); }

   const int64_t GetFirstUnixTimestamp() const { return GetFirstDataEntry()->GetUnixTimestamp(TimestampEndianness); }
   const int64_t GetLastUnixTimestamp() const  { return GetLastDataEntry()->GetUnixTimestamp(TimestampEndianness);  }
};

class GEMBANKARRAY {
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

#endif
