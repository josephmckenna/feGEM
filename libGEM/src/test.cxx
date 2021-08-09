#include "GEM_BANK.h"


struct TEST_DOUBLE_BANK {
   char BANK[4]={'T','E','S','T'}; //LVB1
   char DATATYPE[4]={'D','B','L','\0'}; //DBLE, UINT, INT6, INT3, INT1, INT8, CHAR
   char VARCATEGORY[16]="Category";
   char VARNAME[16]="Varname";
   char EquipmentType[32]="Equipment Type";
   
   uint16_t HistorySettings = 1;
   uint16_t HistoryPeriod = 2;
   uint16_t TimestampEndianness = LittleEndian;
   uint16_t DataEndianness = LittleEndian;
   uint32_t BlockSize = 8*10 + 4 + 4;
   uint32_t NumberOfEntries = 1;
   int64_t Seconds = 62167219200;
   //(u64) positive fractions of a second 
   uint64_t SubSecondFraction = 0;
   // 8 bytes x 10
   double Data[10] = { 0., 1., 2., 3., 4., 5., 6., 7., 8., 9. };
};


#include <assert.h>

int main()
{

   TEST_DOUBLE_BANK a;
   GEMBANK<double>* b = (GEMBANK<double>*)&a;

   assert(strncmp(a.BANK,b->GetBANK().c_str(),4)==0 );

   assert(strncmp(a.DATATYPE, b->GetType().c_str(),4)==0);

   assert(b->GetCategoryName().size() > 0);
   assert(strncmp(a.VARCATEGORY, b->GetCategoryName().c_str(), b->GetCategoryName().size() )==0);

   assert(b->GetVariableName().size() > 0);
   assert(strncmp(a.VARNAME, b->GetVariableName().c_str(), b->GetVariableName().size() )==0);
   
   assert(b->GetEquipmentType().size() > 0);
   //This test fails! The space is sanitised away! (Hence the 'not' at begin of assert)
   assert( not strncmp(a.EquipmentType, b->GetEquipmentType().c_str(),b->GetEquipmentType().size() ) ==0 );
   //This test passed
   assert(strncmp("EquipmentType", b->GetEquipmentType().c_str(),b->GetEquipmentType().size() ) ==0 );

   //This is only testing one case... perhaps we should test multiple variable lengths
   std::string CombinedName = std::string(a.VARCATEGORY) + std::string("\\") + std::string(a.VARNAME);
   assert (CombinedName == b->GetCombinedName() );
   
   assert( a.HistorySettings == b->HistorySettings);
   assert(a.HistoryPeriod == b->HistoryPeriod);
   assert(a.TimestampEndianness == b->TimestampEndianness);
   assert(a.DataEndianness == b->DataEndianness);
   assert(a.BlockSize == b->BlockSize);
   assert(a.NumberOfEntries == b->NumberOfEntries);

   assert(a.Seconds == b->GetFirstDataEntry()->timestamp.Seconds);
   assert(a.SubSecondFraction == b->GetFirstDataEntry()->timestamp.SubSecondFraction);

    std::cout<<"Tests passed!"<<std::endl;
    return 0;
}