#include "HistoryLogger.h"
//--------------------------------------------------
// History Variable
// Hold link to ODB path of variable
// Monitor the last time we updated the ODB (only update at the periodicity specificed in GEMBANK)
//--------------------------------------------------


int HistoryVariable::gHistoryPeriod;

template<typename T> 
void HistoryVariable::BuildCPUMEMHistoryPlot(const GEMBANK<T>* GEM_bank, TMFE* mfe,TMFeEquipment* eq )
{
   std::cout<<"Building history entry for "<<eq->fName<<std::endl;
   assert(strncmp(GEM_bank->GetCategoryName().c_str(),"THISHOST",8)==0);
   assert(strncmp(GEM_bank->GetVariableName().c_str(),"CPUMEM",6)==0);
   //Insert myself into the history
   int size;
   int NVARS=GEM_bank->GetSizeOfDataArray();
   
   /////////////////////////////////////////////////////
   // Setup variables to plot:
   /////////////////////////////////////////////////////
   mfe->fOdbRoot->Chdir("History/Display/feGEM",true);
   MVOdb* OdbPlot = mfe->fOdbRoot->Chdir((std::string("History/Display/feGEM/") + eq->fName).c_str(), true);
   size = 64; // String length in ODB
   {
      std::vector<std::string> Variables;
      for (int i = 0; i < NVARS; i++)
      {
         std::string path=eq->fName + "/CPUMEM";
         if (path.size()>31)
            path = path.substr(0,31);
        Variables.push_back(
            path + ":CPUMEM[" + std::to_string(i) + "]"
         );
      }
      OdbPlot->WSA("Variables",Variables,size);
   }

   /////////////////////////////////////////////////////
   // Setup labels 
   /////////////////////////////////////////////////////
   std::vector<std::string> Labels;
   size = 32;
   {
      if (NVARS > 2)
      {
         for (int i = 0; i < NVARS - 1; i++)
         {
            Labels.push_back( std::string("CPU ") + std::to_string(i+1) + std::string(" Load (%)"));
         }
         Labels.push_back("Memory Usage (%)");
      }
      else
      {
         Labels.push_back("All CPU Load (%)");
         Labels.push_back("Memory Usage (%)");
      }
      OdbPlot->WSA("Label",Labels,size);
   }

   /////////////////////////////////////////////////////
   // Setup colours:
   /////////////////////////////////////////////////////
   size = 32;
   {
      const char *colourList[] = {
           "#00AAFF", "#FF9000", "#FF00A0", "#00C030",
           "#A0C0D0", "#D0A060", "#C04010", "#807060",
           "#F0C000", "#2090A0", "#D040D0", "#90B000",
           "#B0B040", "#B0B0FF", "#FFA0A0", "#A0FFA0",
           "#808080"};
      std::vector<std::string> colours;
      for (int i=0; i<NVARS; i++)
         colours.push_back(std::string(colourList[i%16]));
      OdbPlot->WSA("Colour",colours,size);
   }

   /////////////////////////////////////////////////////
   // Setup time scale and range:
   /////////////////////////////////////////////////////
   OdbPlot->WS("Timescale","1h");
   
   OdbPlot->WF("Minimum",0.);
   OdbPlot->WF("Maximum",100.);
}

template<typename T>
HistoryVariable::HistoryVariable(const GEMBANK<T>* GEM_bank, TMFE* mfe,TMFeEquipment* eq )
{
   fCategory=GEM_bank->GetCategoryName();
   fVarName=GEM_bank->GetVariableName();
   if (GEM_bank->HistoryPeriod!=65535)
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
      std::cout<<"Build THISHOST"<<std::endl;
      OdbEq = mfe->fOdbRoot->Chdir((std::string("Equipment/") + eq->fName).c_str(), true);
      if (strncmp(GEM_bank->GetVariableName().c_str(),"CPUMEM",6)==0)
      {
         std::cout<<"Build CPUMEM"<<std::endl;
         BuildCPUMEMHistoryPlot(GEM_bank, mfe,eq );
      }
      
   }
   else
   {
      OdbEq = mfe->fOdbRoot->Chdir((std::string("Equipment/") + fCategory).c_str(), true);
      //fEq = new TMFeEquipment(mfe,fCategory.c_str(),eq->fCommon);
      //fEq->Init();
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
   //std::cout <<data->GetUnixTimestamp(GEM_bank->TimestampEndianness) <<" <  " <<fLastUpdate + UpdateFrequency <<std::endl;
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

   HistoryVariable* variable = new HistoryVariable(GEM_bank,fMfe,fEq);

   //Push into list of monitored variables
   fVariables.push_back(variable);

   //Announce in control room new variable is logging
   char message[100];
   if (strncmp(GEM_bank->GetCategoryName().c_str(),"THISHOST",8)==0)
   {
      sprintf(
         message,
         "New variable [%s] from %s being logged (type %s)",
         GEM_bank->GetVariableName().c_str(),
         fEq->fName.c_str(), 
         GEM_bank->GetType().c_str()
         );
   }
   else
   {
      sprintf(
         message,
         "New variable [%s] in category [%s] being logged (type %s)",
         GEM_bank->GetVariableName().c_str(),
         GEM_bank->GetCategoryName().c_str(),
         GEM_bank->GetType().c_str()
         );
   }
   fMfe->Msg(MTALK, fEq->fName.c_str(), message);

   //Return pointer to this variable so the history can be updated by caller function
   return variable;
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
