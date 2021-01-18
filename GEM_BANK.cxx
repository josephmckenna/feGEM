
#include "GEM_BANK.h"

void PrintGEMBANK(GEMBANK<void*>* bank)
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

//Define all valid data types for GEMBANKs

template class GEMBANK<char>;
template class GEMBANK<void*>;
template class GEMBANK<double>;
