#ifndef _NULLSHADER_H_
#define _NULLSHADER_H_

#include <core/Common.h>
#include <core/ShaderInterface.h>

MSC_NAMESPACE_BEGIN

/**
 * @brief      Inherits from the shader interface and represents a non-cntributing surface
 */
class NullShader : public ShaderInterface
{
public:

  /**
   * @brief      illumination of mulitple positions on a geometric surface
   *
   * @param[in]  <unnamed>  amount of positions to be illuminated
   * @param      _random    thread local random generator to prevent mutation
   * @param      _position  array of position data
   * @param      _input     array of input directions generated by light for surface evaluation
   * @param      _radiance  array of radiance values for surface postions
   */
  void evaluate(
    const int _size,
    TextureSystem _texture_system,
    const Vector3f* _input,
    const Vector3f* _output,
    const Vector3f* _normal,
    float* _u,
    float* _v,
    float* _result
    ) const;
};

MSC_NAMESPACE_END

YAML_NAMESPACE_BEGIN

template<> struct convert<msc::NullShader>
{
  static Node encode(const msc::NullShader& rhs)
  {
    Node node;
    node["shader"]["type"] = "Null";
    return node;
  }

  static bool decode(const Node& node, msc::NullShader& rhs)
  {
    if(!node.IsMap() || node.size() != 3)
      return false;

    return true;
  }
};

YAML_NAMESPACE_END

#endif