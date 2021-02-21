
#include "MessageHandler.h"


//--------------------------------------------------
// Message Handler Class
// A class to queue messages and errors to be sent to labVIEW  (or python) client as JSON
//--------------------------------------------------

MessageHandler::MessageHandler(TMFE* mfe)
{
   fMfe=mfe;
   TotalText=0;
}

MessageHandler::~MessageHandler()
{
   if (JSONMessageQueue.size())
   {
      std::cout<<"WARNING: Messages not flushed:"<<std::endl;
      for (auto msg: JSONMessageQueue)
         for (char c: msg)
            std::cout<< c;
         std::cout<<std::endl;
   }
   if (JSONErrorQueue.size())
   {
      std::cout<<"ERROR: Errors not flushed:"<<std::endl;
      for (auto err: JSONErrorQueue)
         std::cout<<err<<std::endl;
   }  
}

void MessageHandler::QueueData(const char* name,const char* data, int length)
{
   if (length<0)
      length=strlen(data);
   std::vector<char> message;
   message.reserve(length+strlen(name)+3);
   message.push_back('"');
   for (size_t i=0; i<strlen(name); i++)
   {
      message.push_back(name[i]);
   }
   message.push_back('"');
   message.push_back(':');
   message.push_back('"');
   for (int i=0; i<length; i++)
   {
      if (data[i]=='"')
         message.push_back('\\');
      message.push_back(data[i]);
   }
   message.push_back('"');
   JSONMessageQueue.push_back(message);
   TotalText+=message.size();
}
//Queue JSON Object
void MessageHandler::QueueData(const char* name,std::vector<const char*> names, std::vector<const char*> data, std::vector<size_t> length)
{
   std::vector<char> message;
   int size_estimate=strlen(name)+3;
   for (size_t i=0; i<length.size(); i++)
   {
      size_estimate+=length[i]+1;
   }
   message.reserve(size_estimate);
   message.push_back('"');
   for (size_t i=0; i<strlen(name); i++)
   {
      message.push_back(name[i]);
   }
   message.push_back('"');
   message.push_back(':');
   message.push_back('{');
   message.push_back('"');
   for (size_t i=0; i<length.size(); i++)
   {
      for (size_t j=0; j<strlen(names.at(i)); j++)
      {
         char c=names.at(i)[j];
         message.push_back(c);
      }
      message.push_back('"');
      message.push_back(':');
      message.push_back('"');
      for (size_t j=0; j<length.at(i); j++)
      {
         char c=data.at(i)[j];
         message.push_back(c);
      }
      if (i!=length.size()-1)
      {
         message.push_back('"');
         message.push_back(',');
         message.push_back('"');
      }
   }
   message.push_back('"');
   message.push_back('}');
   JSONMessageQueue.push_back(message);
   TotalText+=message.size();

}
void MessageHandler::QueueMessage(const char* msg)
{
   int len=strlen(msg);
   //Quote marks in messages must have escape characters! (JSON requirement)
   for (int i=1; i<len; i++)
      if (msg[i]=='\"')
         assert(msg[i-1]=='\\');
   QueueData("msg",msg,len);
}

void MessageHandler::QueueError(const char* source, const char* err)
{
   int len=strlen(err);
   //Quote marks in errors must have escape characters! (JSON requirement)
   for (int i=1; i<len; i++)
      if (err[i]=='"')
         assert(err[i-1]=='\\');
   fMfe->Msg(MTALK, source, err);
   QueueData("err",err,len);
}

std::vector<char> MessageHandler::ReadMessageQueue(double midas_time)
{
   //Build basic JSON string ["msg:This is a message to LabVIEW","err:This Is An Error Message"]
   std::vector<char> msg;
   msg.reserve(TotalText+JSONMessageQueue.size()+JSONErrorQueue.size()+1+20);
   std::string buf="{\"MIDASTime\":\""+std::to_string(midas_time)+"\",";
   for (char c: buf)
      msg.push_back(c);
   int i=0;
   for (auto Message: JSONMessageQueue)
   {
      if (i++>0)
         msg.push_back(',');
      for (char c: Message)
         msg.push_back(c);
   }
   JSONMessageQueue.clear();
   for (auto Error: JSONErrorQueue)
   {
      if (i++>0)
         msg.push_back(',');
      for (char c: Error)
         msg.push_back(c);
   }
   JSONErrorQueue.clear();
   msg.push_back('}');
   return msg;
}
