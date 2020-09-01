#include "AllowedHosts.h"
//--------------------------------------------------
// Allowed Hosts class 
//          Contents: Listed allowed hosts, grey listed hosts and banned hosts
//     |-->Host class: 
//          Contents: Hostname, Rejection Counter and Last Contact time
// Thread safe class to monitor host permissions
//--------------------------------------------------
AllowedHosts::Host::Host(const char* hostname): HostName(hostname)
{
   RejectionCount=0;
   LastContact=std::chrono::high_resolution_clock::now();
}

double AllowedHosts::Host::TimeSince(std::chrono::time_point<std::chrono::system_clock> t)
{
   return std::chrono::duration<double, std::milli>(t-LastContact).count();
}

double AllowedHosts::Host::TimeSinceLastContact()
{
   return TimeSince(std::chrono::high_resolution_clock::now());
}

void AllowedHosts::Host::print()
{
   std::cout<<"HostName:\t"<<HostName<<"\n";
   std::cout<<"RejectionCount:\t"<<RejectionCount<<"\n";
   std::cout<<"Last rejection:\t"<< TimeSinceLastContact()*1000.<<"s ago"<<std::endl;
}

AllowedHosts::AllowedHosts(TMFE* mfe): cool_down_time(1000), retry_limit(10)
{
   //Set cooldown time to 10 seconds
   //Set retry limit to 10
   fOdbEqSettings=mfe->fOdbRoot->Chdir((std::string("Equipment/") + mfe->fFrontendName + std::string("/Settings")).c_str() );
   allow_self_registration=false;
   fOdbEqSettings->RB("allow_self_registration",&allow_self_registration,true);
   std::vector<std::string> list;
   list.push_back("local_host");
   fOdbEqSettings->RSA("allowed_hosts", &list,true,10,64);
   //Copy list of good hostnames into array of Host objects
   for (auto host: list)
      allowed_hosts.push_back(Host(host.c_str()));
   list.clear();
   list.push_back("bad_host_name");
   fOdbEqSettings->RSA("banned_hosts", &list,true,10,64);
   //Copy bad of good hostnames into array of Host objects
   for (auto host: list)
      banned_hosts.push_back(Host(host.c_str()));
}

void AllowedHosts::PrintRejection(TMFE* mfe,const char* hostname)
{
   for (auto & host: banned_hosts)
   {
      if (host==hostname)
      {
         if (host.RejectionCount<2*retry_limit)
            mfe->Msg(MERROR, "tryAccept", "rejecting connection from unallowed host \'%s\'", hostname);
         if (host.RejectionCount==2*retry_limit)
            mfe->Msg(MERROR, "tryAccept", "rejecting connection from unallowed host \'%s\'. This message will now be suppressed", hostname); 
         host.RejectionCount++;
         return;
      }
   }
}

bool AllowedHosts::IsAllowed(const char* hostname)
{
   //std::cout<<"Testing:"<<hostname<<std::endl;
   if (IsListedAsAllowed(hostname))
      return true;
   if (IsListedAsBanned(hostname))
      return false;
   //Questionable list only permitted if allowing self registration
   if (!allow_self_registration)
      return false;
   if (IsListedAsQuestionable(hostname))
      return true; 
   //I should never get this far:
   assert("LOGIC_FAILED");
   return false;
}

bool AllowedHosts::IsListedAsAllowed(const char* hostname)
{
   //std::cout<<"Looking for host:"<<hostname<<std::endl;
   std::lock_guard<std::mutex> lock(list_lock);
   for (auto& host: allowed_hosts)
   {
      //host.print();
      if (host==hostname)
         return true;
   }
   return false;
}
//Allow this host:
bool AllowedHosts::AddHost(const char* hostname)
{
   if (!IsListedAsAllowed(hostname))
   {
      {
      std::lock_guard<std::mutex> lock(list_lock);
      allowed_hosts.push_back(Host(hostname));
      }
      std::cout<<"Updating ODB with:"<< hostname<<" at "<<(int)allowed_hosts.size()-1<<std::endl;
      fOdbEqSettings->WSAI("allowed_hosts",(int)allowed_hosts.size()-1, hostname);
      //True for new item added
      return true;
   }
   //False, item not added (already in list)
   return false;
}
//Ban this host:
bool AllowedHosts::BanHost(const char* hostname)
{
   if (!IsListedAsBanned(hostname))
   {
      {
      std::lock_guard<std::mutex> lock(list_lock);
      banned_hosts.push_back(Host(hostname));
      fOdbEqSettings->WSAI("banned_hosts",banned_hosts.size() -1, hostname );
      }
      return true;
   }
   return false;
}

bool AllowedHosts::IsListedAsBanned(const char* hostname)
{
   const std::lock_guard<std::mutex> lock(list_lock);
   if (!banned_hosts.size()) return false;
   for (auto & host: banned_hosts)
   //for(std::vector<Host>::iterator host = banned_hosts.begin(); host != banned_hosts.end(); ++host) 
   {
      if (host==hostname)
         return true;
   }
   return false;
}

bool AllowedHosts::IsListedAsQuestionable(const char* hostname)
{
   const std::lock_guard<std::mutex> lock(list_lock);
   for (auto& host: questionable_hosts)
   {
      if (host==hostname)
      {
         host.print();
         std::cout<<"Rejection count:"<<host.RejectionCount<<std::endl;
         if (host.RejectionCount>retry_limit)
         {
            std::cout<<"Banning host "<<hostname<<std::endl;
            banned_hosts.push_back(host);
            questionable_hosts.remove(host);
         }
         
         std::chrono::time_point<std::chrono::system_clock> time_now=std::chrono::high_resolution_clock::now();
         std::cout<<host.TimeSince(time_now) << ">"<<cool_down_time<<std::endl;
         if (host.TimeSince(time_now)>cool_down_time)
         {
            std::cout<<"I've seen this host before, but "<<host.TimeSince(time_now)/1000. <<" seconds a long time ago"<<std::endl;
            host.LastContact=time_now;
            host.RejectionCount++;
            return true;
         }
         else
         {
            std::cout<<"This host has tried to connect too recently"<<std::endl;
            return false;
         }
      }
   }
   //This is the first time a host has tried to connect:
   questionable_hosts.push_back(Host(hostname));
   return true;
}
