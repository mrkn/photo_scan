#include "scan_image_controller.hpp"

scan_image_controller::scan_image_controller(boost::shared_ptr<scan_image> image)
  : image_(image)
{
}

scan_image_controller::scan_image_controller(std::string const& filename)
  : image_(scan_image::from_file(filename))
{
}

scan_image_controller::scan_image_controller(char const* filename)
  : image_(scan_image::from_file(filename))
{
}

