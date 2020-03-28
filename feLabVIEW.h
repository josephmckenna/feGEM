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

template <class T>
T change_endian(T in)
{
    char* const p = reinterpret_cast<char*>(&in);
    for (size_t i = 0; i < sizeof(T) / 2; ++i)
        std::swap(p[i], p[sizeof(T) - i - 1]);
    return in;
}

std::string sanitiseString(const char* input, int assert_size=0)
{
   
   const int input_size=strlen(input);
   int output_size=input_size;
   if (assert_size)
      output_size=assert_size;
   std::string output;
   output.resize(output_size);
   
   int i,j;
   for (i = 0, j = 0; i<input_size; i++,j++)
   {
      if (isalnum(input[i]))
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
   void print(uint32_t size, bool LittleEndian)
   {
      std::cout<<"   Coarse Time:"<<GetLabVIEWCoarseTime()<<std::endl;
      std::cout<<"   Unix Time:"<<GetUnixTimestamp()<<std::endl;;
      std::cout<<"   Fine Time:"<<GetLabVIEWFineTime()<<std::endl;
      uint32_t data_points=GetEntries(size);
      std::cout<<"   size:"<<data_points<<std::endl;
      if (LittleEndian)
         for (int i=0; i<data_points; i++)
            std::cout<<"   DATA["<<i<<"]="<<DATA[i]<<std::endl;
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
      printf("  Variable:%.16s/%.16s\n",sanitiseString(VARCATEGORY,sizeof(VARCATEGORY)).c_str(),sanitiseString(VARNAME,sizeof(VARNAME)).c_str());
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
   void print() const
   {
      NAME.print();
      std::cout<<"  EquipmentType:"<<EquipmentType<<std::endl;
      std::cout<<"  HistoryRate:"<<HistoryRate<<std::endl;
      std::cout<<"  DataEndianess:"<<DataEndianess<<std::endl;
      std::cout<<"  BlockSize:"<<BlockSize<<std::endl;
      std::cout<<"  NumberOfEntries:"<<NumberOfEntries<<std::endl;
      for (int i=0; i<NumberOfEntries; i++)
      {
         char* buf=(char*)&DATA;
         LVDATA<T>* data=(LVDATA<T>*)buf;
         data->print(BlockSize, (DataEndianess==LVEndianess::LittleEndian));
         buf+=BlockSize;
      }
   }
   std::string GetCategoryName() const
   {
      return sanitiseString(NAME.VARCATEGORY,sizeof(NAME.VARCATEGORY));
   }
   std::string GetVarName() const
   {
      return sanitiseString(NAME.VARNAME,sizeof(NAME.VARNAME));
   }
   uint64_t GetHeaderSize()
   {
      return sizeof(NAME)
             + sizeof(EquipmentType)
             + sizeof(HistoryRate)
             + sizeof(DataEndianess)
             + sizeof(BlockSize)
             + sizeof(NumberOfEntries);
   }
   uint64_t GetTotalSize()
   {
      return GetHeaderSize()+BlockSize*NumberOfEntries;
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
   char PAD[4];
   uint32_t BlockSize;
   uint32_t NumberOfEntries;
   //Block of data of unknown type;
   char* DATA[];
   uint32_t GetHeaderSize()
   {
      return sizeof(BANK)+sizeof(PAD)+sizeof(BlockSize)+sizeof(NumberOfEntries);
   }
   uint32_t GetTotalSize()
   {
      //std::cout<<"Header size:"<<GetHeaderSize()<<std::endl;
      //std::cout<<"Block size: "<<BlockSize<<std::endl;
      return GetHeaderSize()+BlockSize;
   }
   void print()
   {
      printf("-------------------------\n");
      printf("BANK:%.4s\n",BANK);
      printf("BlockSize:%u\n",BlockSize);
      printf("NumberOfEntries:%u\n",NumberOfEntries);
      printf("-------------------------\n");
   }
};