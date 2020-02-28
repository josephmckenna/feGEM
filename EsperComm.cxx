#include "EsperComm.h"
//#include <unistd.h>

namespace Esper
{
  EsperComm::EsperComm(const char* name):fName(name),fVerbose(false)
  {
    Open();
    if( fVerbose ) std::cout<<"EsperComm::EsperComm "<<fName<<std::endl;
  }
  
  EsperComm::~EsperComm()
  {
    Close();
    delete s;
    std::cout<<"EsperComm says bye and thanks you for using it."<<std::endl;
  }
  
  void EsperComm::Open()
  {
    if( s )
      {
	s->Close();
	delete s;
      }
    s = new KOtcpConnection(fName.c_str(), "http");     
    s->fConnectTimeoutMilliSec = 20*1000;
    s->fReadTimeoutMilliSec = 20*1000;
    s->fWriteTimeoutMilliSec = 10*1000;
    s->fHttpKeepOpen = true;
    KOtcpError e  = s->Connect();
    if( e.error )
      {
	fFailed = true;
	std::cout<<"EsperComm::Open() "<<e.message<<std::endl;
      }
    else
      fFailed = false;
  }
  
  KOtcpError EsperComm::Close()
  {
    return s->Close();
  }

  KOtcpError EsperComm::GetModules(std::vector<std::string>* mid)
  {
    std::vector<std::string> headers;
    std::vector<std::string> reply_headers;
    std::string reply_body;

    KOtcpError e = s->HttpGet(headers, "/read_node?includeMods=y", &reply_headers, &reply_body);
    if( e.error )
      {
	char msg[1024];
	sprintf(msg, "EsperComm::GetModules() error: HttpGet(read_node) error %s", e.message.c_str());
	fFailed = true;
	fFailedMessage = msg;
	return e;
      }

    MJsonNode* jtree = MJsonNode::Parse(reply_body.c_str());
    //jtree->Dump();

    const MJsonNode* m = jtree->FindObjectNode("module");
    if (m) {
      const MJsonNodeVector *ma = m->GetArray();
      if (ma) {
	for (unsigned i=0; i<ma->size(); i++) {
	  const MJsonNode* mae = ma->at(i);
	  if (mae) {
	    //mae->Dump();
	    const MJsonNode* maek = mae->FindObjectNode("key");
	    const MJsonNode* maen = mae->FindObjectNode("name");
	    if (maek && maen) {
	      if (fVerbose)
		printf("module [%s] %s\n", maek->GetString().c_str(), maen->GetString().c_str());
	      mid->push_back(maek->GetString());
	    }
	  }
	}
      }
    }

    delete jtree;

    return KOtcpError();
  }

  KOtcpError EsperComm::ReadVariables(TMVOdb* odb, const std::string& mid, EsperModuleData* vars)
  {
    if (fFailed)
      return KOtcpError("ReadVariables", "failed flag");

    std::vector<std::string> headers;
    std::vector<std::string> reply_headers;
    std::string reply_body;

    std::string url;
    url += "/read_module?includeVars=y&mid=";
    url += mid.c_str();
    url += "&includeData=y";
    std::cout<<"EsperComm::ReadVariables at "<<url<<std::endl;

    KOtcpError e = s->HttpGet(headers, url.c_str(), &reply_headers, &reply_body);

    if (e.error) 
      {
	char msg[1024];
	sprintf(msg, "ReadVariables() error: HttpGet(read_module %s) error %s", mid.c_str(), e.message.c_str());
	fFailed = true;
	fFailedMessage = msg;
	return e;
      }

    MJsonNode* jtree = MJsonNode::Parse(reply_body.c_str());
    //jtree->Dump();

    const MJsonNode* v = jtree->FindObjectNode("var");
    if( v ) 
      {
	//v->Dump();
	const MJsonNodeVector *va = v->GetArray();
	if( va ) 
	  {
	    for (unsigned i=0; i<va->size(); i++) 
	      {
		const MJsonNode* vae = va->at(i);
		if( vae ) 
		  {
		    //vae->Dump();
		    const MJsonNode* vaek = vae->FindObjectNode("key");
		    const MJsonNode* vaet = vae->FindObjectNode("type");
		    const MJsonNode* vaed = vae->FindObjectNode("d");
		    if (vaek && vaet && vaed) 
		      {
			std::string vid(vaek->GetString());
			int type = vaet->GetInt();
			vars->t[vid] = type;
			if (fVerbose)
			  printf("mid [%s] vid [%s] type %d json value %s\n", 
				 mid.c_str(), vid.c_str(), type, vaed->Stringify().c_str());
			if( type == 0 ) 
			  {
			    odb->WS( vid.c_str(), vaed->Stringify().c_str() );
			    //std::cout<<type<<"\t"<<mid<<"\t"<<vid<<"\t"<<vaed->Stringify()<<std::endl;
			  } 
			else if (type == 1 || type == 2 || type == 3 || type == 4 || type == 5 || type == 6 ) 
			  {
			    std::vector<int> val = JsonToIntArray(vaed);
			    if (val.size() == 1)
			      vars->i[vid] = val[0];
			    else
			      vars->ia[vid] = val;
			    odb->WIA( vid.c_str(), val );
			    //std::cout<<type<<"\t"<<mid<<"\t"<<vid<<"\t"<<val<<std::endl;
			  } 
			else if (type == 9) 
			  {
			    std::vector<double> val = JsonToDoubleArray(vaed);
			    if (val.size() == 1)
			      vars->d[vid] = val[0];
			    else
			      vars->da[vid] = val;
			    odb->WDA( vid.c_str(), val) ;
			    //std::cout<<type<<"\t"<<mid<<"\t"<<vid<<"\t"<<val<<std::endl;
			  } 
			else if (type == 11) 
			  {
			    std::string val = vaed->GetString();
			    vars->s[vid] = val;
			    odb->WS( vid.c_str(), val.c_str() );
			    //std::cout<<type<<"\t"<< mid<<"\t"<<vid<<"\t"<<val<<std::endl;
			  } 
			else if (type == 12) 
			  {
			    std::vector<bool> val = JsonToBoolArray(vaed);
			    if (val.size() == 1)
			      vars->b[vid] = val[0];
			    else
			      vars->ba[vid] = val;
			    odb->WBA( vid.c_str(), val) ;
			    //std::cout<<type<<"\t"<< mid<<"\t"<<vid<<"\t"<<val<<std::endl;
			  } 
			else if (type == 13) 
			  {
			    odb->WS(  vid.c_str(), vaed->Stringify().c_str() );
			    //std::cout<<type<<"\t"<< mid<<"\t"<<vid<<"\t"<<vaed->Stringify()<<std::endl;
			  } 
			else 
			  {
			    printf("mid [%s] vid [%s] type %d json value %s\n", 
				   mid.c_str(), vid.c_str(), type, vaed->Stringify().c_str());
			    odb->WS(  vid.c_str(), vaed->Stringify().c_str() );
			    //std::cout<<type<<"\t"<<mid<<"\t"<<vid<<"\t"<<vaed->Stringify()<<std::endl;
			  }
		      }
		  }
	      }
	  }
      }
    delete jtree;

    return KOtcpError();
  }

  KOtcpError EsperComm::ReadVariables(const std::string& mid, EsperModuleData* vars)
  {
    if (fFailed)
      return KOtcpError("ReadVariables", "failed flag");

    const char* odbname="none";

    std::vector<std::string> headers;
    std::vector<std::string> reply_headers;
    std::string reply_body;

    std::string url;
    url += "/read_module?includeVars=y&mid=";
    url += mid.c_str();
    url += "&includeData=y";
    std::cout<<"EsperComm::ReadVariables at "<<url<<std::endl;

    KOtcpError e = s->HttpGet(headers, url.c_str(), &reply_headers, &reply_body);

    if (e.error) 
      {
	char msg[1024];
	sprintf(msg, "ReadVariables() error: HttpGet(read_module %s) error %s", mid.c_str(), e.message.c_str());
	fFailed = true;
	fFailedMessage = msg;
	return e;
      }

    MJsonNode* jtree = MJsonNode::Parse(reply_body.c_str());
    //jtree->Dump();

    const MJsonNode* v = jtree->FindObjectNode("var");
    if( v ) 
      {
	//v->Dump();
	const MJsonNodeVector *va = v->GetArray();
	if( va ) 
	  {
	    for (unsigned i=0; i<va->size(); i++) 
	      {
		const MJsonNode* vae = va->at(i);
		if( vae ) 
		  {
		    //vae->Dump();
		    const MJsonNode* vaek = vae->FindObjectNode("key");
		    const MJsonNode* vaet = vae->FindObjectNode("type");
		    const MJsonNode* vaed = vae->FindObjectNode("d");
		    if (vaek && vaet && vaed) 
		      {
			std::string vid = vaek->GetString();
			int type = vaet->GetInt();
			vars->t[vid] = type;
			if (fVerbose)
			  printf("mid [%s] vid [%s] type %d json value %s\n", 
				 mid.c_str(), vid.c_str(), type, vaed->Stringify().c_str());
			if( type == 0 ) 
			  {
			    //WR(mfe, eq, odbname, mid.c_str(), vid.c_str(), vaed->Stringify().c_str());
			    std::cout<<odbname<<"\t"<<type<<"\t"<<mid<<"\t"<<vid<<"\t"<<vaed->Stringify()<<std::endl;
			  } 
			else if (type == 1 || type == 2 || type == 3 || type == 4 || type == 5 || type == 6) 
			  {
			    std::vector<int> val = JsonToIntArray(vaed);
			    if (val.size() == 1)
			      vars->i[vid] = val[0];
			    else
			      vars->ia[vid] = val;
			    //WRI(mfe, eq, odbname, mid.c_str(), vid.c_str(), val);
			    //std::cout<<odbname<<"\t"<<type<<"\t"<<mid<<"\t"<<vid<<"\t"<<val<<std::endl;
			  } 
			else if (type == 9) 
			  {
			    std::vector<double> val = JsonToDoubleArray(vaed);
			    if (val.size() == 1)
			      vars->d[vid] = val[0];
			    else
			      vars->da[vid] = val;
			    //                        WRD(mfe, eq, odbname, mid.c_str(), vid.c_str(), val);
			    // std::cout<<odbname<<"\t"<<type<<"\t"<<mid<<"\t"<<vid<<"\t"<<val<<std::endl;
			  } 
			else if (type == 11) 
			  {
			    std::string val = vaed->GetString();
			    vars->s[vid] = val;
			    //                        WR(mfe, eq, odbname, mid.c_str(), vid.c_str(), val.c_str());
			    std::cout<<odbname<<"\t"<<type<<"\t"<< mid<<"\t"<<vid<<"\t"<<val<<std::endl;
			  } 
			else if (type == 12) 
			  {
			    std::vector<bool> val = JsonToBoolArray(vaed);
			    if (val.size() == 1)
			      vars->b[vid] = val[0];
			    else
			      vars->ba[vid] = val;
			    //WRB(mfe, eq, odbname, mid.c_str(), vid.c_str(), val);
			    //std::cout<<odbname<<"\t"<<type<<"\t"<< mid<<"\t"<<vid<<"\t"<<val<<std::endl;
			  } 
			else if (type == 13) 
			  {
			    //			    WR(mfe, eq, odbname, mid.c_str(), vid.c_str(), vaed->Stringify().c_str()); 
			    std::cout<<odbname<<"\t"<<type<<"\t"<< mid<<"\t"<<vid<<"\t"<<vaed->Stringify()<<std::endl;
			  } 
			else 
			  {
			    printf("mid [%s] vid [%s] type %d json value %s\n", 
				   mid.c_str(), vid.c_str(), type, vaed->Stringify().c_str());
			    //WR(mfe, eq, odbname, mid.c_str(), vid.c_str(), vaed->Stringify().c_str());
			    //std::cout<<odbname<<"\t"<<type<<"\t"<<mid<<"\t"<<vid<<"\t"<<vaed->Stringify()<<std::endl;
			  }
			//variables.push_back(vid);
		      }
		  }
	      }
	  }
      }
    delete jtree;

    return KOtcpError();
  }

  bool EsperComm::Write(const char* mid, const char* vid, const char* json, bool binaryn)
  {
    if( fFailed )
      {
	std::cout<<"EsperComm::Write write failed status"<<std::endl;
	return false;
      }

    std::string url;
    url += "/write_var?";
    if (binaryn) {
      url += "binary=n";
      url += "&";
    }
    url += "mid=";
    url += mid;
    url += "&";
    url += "vid=";
    url += vid;
  
    printf("EsperComm::Write at URL: %s\n", url.c_str());

    std::vector<std::string> headers;
    std::vector<std::string> reply_headers;
    std::string reply_body;

    KOtcpError e = s->HttpPost(headers, url.c_str(), json, &reply_headers, &reply_body);

    if (e.error) 
      {
	char msg[1024];
	sprintf(msg, "Write() error: HttpPost(write_var %s.%s) error %s", mid, vid, e.message.c_str());
	fFailed = true;
	fFailedMessage = msg;
	return false;
      }

    if (reply_body.find("error") != std::string::npos) 
      {
	//         mfe->Msg(MERROR, "Write", "%s: AJAX write %s.%s value \"%s\" error: %s", fName.c_str(), mid, vid, json, reply_body.c_str());
	printf("%s: AJAX write %s.%s value \"%s\" error: %s", fName.c_str(), mid, vid, json, reply_body.c_str());
	return false;
      }

#if 0
    printf("reply headers for %s:\n", url.c_str());
    for (unsigned i=0; i<reply_headers.size(); i++)
      printf("%d: %s\n", i, reply_headers[i].c_str());

    printf("json: %s\n", reply_body.c_str());
#endif

    return true;
  }

  std::string EsperComm::Read(const char* mid, const char* vid, std::string* last_errmsg)
  {
    if(fFailed)
      {
	std::cout<<"EsperComm::Read read failed status"<<std::endl;
	return "";
      }

    std::string url;
    url += "/read_var?";
    url += "mid=";
    url += mid;
    url += "&";
    url += "vid=";
    url += vid;
    url += "&";
    url += "offset=";
    url += "0";
    url += "&";
    url += "dataOnly=y";

    // "/read_var?vid=elf_build_str&mid=board&offset=0&len=0&dataOnly=y"

    printf("EsperComm::Read at URL: %s\n", url.c_str());

    std::vector<std::string> headers;
    std::vector<std::string> reply_headers;
    std::string reply_body;

    KOtcpError e = s->HttpGet(headers, url.c_str(), &reply_headers, &reply_body);

    if (e.error) 
      {
	char msg[1024];
	sprintf(msg, "Read %s.%s HttpGet() error %s", mid, vid, e.message.c_str());
	if( !last_errmsg || e.message != *last_errmsg) 
	  {
	    if(last_errmsg) 
	      {
		*last_errmsg = e.message;
	      }
	  }
	fFailed = true;
	fFailedMessage = msg;
	std::cout<<msg<<std::endl;
	return "";
      }

#if 0
    printf("reply headers:\n");
    for (unsigned i=0; i<reply_headers.size(); i++)
      printf("%d: %s\n", i, reply_headers[i].c_str());

    printf("json: %s\n", reply_body.c_str());
#endif

#if 0
    if (strcmp(mid, "board") == 0) 
      {
	printf("mid %s, vid %s, json: %s\n", mid, vid, reply_body.c_str());
      }
#endif

    if (reply_body.length()>0) 
      {
	if (reply_body[0] == '{') 
	  {
	    if (reply_body.find("{\"error")==0) 
	      {
		printf("%s: Read %s.%s esper error %s", fName.c_str(), mid, vid, reply_body.c_str());
		return "";
	      }
	  }
      }
      
    return reply_body;
  }

}

