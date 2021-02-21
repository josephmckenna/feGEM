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
   LVTimestamp(const LVTimestamp& ts)
   {
      Seconds = ts.Seconds;
      SubSecondFraction = ts.SubSecondFraction;
   }
   LVTimestamp& operator=(const LVTimestamp& ts)
   {
      Seconds = ts.Seconds;
      SubSecondFraction = ts.SubSecondFraction;
      return *this;
   }
   ~LVTimestamp()
   {
      //dtor
   }
   void print()
   {
      std::cout<<"LV Seconds:\t"<<Seconds<<std::endl;
      std::cout<<"Unix Seconds\t"<<Seconds-2082844800<<std::endl;
      std::cout<<"Subfrac:\t"<<SubSecondFraction<<std::endl;
   }
};

//--------------------------------------------------
// GEMDATA object
// A class to contain the raw data we want to log
// Contents: I have a timestamp and an array
//--------------------------------------------------

//Data as transmitted
template<typename T>
class GEMDATA {
   public:
   //LabVIEW formatted time... (128bit)
   LVTimestamp timestamp;
   T DATA[];
   
   void print(uint32_t size, uint16_t TimestampEndianness, uint16_t DataEndianness, bool IsString) const
   {
      std::cout<<"   Coarse Time:"<<GetLabVIEWCoarseTime(TimestampEndianness)<<std::endl;
      std::cout<<"   Unix Time:"<<GetUnixTimestamp(TimestampEndianness)<<std::endl;;
      std::cout<<"   Fine Time:"<<GetLabVIEWFineTime(TimestampEndianness)<<std::endl;
      uint32_t data_points=GetEntries(size);
      std::cout<<"   size:"<<data_points<<std::endl;
      if (IsString)
      {
         std::cout<<"DATA:";
         for (uint32_t i=0; i<data_points; i++)
         {
            if (DATA[i])
               std::cout<<DATA[i];
            else
               std::cout<<"NULL";
         }
         std::cout<<std::endl;
         return;
      }
      if (DataEndianness != LittleEndian)
      {
         for (uint32_t i=0; i<data_points; i++)
            std::cout<<"   DATA["<<i<<"]="<<change_endian(DATA[i])<<std::endl;
      }
      else
      {
         for (uint32_t i=0; i<data_points; i++)
         {
            if (DATA[i])
               std::cout<<"   DATA["<<i<<"]="<<DATA[i]<<std::endl;
            else
               std::cout<<"   DATA["<<i<<"]="<<"NULL"<<std::endl;         
         }
      }
   }
   operator T*() { return &DATA[0]; }
   uint32_t GetHeaderSize() const            { return sizeof(timestamp); }
   uint32_t GetEntries(uint32_t size) const  { return (size-GetHeaderSize())/sizeof(T);      }
   
   //LabVIEW timestamp is Big Endian... convert when reading, store as orignal data (BigEndian)
   int64_t GetLabVIEWCoarseTime(uint16_t Endianess) const 
   { 
      if (Endianess!=LittleEndian)
         return change_endian(timestamp.Seconds);
      else
         return timestamp.Seconds;
   }
   uint64_t GetLabVIEWFineTime(uint16_t Endianess) const
   {
      if (Endianess!=LittleEndian)
         return change_endian(timestamp.SubSecondFraction);
      else
         return timestamp.SubSecondFraction;
   }
   int64_t GetUnixTimestamp(uint16_t Endianess) const
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
   void print() const
   {
      printf("  BANK:%.4s\n",BANK);
      printf("  DATATYPE:%.4s\n",DATATYPE);
      printf("  Variable:%.16s/%.16s\n",GetCategoryName().c_str(),GetVariableName().c_str());
      printf("  EquipmentType:%.32s\n",GetEquipmentType().c_str());
   }
   std::string SanitiseBankString(const char* input, int assert_size=0) const
   {
      //std::cout<<input;
      const int input_size=strlen(input);
      int output_size = input_size + 1;
      if (assert_size && assert_size < input_size)
         output_size = assert_size;
      char output[output_size];
      //Clean all output chars NULL to start
      for (int i = 0; i<output_size; i++)
         output[i]=0;
      
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
      //std::cout<<output.c_str()<<"|"<<output.size()<<std::endl;
      std::string returnstring(output);
      return returnstring;
   }  

   const std::string GetBANK() const          { return SanitiseBankString(BANK,4);                              }
   const std::string GetType() const          { return SanitiseBankString(DATATYPE,4);                          }
   const std::string GetCategoryName() const  { return SanitiseBankString(VARCATEGORY,sizeof(VARCATEGORY));     }
   const std::string GetVariableName() const  { return SanitiseBankString(VARNAME,sizeof(VARNAME));             }
   const std::string GetEquipmentType() const { return SanitiseBankString(EquipmentType,sizeof(EquipmentType)); }
   const std::string GetCombinedName() const
   {
      if (strncmp(VARCATEGORY,"THISHOST",8)==0)
         return GetEquipmentType() + "\\" + GetVariableName();
      else
         return GetCategoryName() + "\\" + GetVariableName();
   }
   
};
static_assert(sizeof(BANK_TITLE)==72,"BANK_TITLE must be 72 bytes... compiler issues likely");

//--------------------------------------------------
// GEMBANK object
// A class to contain a bundle of GEMDATA objects
// Contents:
//    Variable, Category and Equipment names
//    The rate to log to history
//    The endianess of the GEMDATA data
//    Size
//--------------------------------------------------

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
   
   void printheader() const
   {
      NAME.print();
      std::cout<<"  HistorySettings:"<<HistorySettings<<std::endl;
      std::cout<<"  HistoryPeriod:"<<HistoryPeriod<<std::endl;
      std::cout<<"  TimestampEndianess"<<TimestampEndianness<<std::endl;
      std::cout<<"  DataEndianess:"<<DataEndianness<<std::endl;
      std::cout<<"  BlockSize:"<<BlockSize<<std::endl;
      std::cout<<"  NumberOfEntries:"<<NumberOfEntries<<std::endl;
   }

   void print() const
   {
      printheader();
      bool IsString=false;
      if (strncmp(NAME.DATATYPE,"STR",3)==0)
         IsString=true;
      for (uint32_t i=0; i<NumberOfEntries; i++)
      {
         const GEMDATA<T>* data=GetDataEntry(i);
         data->print(BlockSize, TimestampEndianness,DataEndianness,IsString);
      }
   }

   std::string GetBANK() const          { return NAME.GetBANK();          }
   std::string GetType() const          { return NAME.GetType();          }
   std::string GetCategoryName() const  { return NAME.GetCategoryName();  }
   std::string GetVariableName() const  { return NAME.GetVariableName();  }
   std::string GetEquipmentType() const { return NAME.GetEquipmentType(); }
   std::string GetCombinedName() const  { return NAME.GetCombinedName();  }
   uint32_t GetHeaderSize()
   {
      return sizeof(NAME)
         + sizeof(HistorySettings)
         + sizeof(HistoryPeriod)
         + sizeof(TimestampEndianness)
         + sizeof(DataEndianness)
         + sizeof(BlockSize)
         + sizeof(NumberOfEntries);
   }

   uint32_t GetTotalSize() //Size including header
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
      NAME.EquipmentType[0]=0;
      HistorySettings=0;
      HistoryPeriod=0;
      TimestampEndianness=-1;
      DataEndianness=-1;
      BlockSize=-1;
      NumberOfEntries=-1;
   }

   const GEMDATA<T>* GetFirstDataEntry() const { return (GEMDATA<T>*)this + sizeof(GEMBANK); }
   
   uint32_t GetSizeOfDataArray() const
   {
     return GetFirstDataEntry()->GetEntries(BlockSize);
   }
   
   
   const GEMDATA<T>* GetDataEntry(uint32_t i) const
   {
      char* ptr = (char*)this + sizeof(GEMBANK);
      ptr += BlockSize*(i);
      return (GEMDATA<T>*)ptr;
   }
   const GEMDATA<T>* GetLastDataEntry() const { return GetDataEntry(NumberOfEntries-1); }

   int64_t GetFirstUnixTimestamp() const { return GetFirstDataEntry()->GetUnixTimestamp(TimestampEndianness); }
   int64_t GetLastUnixTimestamp() const  { return GetLastDataEntry()->GetUnixTimestamp(TimestampEndianness);  }
};
static_assert(sizeof(GEMBANK<void*>)==88,"BANKBANK must be 88 bytes... compiler issues likely");

//Call the correct print based on type
static inline void PrintGEMBANK(GEMBANK<void*>* bank)
{
   if (strncmp(bank->NAME.DATATYPE,"DBL",3)==0) 
      ((GEMBANK<double>*)bank)->print();
   else if (strncmp(bank->NAME.DATATYPE,"FLT",3)==0)
      ((GEMBANK<float>*)bank)->print();
   else if (strncmp(bank->NAME.DATATYPE,"BOOL",4)==0)
      ((GEMBANK<bool>*)bank)->print();
   else if (strncmp(bank->NAME.DATATYPE,"I32",3)==0)
      ((GEMBANK<int32_t>*)bank)->print();
   else if (strncmp(bank->NAME.DATATYPE,"U32",4)==0)
      ((GEMBANK<uint32_t>*)bank)->print();
   else if (strncmp(bank->NAME.DATATYPE,"U16",3)==0)
      ((GEMBANK<uint16_t>*)bank)->print();
   else if (strncmp(bank->NAME.DATATYPE,"STRA",4)==0)
   {
      std::cout<<"Printing of string array probably wobbly"<<std::endl;
      ((GEMBANK<char>*)bank)->print();
   }
   else if (strncmp(bank->NAME.DATATYPE,"STR",3)==0) 
      ((GEMBANK<char>*)bank)->print();
   else
   {
      std::cout<<"Unknown bank data type... "<<std::endl;
      bank->print();
   }
}
//--------------------------------------------------
// GEMBANKARRAY object
// A simple contaner to hold an array of GEMBANKs
// Contents:
//    BANK array ID number
//    Array of GEMBANKs
//    Size
//--------------------------------------------------

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


#endif
