#ifndef _STRATIFIEDSAMPLER_H_
#define _STRATIFIEDSAMPLER_H_

#include <core/Common.h>
#include <core/SamplerInterface.h>

MSC_NAMESPACE_BEGIN

/**
 * @brief      Inherits from the sampler interface and represents a stratified sampler function
 * 
 * This will create an ditributed range of samples across a single pixel with stocastic variance
 * unlike the grid sampler.
 */
class StratifiedSampler : public SamplerInterface
{
public:
  /**
   * @brief      Create a stratified grid of samples across pixel area
   *
   * @param[in]  _base    base of sample count
   * @param      _random  thread local random generator to prevent mutation
   * @param      _output  output array of samples
   */
  void sample(const int _base, RandomGenerator* _random, float* _output) const;
};

MSC_NAMESPACE_END

YAML_NAMESPACE_BEGIN

template<> struct convert<msc::StratifiedSampler>
{
  static bool decode(const Node& node, msc::StratifiedSampler& rhs)
  {
    if(!node.IsMap() || node.size() != 1)
      return false;
    
    return true;
  }
};

YAML_NAMESPACE_END

#endif
