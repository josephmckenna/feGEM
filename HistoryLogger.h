#ifndef _HISTORY_VARIABLE_
#define _HISTORY_VARIABLE_

#include <chrono>
#include "GEM_BANK.h"
#include "midas.h"
#include "tmfe.h"
#include "msystem.h"

class HistoryVariable
{
   public:
   std::string fCategory;
   std::string fVarName;
   int64_t fLastUpdate; //Converted to UXIXTime
   int UpdateFrequency;
   MVOdb* fOdbEqVariables; 
   template<typename T>
   HistoryVariable(const GEMBANK<T>* gembank, TMFE* mfe,TMFeEquipment* eq );
   template<typename T>
   bool IsMatch(const GEMBANK<T>* gembank);
   template<typename T>
   void Update(const GEMBANK<T>* gembank);
   private:
   void WriteODB(std::vector<bool>& data)
   {
      fOdbEqVariables->WBA(fVarName.c_str(),data);
   }
   void WriteODB(std::vector<int>& data)
   {
      fOdbEqVariables->WIA(fVarName.c_str(),data);
   }
   void WriteODB(std::vector<double>& data)
   {
      fOdbEqVariables->WDA(fVarName.c_str(),data);
   }
   void WriteODB(std::vector<float>& data)
   {
      fOdbEqVariables->WFA(fVarName.c_str(),data);
   }
   void WriteODB(std::vector<char>& data)
   {
      //Note: Char arrays not supported... but std::string is supported...
      const int data_entries=data.size();
      std::vector<std::string> array(data_entries);
      size_t max_size=0;
      for (int i=0; i<data_entries; i++)
      {
         array[i]=data[i];
         if (array[i].size()>max_size)
             max_size=array[i].size();
      }
      fOdbEqVariables->WSA(fVarName.c_str(),array,max_size);
   }
   void WriteODB(std::vector<uint16_t>& data)
   {
      fOdbEqVariables->WU16A(fVarName.c_str(),data);
   }
   void WriteODB(std::vector<uint32_t>& data)
   {
      fOdbEqVariables->WU32A(fVarName.c_str(),data);
   }
};

class HistoryLogger
{
public:
   TMFE* fMfe;
   TMFeEquipment* fEq;
   std::vector<HistoryVariable*> fVariables;
   HistoryLogger(TMFE* mfe,TMFeEquipment* eq);
   ~HistoryLogger();
   template<typename T>
   HistoryVariable* AddNewVariable(const GEMBANK<T>* gembank);
   template<typename T>
   HistoryVariable* Find(const GEMBANK<T>* gembank, bool AddIfNotFound);
   template<typename T>
   void Update(const GEMBANK<T>* gembank)
   {
      HistoryVariable* UpdateThis=Find(gembank,true);
      UpdateThis->Update(gembank);
   }
};

#endif