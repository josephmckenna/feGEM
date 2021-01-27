//
// feGEM analyzer module for rootana's manalzer
//
// JTK McKENNA
//

#include <stdio.h>

#include "GEM_BANK.h"
#include "GEM_BANK_flow.h"

#include "TTree.h"

#include "manalyzer.h"
#include "midasio.h"

#include <iostream>
class feGEMModuleFlags
{
public:
   bool fPrint = false;
};


#include "TStoreGEMEvent.h"
class feGEMModuleWriter
{
   private:
      TTree* headertree;
      TBranch* headerbranch;
      std::vector<TTree*> datatrees;
      std::vector<TBranch*> data;
      double RunTimeOffset;
   private:
      template <typename T> void BranchTreeFromData(TTree* t, TBranch* b, GEMBANK<T>* bank, uint32_t MIDAS_TIME, const std::string& name)
      {
         //TStoreGEMEventHeader GEMEvents;
         TStoreGEMData<T>* data = new TStoreGEMData<T>();
         
         b->SetAddress(&data);

         for (int i=0; i<bank->NumberOfEntries; i++)
         {
            data->Set(
               bank->GetDataEntry(i),
               bank->BlockSize,
               bank->TimestampEndianness,
               bank->DataEndianness,
               MIDAS_TIME,
               RunTimeOffset
               );
            //gemdata->print(BlockSize, TimestampEndianness,DataEndianness,IsString);
            t->Fill();
         }
         delete data;
      }
      template<typename T>
      std::pair<TTree*,TBranch*> FindOrCreateTree(TARunInfo* runinfo, GEMBANK<T>* bank, const std::string& CombinedName)
      {
         int numberoftrees = datatrees.size();

         for(int i=0; i<numberoftrees; i++)
         {
            std::string treename = datatrees[i]->GetName();
            if(treename == CombinedName)
            {
               //Tree exists
               return {datatrees[i],data[i]};
            }
         }
         runinfo->fRoot->fOutputFile->cd("feGEM");
         TTree* currentTree = new TTree(CombinedName.c_str(),"feGEM Event Tree");
         TStoreGEMData<T> event;

         std::string BranchName = "TStoreGEMData<";
            if (typeid(T) == typeid(double))
               BranchName += "double>";
            else if (typeid(T) == typeid(float))
               BranchName += "float>";
            else if (typeid(T) == typeid(bool))
               BranchName += "bool>";
            else if (typeid(T) == typeid(int32_t))
               BranchName += "int32_t>";
            else if (typeid(T) == typeid(uint32_t))
               BranchName += "uint32_t>";
            else if (typeid(T) == typeid(uint16_t))
               BranchName += "uint16_t>";
            else if (typeid(T) == typeid(char))
               BranchName += "char>";
            else
               BranchName += "unknown>";
         TBranch* branch = currentTree->Branch(BranchName.c_str(),"TStoreGEMData",&event);
         datatrees.push_back(currentTree);
         data.push_back(branch);
         return {currentTree, branch};
      }
      public:
      feGEMModuleWriter(double offset = 0)
      {
         RunTimeOffset = offset;
      }
      

      void SaveToTree(TARunInfo* runinfo, GEMBANK<void*>* bank, uint32_t MIDAS_TIME)
      {
         
         std::string CombinedName = bank->GetCombinedName();

         #ifdef HAVE_CXX11_THREADS
         std::lock_guard<std::mutex> lock(TAMultithreadHelper::gfLock);
         #endif
         if (strncmp(bank->NAME.DATATYPE,"DBL",3)==0)
         {
            std::pair<TTree*,TBranch*> t = FindOrCreateTree(runinfo, (GEMBANK<double>*)bank, CombinedName);
            BranchTreeFromData(t.first, t.second, (GEMBANK<double>*)bank, MIDAS_TIME, CombinedName);
         }
         else if (strncmp(bank->NAME.DATATYPE,"FLT",3)==0)
         {
            std::pair<TTree*,TBranch*> t = FindOrCreateTree(runinfo, (GEMBANK<float>*)bank, CombinedName);
            BranchTreeFromData(t.first, t.second, (GEMBANK<float>*)bank, MIDAS_TIME, CombinedName);
         }
         else if (strncmp(bank->NAME.DATATYPE,"BOOL",4)==0)
         {
            std::pair<TTree*,TBranch*> t = FindOrCreateTree(runinfo, (GEMBANK<bool>*)bank, CombinedName);
            BranchTreeFromData(t.first, t.second, (GEMBANK<bool>*)bank, MIDAS_TIME, CombinedName);
         }
         else if (strncmp(bank->NAME.DATATYPE,"I32",3)==0)
         {
            std::pair<TTree*,TBranch*> t = FindOrCreateTree(runinfo, (GEMBANK<int32_t>*)bank, CombinedName);
            BranchTreeFromData(t.first, t.second, (GEMBANK<int32_t>*)bank, MIDAS_TIME, CombinedName);
         }
         else if (strncmp(bank->NAME.DATATYPE,"U32",4)==0)
         {
            std::pair<TTree*,TBranch*> t = FindOrCreateTree(runinfo, (GEMBANK<uint32_t>*)bank, CombinedName);
            BranchTreeFromData(t.first, t.second, (GEMBANK<uint32_t>*)bank, MIDAS_TIME, CombinedName);
         }
         else if (strncmp(bank->NAME.DATATYPE,"U16",3)==0)
         {
            std::pair<TTree*,TBranch*> t = FindOrCreateTree(runinfo, (GEMBANK<uint16_t>*)bank, CombinedName);
            BranchTreeFromData(t.first, t.second, (GEMBANK<uint16_t>*)bank, MIDAS_TIME, CombinedName);
         }
         else if (strncmp(bank->NAME.DATATYPE,"STRA",4)==0)
         {
            std::cout<<"Writing of string array probably wobbly"<<std::endl;
            std::pair<TTree*,TBranch*> t = FindOrCreateTree(runinfo, (GEMBANK<char>*)bank, CombinedName);
            BranchTreeFromData(t.first, t.second, (GEMBANK<char>*)bank, MIDAS_TIME, CombinedName);
         }
         else if (strncmp(bank->NAME.DATATYPE,"STR",3)==0) 
         {
            std::pair<TTree*,TBranch*> t = FindOrCreateTree(runinfo, (GEMBANK<char>*)bank, CombinedName);
            BranchTreeFromData(t.first, t.second, (GEMBANK<char>*)bank, MIDAS_TIME, CombinedName);
         }
         else
         {
            std::cout<<"Unknown bank data type... "<<std::endl;
            bank->print();
         }
         return;
      }

};



class feGEMModule: public TARunObject
{
private:
   int nGEMBanks = 0;
public:
   feGEMModuleFlags* fFlags;
   bool fTrace = false;
   feGEMModuleWriter* writer;
   uint32_t RunStartTime;
   feGEMModule(TARunInfo* runinfo, feGEMModuleFlags* flags)
      : TARunObject(runinfo), fFlags(flags)
   {
      ModuleName="feGEM_module";
      if (fTrace)
         printf("feGEMModule::ctor!\n");
   }

   ~feGEMModule()
   {
      if (fTrace)
         printf("feGEMModule::dtor!\n");
   }

   void BeginRun(TARunInfo* runinfo)
   {
      runinfo->fRoot->fOutputFile->cd();
      gDirectory->mkdir("feGEM")->cd();
      runinfo->fOdb->RU32("/Runinfo/Start time binary",&RunStartTime);
      writer = new feGEMModuleWriter((double)RunStartTime);
      if (fTrace)
         printf("feGEMModule::BeginRun, run %d, file %s\n", runinfo->fRunNo, runinfo->fFileName.c_str());
    }

   void EndRun(TARunInfo* runinfo)
   {
      if (fTrace)
         printf("feGEMModule::EndRun, run %d\n", runinfo->fRunNo);
   }
   
   void PauseRun(TARunInfo* runinfo)
   {
      if (fTrace)
         printf("feGEMModule::PauseRun, run %d\n", runinfo->fRunNo);
   }

   void ResumeRun(TARunInfo* runinfo)
   {
      if (fTrace)
         printf("ResumeModule, run %d\n", runinfo->fRunNo);
   }

   TAFlowEvent* AnalyzeFlowEvent(TARunInfo* runinfo, TAFlags* flags, TAFlowEvent* flow)
   {
      
      //I do nothing here.. I am just printing the feGEM data to screen
      GEMBANKARRAY_Flow* array = flow->Find<GEMBANKARRAY_Flow>();
      if (array)
      {
         array->data->print();
         uint32_t MIDAS_TIME = array->MIDAS_TIME;
         char* dataptr = (char*) &array->data->DATA[0];
         for (int i=0; i<array->data->NumberOfEntries; i++)
         {
            GEMBANK<void*>* bank = (GEMBANK<void*>*)dataptr;
            writer->SaveToTree(runinfo, bank, MIDAS_TIME);
            //PrintGEMBANK(bank);
            dataptr += bank->GetTotalSize();
         }
         std::cout<<"============================="<<std::endl;
      }   
      GEMBANK_Flow* gembank = flow->Find<GEMBANK_Flow>();

      if (gembank)
      {
         GEMBANK<void*>* bank = gembank->data;
         writer->SaveToTree(runinfo, bank, gembank->MIDAS_TIME);
         //PrintGEMBANK(bank);
      }
      
      return flow; 
   }

   TAFlowEvent* Analyze(TARunInfo* runinfo, TMEvent* me, TAFlags* flags, TAFlowEvent* flow)
   {
      //if (me->event_id == 1)
      me->FindAllBanks();
      {
         const TMBank* MIDAS_BANK = me->FindBank("GEM1");
         std::cout<<me->BankListToString()<<std::endl;
         if (MIDAS_BANK)
         {
            nGEMBanks++;
            uint32_t MIDAS_TIME = me->time_stamp;
            char* GEM_BANK_DATA = me->GetBankData(MIDAS_BANK);
            char GEM_BANK_VERSION = GEM_BANK_DATA[3];
            int BankLen = MIDAS_BANK->data_size;
            //char* GEM_BANK_VERSION = (char*)MIDAS_BANK + MIDAS_BANK->data_offset;
            if (strncmp(GEM_BANK_DATA,"GEB",3)==0)
            {
               //std::cout<<"GEM_BANK!"<<std::endl;
               switch (GEM_BANK_VERSION - '0')
               {
                  case 1:
                  {
                     //GEM_BANK Version 1
                     flow = new GEMBANK_Flow(
                        flow,
                        (GEMBANK<void*>*)GEM_BANK_DATA,
                        BankLen,
                        MIDAS_TIME
                        );
                     return flow;
                  }
                  default:
                  {
                     std::cout<<"Fatal, unknown GEM BANK Version! Upgrade!"<<std::endl;
                     exit(123);
                  }
               }
            } //if GEB1
            
            if (strncmp(GEM_BANK_DATA,"GEA",3)==0)
            {
               //std::cout<<"GEM_BANK_ARRAY!"<<std::endl;
               switch (GEM_BANK_VERSION - '0')
               {
                  case 1:
                  {
                     //GEM_BANK_ARRAY Version 1
                     flow = new GEMBANKARRAY_Flow(
                        flow,
                        (GEMBANKARRAY*)GEM_BANK_DATA,
                        BankLen,
                        MIDAS_TIME
                        );
                     return flow;
                  }
                  default:
                  {
                     std::cout<<"Fatal, unknown GEM BANK ARRAY Version! Upgrade!"<<std::endl;
                     exit(123);
                  }
               }
            } //if GEA1
         } //if (MIDAS_BANK)
      }
      return flow;
   }

   void AnalyzeSpecialEvent(TARunInfo* runinfo, TMEvent* event)
   {
      if (fTrace)
         printf("feGEMModule::AnalyzeSpecialEvent, run %d, event serno %d, id 0x%04x, data size %d\n", 
                runinfo->fRunNo, event->serial_number, (int)event->event_id, event->data_size);
   }
};

class feGEMModuleFactory: public TAFactory
{
public:
   feGEMModuleFlags fFlags;

public:
   void Init(const std::vector<std::string> &args)
   {
      printf("feGEMModuleFactory::Init!\n");

      for (unsigned i=0; i<args.size(); i++) {
         if (args[i] == "--print")
            fFlags.fPrint = true;
      }
   }

   void Finish()
   {
      if (fFlags.fPrint)
         printf("feGEMModuleFactory::Finish!\n");
   }
   
   TARunObject* NewRunObject(TARunInfo* runinfo)
   {
      printf("feGEMModuleFactory::NewRunObject, run %d, file %s\n", runinfo->fRunNo, runinfo->fFileName.c_str());
      return new feGEMModule(runinfo, &fFlags);
   }
};

static TARegister tar(new feGEMModuleFactory);

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
