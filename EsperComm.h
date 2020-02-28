#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cstring>
#include <unistd.h>

#include "KOtcp.h"
#include "JsonTo.h"
#include "tmvodb.h"

namespace Esper
{
  struct EsperModuleData
  {
    std::map<std::string,int> t; // type
    std::map<std::string,std::string> s; // string variables
    std::map<std::string,std::vector<std::string>> sa; // string array variables
    std::map<std::string,int> i; // integer variables
    std::map<std::string,double> d; // double variables
    std::map<std::string,bool> b; // boolean variables
    std::map<std::string,std::vector<int>> ia; // integer array variables
    std::map<std::string,std::vector<double>> da; // double array variables
    std::map<std::string,std::vector<bool>> ba; // boolean array variables
  };

  typedef std::map<std::string,EsperModuleData> EsperNodeData;

  class EsperComm
  {
  public:
    std::string fName;
    bool fFailed;
    std::string fFailedMessage;
    bool fVerbose;

    KOtcpConnection* s;

  public:
    EsperComm(const char* name);
    ~EsperComm();
 
    void Open();
    KOtcpError Close();

    KOtcpError GetModules(std::vector<std::string>* mid);

    KOtcpError ReadVariables(TMVOdb* odb, const std::string& mid, EsperModuleData* vars);
    KOtcpError ReadVariables(const std::string& mid, EsperModuleData* vars);
    bool Write(const char* mid, const char* vid, const char* json, bool binaryn=false);
    std::string Read(const char* mid, const char* vid, std::string* last_errmsg = NULL);
  };

}
