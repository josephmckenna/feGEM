//--------------------------------------------------
// GEMDATA object
// A class to contain the raw data we want to log
// Contents: I have a timestamp and an array
//--------------------------------------------------
#include "GEM_BANK.h"

template<typename T>
void GEMDATA<T>::print(uint32_t size, uint16_t TimestampEndianness, uint16_t DataEndianness, bool IsString)  const
{
   std::cout<<"   Coarse Time:"<<GetLabVIEWCoarseTime(TimestampEndianness)<<std::endl;
   std::cout<<"   Unix Time:"<<GetUnixTimestamp(TimestampEndianness)<<std::endl;;
   std::cout<<"   Fine Time:"<<GetLabVIEWFineTime(TimestampEndianness)<<std::endl;
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
   if (DataEndianness != LittleEndian)
   {
      for (int i=0; i<data_points; i++)
      {
         std::cout<<"   DATA["<<i<<"]="<<change_endian(DATA[i])<<std::endl;
      }
   }
   else
   {
      for (int i=0; i<data_points; i++)
      {
         if (DATA[i])
            std::cout<<"   DATA["<<i<<"]="<<DATA[i]<<std::endl;
         else
            std::cout<<"   DATA["<<i<<"]="<<"NULL"<<std::endl;         
      }
   }
}


void BANK_TITLE::print() const
{
   printf("  BANK:%.4s\n",BANK);
   printf("  DATATYPE:%.4s\n",DATATYPE);
   printf("  Variable:%.16s/%.16s\n",GetCategoryName().c_str(),GetVariableName().c_str());
   printf("  EquipmentType:%.32s\n",GetEquipmentType().c_str());
}
std::string BANK_TITLE::SanitiseBankString(const char* input, int assert_size) const
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
void GEMBANK<T>::printheader() const
{
   NAME.print();
   std::cout<<"  HistorySettings:"<<HistorySettings<<std::endl;
   std::cout<<"  HistoryPeriod:"<<HistoryPeriod<<std::endl;
   std::cout<<"  TimestampEndianess"<<TimestampEndianness<<std::endl;
   std::cout<<"  DataEndianess:"<<DataEndianness<<std::endl;
   std::cout<<"  BlockSize:"<<BlockSize<<std::endl;
   std::cout<<"  NumberOfEntries:"<<NumberOfEntries<<std::endl;
}

template<typename T>
void GEMBANK<T>::print() const
{
   printheader();
   bool IsString=false;
   if (strncmp(NAME.DATATYPE,"STR",3)==0)
      IsString=true;
   for (int i=0; i<NumberOfEntries; i++)
   {
      char* buf=(char*)&DATA;
      GEMDATA<T>* data=(GEMDATA<T>*)buf;
      data->print(BlockSize, TimestampEndianness,DataEndianness,IsString);
      buf+=BlockSize;
   }
}

template<typename T>
uint32_t GEMBANK<T>::GetHeaderSize()
{
   return sizeof(NAME)
          + sizeof(HistorySettings)
          + sizeof(HistoryPeriod)
          + sizeof(TimestampEndianness)
          + sizeof(DataEndianness)
          + sizeof(BlockSize)
          + sizeof(NumberOfEntries);
}

template<typename T>
uint32_t GEMBANK<T>::GetTotalSize()
{
   return GetHeaderSize()+BlockSize*NumberOfEntries;
}

template<typename T>
void GEMBANK<T>::ClearHeader()
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

//--------------------------------------------------
// GEMBANKARRAY object
// A simple contaner to hold an array of GEMBANKs
// Contents:
//    BANK array ID number
//    Array of GEMBANKs
//    Size
//--------------------------------------------------

void GEMBANKARRAY::ClearHeader()
{
   for (int i=0; i<4; i++)
      BANK[i]=0;
   BankArrayID=-1;
   BlockSize=-1;
   NumberOfEntries=-1;
}

void GEMBANKARRAY::print()
{
   printf("-------------------------\n");
   printf("BANK:%.4s\n",BANK);
   printf("BankArrayID:%u\n",BankArrayID);
   printf("BlockSize:%u\n",BlockSize);
   printf("NumberOfEntries:%u\n",NumberOfEntries);
   printf("-------------------------\n");
}


//Define all valid data types for GEMBANKs

template class GEMBANK<char>;
template class GEMBANK<void*>;
template class GEMBANK<double>;
