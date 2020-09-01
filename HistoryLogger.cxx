#include "HistoryLogger.h"
//--------------------------------------------------
// History Variable
// Hold link to ODB path of variable
// Monitor the last time we updated the ODB (only update at the periodicity specificed in GEMBANK)
//--------------------------------------------------

int gHistoryPeriod;

template<typename T>
HistoryVariable::HistoryVariable(const GEMBANK<T>* GEM_bank, TMFE* mfe,TMFeEquipment* eq )
{
   fCategory=GEM_bank->GetCategoryName();
   fVarName=GEM_bank->GetVariableName();
   if (GEM_bank->HistorySettings!=65535)
      UpdateFrequency=GEM_bank->HistoryPeriod;
   else
      UpdateFrequency=gHistoryPeriod;
   if (!UpdateFrequency)
      return;
   fLastUpdate=0;
   //Prepare ODB entry for variable
   MVOdb* OdbEq = NULL;
   if (strncmp(GEM_bank->GetCategoryName().c_str(),"THISHOST",8)==0)
   {
      OdbEq = mfe->fOdbRoot->Chdir((std::string("Equipment/") + eq->fName).c_str(), true);
   }
   else
   {
      OdbEq = mfe->fOdbRoot->Chdir((std::string("Equipment/") + fCategory).c_str(), true);
   }
   fOdbEqVariables  = OdbEq->Chdir("Variables", true);
}

template<typename T>
bool HistoryVariable::IsMatch(const GEMBANK<T>* GEM_bank)
{
   if (strcmp(fCategory.c_str(),GEM_bank->GetCategoryName().c_str())!=0)
      return false;
   if (strcmp(fVarName.c_str(),GEM_bank->GetVariableName().c_str())!=0)
      return false;
   return true;
}
template<typename T>
void HistoryVariable::Update(const GEMBANK<T>* GEM_bank)
{
   if (!UpdateFrequency)
      return;
   
   const GEMDATA<T>* data=GEM_bank->GetLastDataEntry();
   //std::cout <<data->GetUnixTimestamp() <<" <  " <<fLastUpdate + UpdateFrequency <<std::endl;
   if (data->GetUnixTimestamp(GEM_bank->TimestampEndianness) < fLastUpdate + UpdateFrequency)
      return;
   fLastUpdate=data->GetUnixTimestamp(GEM_bank->TimestampEndianness);
   const int data_entries=data->GetEntries(GEM_bank->BlockSize);
   std::vector<T> array(data_entries);
   for (int i=0; i<data_entries; i++)
      array[i]=data->DATA[i];
   WriteODB(array);
}

template void HistoryVariable::Update(const GEMBANK<double>* GEM_bank);
template void HistoryVariable::Update(const GEMBANK<bool>* GEM_bank);
template void HistoryVariable::Update(const GEMBANK<float>* GEM_bank);
template void HistoryVariable::Update(const GEMBANK<int>* GEM_bank);
template void HistoryVariable::Update(const GEMBANK<unsigned int>* GEM_bank);
template void HistoryVariable::Update(const GEMBANK<unsigned short>* GEM_bank);
template void HistoryVariable::Update(const GEMBANK<char>* GEM_bank);

//--------------------------------------------------
// History Logger Class
// Hold array of unique HistoryVariables 
//--------------------------------------------------

HistoryLogger::HistoryLogger(TMFE* mfe,TMFeEquipment* eq)
{
   fEq=eq;
   fMfe=mfe;
}

HistoryLogger::~HistoryLogger()
{
   //I do not own fMfe or fEq
   for (auto* var: fVariables)
      delete var;
   fVariables.clear();
}

template<typename T>
HistoryVariable* HistoryLogger::AddNewVariable(const GEMBANK<T>* GEM_bank)
{
   //Assert that category and var name are null terminated
   //assert(GEM_bank->NAME.VARCATEGORY[15]==0);
   //assert(GEM_bank->NAME.VARNAME[15]==0);
   
   //Store list of logged variables in Equipment settings
   char VarAndCategory[32];
   sprintf(VarAndCategory,"%s/%s",
                    GEM_bank->GetCategoryName().c_str(),
                    GEM_bank->GetVariableName().c_str());
   fEq->fOdbEqSettings->WSAI("feVariables",fVariables.size(), VarAndCategory);
   fEq->fOdbEqSettings->WU32AI("DateAdded",(int)fVariables.size(), GEM_bank->GetFirstUnixTimestamp());
   
   //Push into list of monitored variables
   fVariables.push_back(new HistoryVariable(GEM_bank,fMfe,fEq));
   //Announce in control room new variable is logging
   char message[100];
   sprintf(message,"New variable [%s] in category [%s] being logged (type %s)",GEM_bank->GetVariableName().c_str(),GEM_bank->GetCategoryName().c_str(), GEM_bank->GetType().c_str());
   fMfe->Msg(MTALK, fEq->fName.c_str(), message);
   //Return pointer to this variable so the history can be updated by caller function
   return fVariables.back();
}

template<typename T>
HistoryVariable* HistoryLogger::Find(const GEMBANK<T>* GEM_bank, bool AddIfNotFound=true)
{
   HistoryVariable* FindThis=NULL;
   //Find HistoryVariable that matches 
   for (auto var: fVariables)
   {
      if (var->IsMatch(GEM_bank))
      {
         FindThis=var;
         break;
      }
   }
   //If no match found... create one
   if (!FindThis && AddIfNotFound)
   {
      FindThis=AddNewVariable(GEM_bank);
   }
   return FindThis;
}


template HistoryVariable* HistoryLogger::Find(const GEMBANK<char>* GEM_bank, bool AddIfNotFound);
template HistoryVariable* HistoryLogger::Find(const GEMBANK<double>* GEM_bank, bool AddIfNotFound);
template HistoryVariable* HistoryLogger::Find(const GEMBANK<bool>* GEM_bank, bool AddIfNotFound);
template HistoryVariable* HistoryLogger::Find(const GEMBANK<float>* GEM_bank, bool AddIfNotFound);
template HistoryVariable* HistoryLogger::Find(const GEMBANK<int>* GEM_bank, bool AddIfNotFound);
template HistoryVariable* HistoryLogger::Find(const GEMBANK<unsigned int>* GEM_bank, bool AddIfNotFound);
template HistoryVariable* HistoryLogger::Find(const GEMBANK<unsigned short>* GEM_bank, bool AddIfNotFound);
