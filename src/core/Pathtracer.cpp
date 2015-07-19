#include <fstream>

#include <tbb/tbb.h>
#include <boost/thread.hpp>

#include <core/Pathtracer.h>
#include <core/Bin.h>
#include <core/GeometricCamera.h>
#include <core/TentFilter.h>
#include <core/StratifiedSampler.h>
#include <core/PolygonObject.h>
#include <core/RaySort.h>
#include <core/RayIntersect.h>
#include <core/RayDecompress.h>
#include <core/RayBoundingbox.h>

MSC_NAMESPACE_BEGIN

void Pathtracer::construct(const std::string &_filename)
{
  std::vector<YAML::Node> node_root = YAML::LoadAllFromFile(_filename);

  YAML::Node node_setup = node_root[0];
  YAML::Node node_scene = node_root[1];

  {
    Image* image = new Image;

    image->width = 700;
    image->height = 500;
    image->base = 8;
    image->iteration = 0;

    if(node_setup["image"])
      *image = node_setup["image"].as<Image>();

    size_t sample_count = image->width * image->height * image->base * image->base;
    image->samples.resize(sample_count);

    size_t pixel_count = image->width * image->height;
    image->pixels.resize(pixel_count);
    for(size_t i = 0; i < pixel_count; ++i)
    {
      image->pixels[i].r = 0.f;
      image->pixels[i].g = 0.f;
      image->pixels[i].b = 0.f;
    }

    m_image.reset(image);
  }

  {
    Settings* settings = new Settings;

    settings->ray_depth = 4;
    settings->bucket_size = 16;
    settings->shading_size = 4096;
    settings->bin_exponent = 11;
    settings->batch_exponent = 25;

    if(node_setup["settings"])
      *settings = node_setup["settings"].as<Settings>();

    m_settings.reset(settings);
  }

  {
    GeometricCamera* camera = new GeometricCamera();

    camera->origin(Vector3f(0.f, 0.f, 0.f));
    camera->direction(Vector3f(0.f, 0.f, 1.f));
    camera->focalLength(5.f);
    camera->focalDistance(100.f);
    camera->aperture(10.f);

    m_camera.reset(camera);

    if(node_setup["camera"])
    {
      if(node_setup["camera"]["type"].as< std::string >() == "Geometric")
      {
        GeometricCamera* geometric_camera = new GeometricCamera();
        *geometric_camera = node_setup["camera"].as<GeometricCamera>();
        m_camera.reset(geometric_camera);
      }
    }
  }

  {
    TentFilter* filter = new TentFilter();

    m_filter.reset(filter);

    if(node_setup["filter"])
    {
      if(node_setup["filter"]["type"].as< std::string >() == "Tent")
      {
        TentFilter* tent_filter = new TentFilter();
        *tent_filter = node_setup["filter"].as<TentFilter>();
        m_filter.reset(tent_filter);
      }
    }
  }

  {
    StratifiedSampler* sampler = new StratifiedSampler();

    m_sampler.reset(sampler);

    if(node_setup["sampler"])
    {
      if(node_setup["sampler"]["type"].as< std::string >() == "Stratified")
      {
        StratifiedSampler* stratified_sampler = new StratifiedSampler();
        *stratified_sampler = node_setup["sampler"].as<StratifiedSampler>();
        m_sampler.reset(stratified_sampler);
      }
    }
  }

  m_batch.reset(new Batch);

  m_scene.reset(new Scene);
  m_scene->rtc_scene = rtcNewScene(RTC_SCENE_STATIC, RTC_INTERSECT1);
  
  for(YAML::const_iterator scene_iterator = node_scene.begin(); scene_iterator != node_scene.end(); ++scene_iterator)
  {
    YAML::Node first = scene_iterator->first;
    YAML::Node second = scene_iterator->second;

    if(first.as< std::string >() == "object")
    {
      if(second["type"].as< std::string >() == "Polygon")
      {
        PolygonObject polygon_object = second.as<PolygonObject>();

        size_t geom_id = rtcNewTriangleMesh(
          m_scene->rtc_scene,
          RTC_GEOMETRY_STATIC,
          polygon_object.indices.size() / 3,
          polygon_object.positions.size()
          );

        rtcSetBuffer(
          m_scene->rtc_scene,
          geom_id,
          RTC_VERTEX_BUFFER,
          polygon_object.positions.data(),
          0,
          sizeof(Vertex)
          );

        rtcSetBuffer(
          m_scene->rtc_scene,
          geom_id,
          RTC_INDEX_BUFFER,
          polygon_object.indices.data(),
          0,
          3 * sizeof(unsigned int)
          );

        m_scene->objects[geom_id] = polygon_object;
      }
    }

    if(first.as< std::string >() == "light")
    {
      if(second["type"].as< std::string >() == "Quad")
      {
        // PolygonObject* polygon_object = new PolygonObject();
        // *polygon_object = second.as<PolygonObject>();
        // m_scene.add(polygon_object);
      }
    }

    if(first.as< std::string >() == "material")
    {
      if(second["type"].as< std::string >() == "Lambert")
      {
        // PolygonObject* polygon_object = new PolygonObject();
        // *polygon_object = second.as<PolygonObject>();
        // m_scene.add(polygon_object);
      }
    }
  }

  rtcCommit(m_scene->rtc_scene);
}

void Pathtracer::create_threads()
{
  m_nthreads = boost::thread::hardware_concurrency();
//  m_nthreads = 1;

  if(m_nthreads > 1)
    std::cout << m_nthreads << " supported threads found" << std::endl;

  if(!(m_nthreads > 1))
    std::cout << "Single thread found, system will function as a serial operation" << std::endl;

  for(size_t i = 0; i < m_nthreads; ++i)
  {
    boost::shared_ptr< Bin > local_bin(new Bin());
    local_bin->size = pow(2, m_settings->bin_exponent);
    for(size_t iterator_bin = 0; iterator_bin < 6; ++iterator_bin)
    {
      local_bin->bin[iterator_bin] = boost::shared_ptr< RayCompressed[] >(new RayCompressed[local_bin->size]);
      local_bin->index[iterator_bin] = 0;
    }
    
    m_camera_threads.push_back(boost::shared_ptr< CameraThread >(new CameraThread(
      local_bin,
      m_batch,
      m_camera,
      m_sampler,
      m_image
      )));
    
    m_surface_threads.push_back(boost::shared_ptr< SurfaceThread >(new SurfaceThread(
      local_bin,
      m_batch,
      m_scene,
      m_image
      )));
  }
}

void Pathtracer::camera_threads()
{
  size_t bucket_size = m_settings->bucket_size;
  size_t task_count_x = ceil(m_image->width / static_cast<float>(bucket_size));
  size_t task_count_y = ceil(m_image->height / static_cast<float>(bucket_size));

  for(size_t i = 0; i < task_count_x; ++i)
  {
    for(size_t j = 0; j < task_count_y; ++j)
    {
      //Check for any pixels left after resolution division with bucket size
      size_t task_width = bucket_size;
      if((i * bucket_size + bucket_size) > m_image->width)
        task_width = m_image->width - (i * bucket_size);

      //Check for any pixels left after resolution division with bucket size
      size_t task_height = bucket_size;
      if((j * bucket_size + bucket_size) > m_image->height)
        task_height = m_image->height - (j * bucket_size);

      CameraTask task;
      task.begin_x = i * bucket_size;
      task.begin_y = j * bucket_size;
      task.end_x = task.begin_x + task_width;
      task.end_y = task.begin_y + task_height;

      m_camera_queue.push(task);
    }
  }

  // std::cout << task_count_x * task_count_y << " render tasks created" << std::endl;

  for(int iterator = 0; iterator < m_nthreads; ++iterator)
    m_camera_threads[iterator]->start(&m_camera_queue);

  for(int iterator = 0; iterator < m_nthreads; ++iterator)
    m_camera_threads[iterator]->join();
}

void Pathtracer::surface_threads(size_t _size, RayUncompressed* _batch)
{
  int current_id = _batch[0].geomID;
  size_t current_index = 0;
  for(size_t iterator = 0; iterator < _size; ++iterator)
  {
    if(current_id != _batch[iterator].geomID)
    {
      SurfaceTask task;
      task.begin = current_index;
      task.end = iterator;

      m_surface_queue.push(task);

      current_id = _batch[iterator].geomID;
      current_index = iterator;
    }
    else if((iterator - current_index) > 4096)
    {
      SurfaceTask task;
      task.begin = current_index;
      task.end = iterator;

      m_surface_queue.push(task);

      current_index = iterator;
    }
  }

  SurfaceTask task;
  task.begin = current_index;
  task.end = _size;

  m_surface_queue.push(task);

  for(int iterator = 0; iterator < m_nthreads; ++iterator)
    m_surface_threads[iterator]->start(&m_surface_queue, _batch);

  for(int iterator = 0; iterator < m_nthreads; ++iterator)
    m_surface_threads[iterator]->join();
}

Pathtracer::Pathtracer(const std::string &_filename)
{
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
  _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

  rtcInit(NULL);

  construct(_filename);
  create_threads();
}

Pathtracer::~Pathtracer()
{
  rtcDeleteScene(m_scene->rtc_scene);
  rtcExit();
}

void Pathtracer::image(float** _pixels, int* _width, int* _height)
{
  *_pixels = &(m_image->pixels[0].v[0]);

  *_width = m_image->width;
  *_height = m_image->height;
}

void Pathtracer::clear()
{
  size_t pixel_count = m_image->width * m_image->height;
  for(size_t i = 0; i < pixel_count; ++i)
  {
    m_image->pixels[i].r = 0.f;
    m_image->pixels[i].g = 0.f;
    m_image->pixels[i].b = 0.f;
  }

  m_image->iteration = 0;
}

static inline bool operator<(const RayUncompressed &lhs, const RayUncompressed &rhs)
{
  return (lhs.geomID < rhs.geomID) || (lhs.geomID == rhs.geomID && lhs.primID < rhs.primID);
}

int Pathtracer::process()
{
  m_batch->construct(m_settings->batch_exponent);

  size_t batch_size = pow(2, m_settings->batch_exponent);
  RayCompressed* batch_compressed = new RayCompressed[batch_size];
  RayUncompressed* batch_uncompressed = new RayUncompressed[batch_size];

  camera_threads();

  std::string path;
  while(m_batch->pop(&path))
  {
    std::ifstream infile;

    infile.open(path);
    infile.read((char *)batch_compressed, sizeof(RayCompressed) * batch_size);
    infile.close();

    tbb::parallel_for(tbb::blocked_range< size_t >(0, batch_size, 1024), RayDecompress(batch_compressed, batch_uncompressed));

    RayBoundingbox limits(batch_uncompressed);
    tbb::parallel_reduce(tbb::blocked_range< size_t >(0, batch_size, 1024), limits);

    RaySort &task = *new(tbb::task::allocate_root()) RaySort(0, batch_size, limits.value(), batch_uncompressed);
    tbb::task::spawn_root_and_wait(task);

    tbb::parallel_for(tbb::blocked_range< size_t >(0, batch_size, 128), RayIntersect(&(*m_scene), batch_uncompressed));

    tbb::parallel_sort(&batch_uncompressed[0], &batch_uncompressed[batch_size]);

    surface_threads(batch_size, batch_uncompressed);

    boost::filesystem::remove(path);
  }

  delete[] batch_uncompressed;
  delete[] batch_compressed;

  m_batch->clear();

  size_t sample_count = m_image->base * m_image->base;
  for(size_t iterator_x = 0; iterator_x < m_image->width; ++iterator_x)
  {
    for(size_t iterator_y = 0; iterator_y < m_image->height; ++iterator_y)
    {
      float total_r = 0.f;
      float total_g = 0.f;
      float total_b = 0.f;

      for(size_t iterator = 0; iterator < sample_count; ++iterator)
      {
        size_t sample_index = (iterator_x * m_image->height * sample_count) + (iterator_y * sample_count) + (iterator);
        total_r += m_image->samples[sample_index].r;
        total_g += m_image->samples[sample_index].g;
        total_b += m_image->samples[sample_index].b;
      }

      size_t pixel_index = iterator_y * m_image->width + iterator_x;
      m_image->pixels[pixel_index].r += (total_r / sample_count);
      m_image->pixels[pixel_index].g += (total_g / sample_count);
      m_image->pixels[pixel_index].b += (total_b / sample_count);
    }
  }

  m_image->iteration += 1;
  return m_image->iteration;
}

MSC_NAMESPACE_END