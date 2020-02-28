#include "JsonTo.h"
namespace Esper
{
  
  std::vector<int> JsonToIntArray(const MJsonNode* n)
  {
    std::vector<int> vi;
    const MJsonNodeVector *a = n->GetArray();
    if (a) {
      for (unsigned i=0; i<a->size(); i++) {
	const MJsonNode* ae = a->at(i);
	if (ae) {
	  if (ae->GetType() == MJSON_NUMBER) {
	    //printf("MJSON_NUMBER [%s] is %f is 0x%x\n", ae->GetString().c_str(), ae->GetDouble(), (unsigned)ae->GetDouble());
	    vi.push_back((unsigned)ae->GetDouble());
	  } else {
	    vi.push_back(ae->GetInt());
	  }
	}
      }
    }
    return vi;
  }

  std::vector<double> JsonToDoubleArray(const MJsonNode* n)
  {
    std::vector<double> vd;
    const MJsonNodeVector *a = n->GetArray();
    if (a) {
      for (unsigned i=0; i<a->size(); i++) {
	const MJsonNode* ae = a->at(i);
	if (ae) {
	  vd.push_back(ae->GetDouble());
	}
      }
    }
    return vd;
  }

  std::vector<bool> JsonToBoolArray(const MJsonNode* n)
  {
    std::vector<bool> vb;
    const MJsonNodeVector *a = n->GetArray();
    if (a) {
      for (unsigned i=0; i<a->size(); i++) {
	const MJsonNode* ae = a->at(i);
	if (ae) {
	  vb.push_back(ae->GetBool());
	}
      }
    }
    return vb;
  }

  std::vector<std::string> JsonToStringArray(const MJsonNode* n)
  {
    std::vector<std::string> vs;
    const MJsonNodeVector *a = n->GetArray();
    if (a) {
      for (unsigned i=0; i<a->size(); i++) {
	const MJsonNode* ae = a->at(i);
	if (ae) {
	  vs.push_back(ae->GetString());
	}
      }
    }
    return vs;
  }
}
