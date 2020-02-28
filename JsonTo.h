#include <vector>
#include <string>
#include "mjson.h"

namespace Esper
{
  std::vector<int> JsonToIntArray(const MJsonNode* n);
  std::vector<double> JsonToDoubleArray(const MJsonNode* n);
  std::vector<bool> JsonToBoolArray(const MJsonNode* n);
  std::vector<std::string> JsonToStringArray(const MJsonNode* n);
}
