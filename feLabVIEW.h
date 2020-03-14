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
char* Copy(char* destination, char* buf, int size)
{
   memcpy(destination,buf,size);
   buf+=size;
   return buf;
}


//Data as transmitted
template<typename T>
class LVDATA {
    public:

    int64_t CoarseTime;
    uint64_t FineTime;
    std::vector<T> DATA;
    LVDATA(int64_t _CoarseTime, uint64_t _FineTime, T* array, int array_size)
    {
        CoarseTime=_CoarseTime;
        FineTime  =_FineTime;
        DATA.resize(array_size);
        for (int i=0; i<array_size; i++)
           DATA.at(i)=array[i];
    }
    LVDATA(char* buf, uint32_t BlockSize)
    {
        buf=Copy((char*)&CoarseTime,buf,sizeof(CoarseTime));
        buf=Copy((char*)&FineTime,buf,sizeof(FineTime));
        //Block size include the 16 bytes for the timestamp
        std::cout<<"Resizing to block size:"<<BlockSize-16<<std::endl;
        DATA.resize((BlockSize-16)/sizeof(T));
        for (auto & point: DATA)
        {
           buf=Copy((char*)&point, buf, sizeof(T));
        }
    }
    void print()
    {
        std::cout<<"Coarse Time:"<<CoarseTime<<"\t\t"<<(unsigned long)&CoarseTime-(unsigned long)&CoarseTime<<std::endl;
        std::cout<<"Unix Time:"<<CoarseTime-2082844800<<std::endl;;
        std::cout<<"Fine Time:"<<FineTime<<"\t\t"<<(unsigned long)&FineTime-(unsigned long)&CoarseTime<<std::endl;
        std::cout<<"size:"<<DATA.size()<<"\t\t"<<(unsigned long)&DATA.back()-(unsigned long)&CoarseTime<<std::endl;
        for (int i=0; i<DATA.size(); i++)
           std::cout<<"DATA["<<i<<"]="<<DATA[i]<<"\t"<<(unsigned long)&DATA[i]-(unsigned long)&CoarseTime<<std::endl;
    }


};



//Data is held in memory on node
class BANK_TITLE {
    public:
    char BANK[4]; //LVB1
    char DATATYPE[4]; //DBLE, UINT, INT6, INT3, INT1, INT8, CHAR
    char VARCATEGORY[16];
    char VARNAME[16];
    BANK_TITLE()
    {
        sprintf(BANK,"LVB1");
    }

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
class LVBANK {
   public:
    BANK_TITLE NAME;
    char EquipmentType[32];
    uint32_t HistoryRate;
    uint32_t other;
    uint32_t BlockSize;
    uint32_t NumberOfEntries;
    std::vector<LVDATA<T>*> DATA;
    LVBANK<T>()
    {
        HistoryRate=0; //Default no history logging :)
        NumberOfEntries=0;
    }
    LVBANK<T>(const char* Category, const char* Name, int PlotRate): LVBANK()
    {
       sprintf(NAME.VARCATEGORY,Category);
       sprintf(NAME.VARNAME,Name);
       sprintf(NAME.DATATYPE,"INT3");
       sprintf(EquipmentType,"LabVIEWHistory");
       HistoryRate=PlotRate;
    }

    LVBANK<T>(char* buf)
    {

       buf=Copy((char*)&NAME,buf,sizeof(NAME));
       buf=Copy((char*)&EquipmentType,buf,sizeof(EquipmentType));
       buf=Copy((char*)&HistoryRate,buf,sizeof(HistoryRate));
       buf=Copy((char*)&other,buf,sizeof(other));
       buf=Copy((char*)&BlockSize,buf,sizeof(BlockSize));
       buf=Copy((char*)&NumberOfEntries,buf,sizeof(NumberOfEntries));
       print();
       for (int i=0; i<NumberOfEntries; i++)
       {
          DATA.push_back(new LVDATA<T>(buf,BlockSize/NumberOfEntries));
          buf+=BlockSize;
       }
    }
    void AddData(unsigned long long NTS, unsigned long long LVTS,T* array, int array_size)
    {
        DATA.push_back(new LVDATA<T>(NTS,LVTS,array, array_size));
    }
    void print()
    {
        //BANK is
        NAME.print();
        
        std::cout<<"EquipmentType:"<<EquipmentType<<std::endl;
        std::cout<<"HistoryRate:"<<HistoryRate<<std::endl;
        std::cout<<"Other:"<<other<<std::endl;
        std::cout<<"BlockSize:"<<BlockSize<<std::endl;
        std::cout<<"NumberOfEntries:"<<NumberOfEntries<<std::endl;
        std::cout<<"size:"<<DATA.size()<<std::endl;
        for (int i=0; i<DATA.size(); i++)
           DATA.at(i)->print();
           //std::cout<<"LVDATA["<<i<<"]="<<DATA[i]<<"\t"<<(unsigned long)&DATA[i]-(unsigned long)&NAME.BANK[0]<<std::endl;

    }


};
//Instantiate all types for LVBANK?
/*template class LVBANK<char>;
template class LVBANK<uint16_t>;
template class LVBANK<uint32_t>;
template class LVBANK<uint64_t>;
template class LVBANK<double>;*/
