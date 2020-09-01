
#ifndef _MessageHandler_
#define _MessageHandler_

#include "midas.h"
#include "tmfe.h"
#include <vector>
#include <string>
#include <iostream>
#include <string.h>
#include <assert.h>

class MessageHandler
{
   private:
      TMFE* fMfe;
      std::vector<std::vector<char>> JSONMessageQueue;
      std::vector<std::string> JSONErrorQueue;
      int TotalText;
   public:
   MessageHandler(TMFE* mfe);
   ~MessageHandler();
   bool HaveErrors() {return JSONErrorQueue.size();};
   void QueueData(const char* name, const char* msg, int length=-1);
   //Queue JSON object
   void QueueData(const char* name,std::vector<const char*> names,std::vector<const char*> data, std::vector<size_t> length);
   void QueueMessage(const char* msg);
   void QueueError(const char* source, const char* err);
   std::vector<char> ReadMessageQueue(double midas_time);
};

#endif
