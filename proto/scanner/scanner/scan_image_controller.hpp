#ifndef SCAN_IMAGE_CONTROLLER_HPP
#define SCAN_IMAGE_CONTROLLER_HPP 1

class scan_image_controller
{
private:
  boost::shared_ptr<scan_image> image_;
  boost::shared_ptr<scan_image_view> view_;
  boost::shared_ptr<image_border_detector> border_detector_;

public:
  scan_image_controller() {}

  explicit
  scan_image_controller(boost::shared_ptr<scan_image> image);

  explicit
  scan_image_controller(std::string const& filename);

  explicit
  scan_image_controller(const const* filename);



  void detect_border();
};

#endif // SCAN_IMAGE_CONTROLLER_HPP
