#ifndef _CAMERATHREAD_H_
#define _CAMERATHREAD_H_

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <tbb/concurrent_queue.h>

#include <core/Common.h>
#include <core/Buffer.h>
#include <core/DirectionalBins.h>
#include <core/CameraInterface.h>
#include <core/SamplerInterface.h>
#include <core/Image.h>
#include <core/RandomGenerator.h>

MSC_NAMESPACE_BEGIN

struct CameraTask
{
  size_t begin_x;
  size_t begin_y;
  size_t end_x;
  size_t end_y;
};

class CameraThread
{ 
public:
  CameraThread(
    boost::shared_ptr< Buffer > _buffer,
    boost::shared_ptr< DirectionalBins > _bin,
    boost::shared_ptr< CameraInterface > _camera,
    boost::shared_ptr< SamplerInterface > _sampler,
    boost::shared_ptr< Image > _image
    )
    : m_buffer(_buffer)
    , m_bin(_bin)
    , m_camera(_camera)
    , m_sampler(_sampler)
    , m_image(_image)
  {;}

  void start(tbb::concurrent_queue< CameraTask >* _queue);
  void join();
  void process(tbb::concurrent_queue< CameraTask >* _queue);

private:
  boost::thread m_thread;
  boost::shared_ptr< Buffer > m_buffer;
  boost::shared_ptr< DirectionalBins > m_bin;
  boost::shared_ptr< CameraInterface > m_camera;
  boost::shared_ptr< SamplerInterface > m_sampler;
  boost::shared_ptr< Image > m_image;

  RandomGenerator m_random;
};

MSC_NAMESPACE_END

#endif