#ifndef _SCENE_H_
#define _SCENE_H_

#include <vector>
#include <map>

#include <boost/shared_ptr.hpp>

#include <core/Common.h>
#include <core/EmbreeWrapper.h>
#include <core/PolygonObject.h>
#include <core/ShaderInterface.h>
#include <core/LightInterface.h>

MSC_NAMESPACE_BEGIN

/**
 * @brief      Data structure for scene information such as objects, lights and shaders
 */
struct Scene
{
  RTCScene rtc_scene;

  std::vector< boost::shared_ptr< PolygonObject > > objects;
  std::vector< boost::shared_ptr< ShaderInterface > > shaders;
  std::vector< boost::shared_ptr< LightInterface > > lights;
  std::map< int, int > shaders_to_lights;
};

MSC_NAMESPACE_END

#endif
