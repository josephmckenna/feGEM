#include "SettingsFileDatabase.h"


void SettingsFileDatabase::SaveSettingsFile(GEMBANK<char>* bank,MessageHandler* message)
{
      
   assert(bank->NumberOfEntries==1);
   //Subtract the timestamp size in the data block
   int size=bank->BlockSize-16;

   //Data layout: [VIName][NULL][Filename][NULL][BINARYDATA][NULL]
   const GEMDATA<char>* gemdata=bank->GetFirstDataEntry();

   char* start = (char*) &(gemdata->DATA[0]);
   char* ptr = start;
   char* end = ptr + size;

   std::string ProjectName="";
   while (ptr < end)
   {
      if (*ptr=='\0')
         break;
      if (*ptr!='/')
         ProjectName+=*ptr;
      ptr++;
   }
   ptr++;
   std::string Filename="";
   while (ptr < end)
   {
      if (*ptr=='\0')
         break;
      if (*ptr!='/')
         Filename+=*ptr;
      ptr++;
   }
   ptr++;
   char* BinaryData=ptr;

   //STR banks are null terminated and we dont need it here!
   int file_size=(int)(end - BinaryData) - 1; 

   //Save path: /ODBSetSavePath/ProjectName/Filename
   std::string SavePath=SettingsFileDatabasePath;
   mkdir(SavePath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
   SavePath+='/';
   SavePath+=ProjectName.c_str();
   mkdir(SavePath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
   SavePath+='/';
   std::string MD5_file=(SavePath+std::string("md5.")+Filename).c_str();
   SavePath+=Filename.c_str();
   
   //Add unix time 
   std::time_t unixtime = std::time(nullptr);
   SavePath+='.';
   SavePath+=std::to_string(unixtime);
   std::cout<<"Save to path:"<<SavePath.c_str()<<std::endl;

   //Check MD5 checksum
   unsigned char digest[MD5_DIGEST_LENGTH];
   MD5((unsigned char*) BinaryData, file_size, digest);

   //Convert MD5 digest to hex string
   static const char hexchars[] = "0123456789abcdef";
   std::string result;
   for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
   {
      unsigned char b = digest[i];
      char hex[3];

      hex[0] = hexchars[b >> 4];
      hex[1] = hexchars[b & 0xF];
      hex[2] = 0;

      result.append(hex);
   }

   if (strncmp(result.c_str(),bank->NAME.EquipmentType,32) == 0 )
      std::cout<<"Save: CHECK SUM GOOD!"<<std::endl;
   std::cout<<"Save:\t"<<result.c_str()<<"\tvs\t"<<bank->NAME.EquipmentType<<std::endl;

   //Write the file we recieved to file
   std::ofstream ofs(SavePath.c_str(),std::ofstream::out);
   for (int i=0; i<file_size; i++)
   {
      ofs<<BinaryData[i];
   }
   ofs.close();

   //Write out the md5 to its own file
   std::ofstream ofmd5( MD5_file.c_str(),std::ofstream::out);
   ofmd5<<result.c_str();
   ofmd5.close();
   
   return;
}

static int nameFilter(const struct dirent *entry)
{
   if (strncmp(entry->d_name,find_filename.c_str(),find_filename.size())==0)
   {
      return 1;
   }
   return 0;
}
long GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}


std::string SettingsFileDatabase::base64_encode(unsigned char const* bytes_to_encode, size_t in_len)
{

   size_t len_encoded = (in_len +2) / 3 * 4;

   unsigned char trailing_char = '=';
   const char* base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789"
             "+/";

   std::string ret;
   ret.reserve(len_encoded);

   unsigned int pos = 0;

   while (pos < in_len) {
      ret.push_back(base64_chars[(bytes_to_encode[pos + 0] & 0xfc) >> 2]);

      if (pos+1 < in_len) {
         ret.push_back(base64_chars[((bytes_to_encode[pos + 0] & 0x03) << 4) + ((bytes_to_encode[pos + 1] & 0xf0) >> 4)]);

           if (pos+2 < in_len) {
              ret.push_back(base64_chars[((bytes_to_encode[pos + 1] & 0x0f) << 2) + ((bytes_to_encode[pos + 2] & 0xc0) >> 6)]);
              ret.push_back(base64_chars[  bytes_to_encode[pos + 2] & 0x3f]);
           }
           else {
              ret.push_back(base64_chars[(bytes_to_encode[pos + 1] & 0x0f) << 2]);
              ret.push_back(trailing_char);
           }
        }
        else {

            ret.push_back(base64_chars[(bytes_to_encode[pos + 0] & 0x03) << 4]);
            ret.push_back(trailing_char);
            ret.push_back(trailing_char);
        }

        pos += 3;
    }


    return ret;
}


//Use offset to load older versions
void SettingsFileDatabase::LoadSettingsFile(GEMBANK<char>* bank,MessageHandler* message,int offset)
{
   assert(bank->NumberOfEntries==1);
   //Subtract the timestamp size in the data block
   int size=bank->BlockSize-16;

   //Data layout: [VIName][NULL][Filename][NULL][BINARYDATA][NULL]
   const GEMDATA<char>* gemdata=bank->GetFirstDataEntry();

   char* start = (char*) &(gemdata->DATA[0]);
   char* ptr = start;
   char* end = ptr + size;

   std::string ProjectName="";
   while (ptr < end)
   {
      if (*ptr=='\0')
         break;
      if (*ptr!='/')
         ProjectName+=*ptr;
      ptr++;
   }
   ptr++;
   std::string Filename="";
   while (ptr < end)
   {
      if (*ptr=='\0')
         break;
      if (*ptr!='/')
         Filename+=*ptr;
      ptr++;
   }
   ptr++;
   
   //Save path: /ODBSetSavePath/ProjectName/Filename
   std::string LoadPath=SettingsFileDatabasePath;
   LoadPath+='/';
   LoadPath+=ProjectName.c_str();
   LoadPath+='/';
   //LoadPath+=Filename.c_str();
   find_filename=Filename;
   struct dirent **namelist;
   int n = scandir( LoadPath.c_str() , &namelist, *nameFilter, alphasort );
   std::cout<<"Load: "<<n <<"items match?"<<std::endl;
   for (int i=0; i<n; i++)
   {
      std::cout<<"Load\t"<<namelist[i]->d_name<<std::endl;
   }

   const char* chosenFile=namelist[n-1]->d_name;
   long chosenFileSize=GetFileSize((LoadPath+chosenFile).c_str());
   std::cout<<"LoadFileSize:"<<chosenFileSize<<std::endl;
   std::ifstream ifs ((LoadPath+chosenFile).c_str(), std::ifstream::in);

   char buf[chosenFileSize];
   int specialCharCount=0;
   for (int i=0; i<chosenFileSize; i++)
   {
      buf[i]=ifs.get();
      if (buf[i]=='"')
        specialCharCount++;
   }
   ifs.close();

   //Check MD5 checksum
   unsigned char digest[MD5_DIGEST_LENGTH];
   MD5((unsigned char*) buf, chosenFileSize, digest);

   //Convert MD5 digest to hex string
   static const char hexchars[] = "0123456789abcdef";
   std::string result;
   for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
   {
      unsigned char b = digest[i];
      char hex[3];

      hex[0] = hexchars[b >> 4];
      hex[1] = hexchars[b & 0xF];
      hex[2] = 0;

      result.append(hex);
   }
   std::cout<<"LoadedFileMD5:"<<result.c_str()<<std::endl;
   std::string data=base64_encode((unsigned char*)buf,chosenFileSize);
   std::string ChosenFileDate=&chosenFile[Filename.size()+1];
   //message->QueueData("SettingsFile",SettingFileBlob,blob_size);
   message->QueueData("SettingsFile",  {
                                          "ProjectName",
                                          "Filename",
                                          "UNIXTIME",
                                          "MD5",
                                          "base64data"
                                       } , {
                                          ProjectName.c_str(),
                                          Filename.c_str(),
                                          ChosenFileDate.c_str(),
                                          result.c_str(),
                                          data.c_str()
                                       } , { 
                                          ProjectName.size(),
                                          Filename.size(),
                                          ChosenFileDate.size(),
                                          result.size(),
                                          data.size()
                                       });
   //message->QueueData("FileName",chosenFile,strlen(chosenFile));
   //message->QueueData("MD5",result.c_str(),result.size());
   //message->QueueData("FileContents",buf,chosenFileSize);

   return;
}
void SettingsFileDatabase::ListSettingsFile(GEMBANK<char>* bank,MessageHandler* message)
{
   return;
}
