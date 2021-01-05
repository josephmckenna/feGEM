//
// feGEM analyzer module for rootana's manalzer
//
// JTK McKENNA
//

#include <stdio.h>

#include "GEM_BANK.h"
#include "GEM_BANK_flow.h"

#include "manalyzer.h"
#include "midasio.h"

#include <iostream>
class feGEMModuleFlags
{
public:
   bool fPrint = false;
};
class feGEMModule: public TARunObject
{
private:
   int nGEMBanks = 0;
public:
   feGEMModuleFlags* fFlags;
   bool fTrace = false;
   
   
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
         char* dataptr = (char*) &array->data->DATA[0];
         for (int i=0; i<array->data->NumberOfEntries; i++)
         {
            GEMBANK<void*>* bank = (GEMBANK<void*>*)dataptr;
            PrintGEMBANK(bank);
            dataptr += bank->GetTotalSize();
         }
         std::cout<<"============================="<<std::endl;
      }   
      GEMBANK_Flow* gembank = flow->Find<GEMBANK_Flow>();
      if (gembank)
      {
         GEMBANK<void*>* bank = gembank->data;
         PrintGEMBANK(bank);
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
                        BankLen);
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
                        BankLen);
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
