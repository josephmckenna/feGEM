//
// GEM_BANK_Flow.h
//
// manalyzer flow objects for feGEM events
// JTK McKenna
//



#ifndef GEM_BANK_Flow_H
#define GEM_BANK_Flow_H
#include "manalyzer.h"
#include "GEM_BANK.h"

#include "assert.h"


class GEMBANKARRAY_Flow: public TAFlowEvent
{
   public:
   GEMBANKARRAY* data;
    public:
  GEMBANKARRAY_Flow(TAFlowEvent* flow, GEMBANKARRAY* e, int BankLen)
       : TAFlowEvent(flow)
  {
      std::cout<<"Bank len:"<<BankLen<<std::endl;
      data = (GEMBANKARRAY*)malloc(BankLen);
      memcpy(data,e,BankLen);
      assert(data->GetTotalSize()==BankLen);
  }
  ~GEMBANKARRAY_Flow()
  {
     if (data)
          free(data);
  }
};


class GEMBANK_Flow: public TAFlowEvent
{
   public:
   GEMBANK<void*>* data;
   public:
   GEMBANK_Flow(TAFlowEvent* flow, GEMBANK<void*>* e, int BankLen)
       : TAFlowEvent(flow)
   {
      //data = new GEMBANK<T>(e);
      data = (GEMBANK<void*>*)malloc(BankLen);
      memcpy(data,e,BankLen);
      //assert(sizeof(e)==BankLen);
   }
   ~GEMBANK_Flow()
   {
      if (data)
         free(data);
  }
};

#endif