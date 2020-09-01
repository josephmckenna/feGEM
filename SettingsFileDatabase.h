#ifndef _SETTINGS_FILE_DATABASE_
#define _SETTINGS_FILE_DATABASE_
#include "GEM_BANK.h"
#include "MessageHandler.h"
#include <openssl/md5.h>
#include <map>
#include <dirent.h>
#include <iostream>
#include <fstream>

#include <stdio.h>

#include "msystem.h"

static std::string find_filename;
class SettingsFileDatabase
{
   private:
   std::string SettingsFileDatabasePath;
   //std::mutex lock;
   std::map<std::string,std::string> ActiveFileHashs;
   
   public:
   SettingsFileDatabase(const char* path)
   {
      SettingsFileDatabasePath=path;
   }
   std::string base64_encode(unsigned char const* bytes_to_encode, size_t in_len);
   void SaveSettingsFile(GEMBANK<char>* bank,MessageHandler* message);
   void LoadSettingsFile(GEMBANK<char>* bank,MessageHandler* message,int offset=0);
   void ListSettingsFile(GEMBANK<char>* bank,MessageHandler* message);
   //static int nameFilter(const struct dirent *entry);
};

#endif
