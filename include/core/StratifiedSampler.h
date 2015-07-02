#ifndef _STRATIFIEDSAMPLER_H_
#define _STRATIFIEDSAMPLER_H_

#include <core/Common.h>
#include <core/SamplerInterface.h>

MSC_NAMESPACE_BEGIN

class StratifiedSampler : public SamplerInterface
{
public:
  StratifiedSampler();

  void virtualFunc(const float value_one, const float value_two) const;

private:
  float m_other_variable;
};

MSC_NAMESPACE_END

YAML_NAMESPACE_BEGIN

template<> struct convert<msc::StratifiedSampler>
{
  static Node encode(const msc::StratifiedSampler& rhs)
  {
    Node node;
    node["sampler"]["type"] = "Stratified";
    return node;
  }

  static bool decode(const Node& node, msc::StratifiedSampler& rhs)
  {
    if(!node.IsMap() || node.size() != 1)
    {
      return false;
    }
    
    return true;
  }
};

YAML_NAMESPACE_END

#endif