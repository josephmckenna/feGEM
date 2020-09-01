#ifndef _ALLOWED_HOSTS_
#define _ALLOWED_HOSTS_
#include <list>
#include <iostream>
#include <mutex>
#include <vector>
#include "tmfe.h"
#include <string.h>
#include <assert.h>
//Thread safe class to monitor host permissions
class AllowedHosts
{
   class Host{
      public:
      const std::string HostName;
      int RejectionCount;
      std::chrono::time_point<std::chrono::system_clock> LastContact;
      Host(const char* hostname);
      double TimeSince(std::chrono::time_point<std::chrono::system_clock> t);
      double TimeSinceLastContact();
      //Support wild card * and ?
      inline bool operator==(const char* hostname) const 
      { 
         //std::cout<<std::endl<<"TESTING:"<<HostName.c_str()<<"=="<<hostname<<std::endl;
         //int i=0;
         //int j=0;
         size_t size=strlen(hostname);
         for (size_t i=0, j=0; j<size; i++, j++)
         {
            //We are beyond the length of this object... comparison failed
            if (!HostName[i])
            {
               return false;
            }
            //'?' is an ignored character... skip
            if (HostName[i]=='?')
            {
               continue;
            }
            //'*' is a wild card of any length
            if (HostName[i]=='*')
            {
               //Wild card is at end of string... its a match!
               if (!HostName[++i])
                  return true;
               //Scan forward until next matching character
               while(HostName[i]!=hostname[j] && hostname[j])
                  ++j;
            }
            //We failed to match:
            if (HostName[i]!=hostname[j] && HostName[i])
            {
               //std::cout<<HostName[i+1]<<"!="<<hostname[j]<<std::endl;
               //std::cout<<HostName<<"!="<<hostname<<std::endl;
               return false;
            }
         }
         //std::cout<<HostName.c_str()<<"=="<<hostname<<std::endl;
         return true;
      }
      bool operator==(const Host & rhs) const     { return HostName==rhs.HostName;                 }
      void print();
   };
   private:
   std::mutex list_lock;
   //Allowed hosts:
   std::vector<Host> allowed_hosts;
   //Allowed hosts in testing mode (no ODB operations)
   std::vector<Host> virtual_allowed_hosts;
   //Hosts with questioned behaviour
   std::list<Host> questionable_hosts;
   //Banned hosts:
   std::vector<Host> banned_hosts;
   const int cool_down_time; //ms
   const int retry_limit;
   // Allow hosts to request addition to the allow_hosts list (default off)
   bool allow_self_registration;
   MVOdb* fOdbEqSettings;
   
   public:
   AllowedHosts(TMFE* mfe);
   void PrintRejection(TMFE* mfe,const char* hostname);
   bool IsAllowed(const char* hostname);
   //Allow this host:
   bool AddHost(const char* hostname);
   //Ban this host:
   bool BanHost(const char* hostname);
   const bool SelfRegistrationIsAllowed() { return allow_self_registration; }
   private:
   bool IsListedAsAllowed(const char* hostname);
   bool IsListedAsBanned(const char* hostname);
   bool IsListedAsQuestionable(const char* hostname);
};

#endif
