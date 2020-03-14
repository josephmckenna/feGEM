#include <chrono>  // for high_resolution_clock
#include <iostream>
#include <stdlib.h>
#include <math.h> 
#include<vector>
#include <stdlib.h>   
#include <cstring>
#include <deque>
#include <cstring>
#include <fstream>





//Data as transmitted
template<typename T>
struct LVDATA {
    int64_t CoarseTime;
    uint64_t FineTime;
    T DATA[];
    uint32_t GetEntries(uint32_t size)
    {
       return (size-16)/sizeof(T);
    }
    void print(uint32_t size)
    {
        std::cout<<"Coarse Time:"<<CoarseTime<<"\t\t"<<(unsigned long)&CoarseTime-(unsigned long)&CoarseTime<<std::endl;
        std::cout<<"Unix Time:"<<CoarseTime-2082844800<<std::endl;;
        std::cout<<"Fine Time:"<<FineTime<<"\t\t"<<(unsigned long)&FineTime-(unsigned long)&CoarseTime<<std::endl;
        uint32_t data_points=GetEntries(size);
        std::cout<<"size:"<<data_points<<std::endl;
        for (int i=0; i<data_points; i++)
           std::cout<<"DATA["<<i<<"]="<<DATA[i]<<"\t"<<(unsigned long)&DATA[i]-(unsigned long)&CoarseTime<<std::endl;
        //std::cout<<"Actual size:"<<get_actual_size()<<std::endl;
    }
};

//Data is held in memory on node
struct BANK_TITLE {
    char BANK[4]; //LVB1
    char DATATYPE[4]; //DBLE, UINT, INT6, INT3, INT1, INT8, CHAR
    char VARCATEGORY[16];
    char VARNAME[16];
    void print()
    {
        //std::cout<<"BANK:"<<NAME.BANK[0]<<NAME.BANK[1]<<NAME.BANK[2]<<NAME.BANK[3]<<std::endl;
        printf("BANK:%.4s\n",BANK);
        printf("DATATYPE:%.4s\n",DATATYPE);
        printf("Variable:%.16s/%.16s\n",VARCATEGORY,VARNAME);
        //std::cout<<"Variable:"<<NAME.VARCATEGORY<<"/"<<NAME.VARNAME<<std::endl;
        //std::cout<<"DATATYPE:"<<DATATYPE<<std::endl;
    }
};

template<typename T>
struct LVBANK {
   BANK_TITLE NAME;
   char EquipmentType[32];
   uint32_t HistoryRate;
   uint32_t other;
   uint32_t BlockSize;
   uint32_t NumberOfEntries;
   LVDATA<T> DATA[];
   void print()
   {
      NAME.print();
      std::cout<<"EquipmentType:"<<EquipmentType<<std::endl;
      std::cout<<"HistoryRate:"<<HistoryRate<<std::endl;
      std::cout<<"Other:"<<other<<std::endl;
      std::cout<<"BlockSize:"<<BlockSize<<std::endl;
      std::cout<<"NumberOfEntries:"<<NumberOfEntries<<std::endl;
      for (int i=0; i<NumberOfEntries; i++)
      {
         char* buf=(char*)&DATA;
         LVDATA<T>* data=(LVDATA<T>*)buf;
         data->print(BlockSize);
         buf+=BlockSize;
      }
   }
   uint64_t GetHeaderSize()
   {
      return sizeof(NAME)
             + sizeof(EquipmentType)
             + sizeof(HistoryRate)
             + sizeof(other)
             + sizeof(BlockSize)
             + sizeof(NumberOfEntries);
   }
   uint64_t GetSize()
   {
      return GetHeaderSize()+BlockSize*NumberOfEntries;
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
   void print()
   {
      printf("-------------------------\n");
      printf("BANK:%.4s\n",BANK);
      printf("BlockSize:%u\n",BlockSize);
      printf("NumberOfEntries:%u\n",NumberOfEntries);
      printf("-------------------------\n");
   }
};