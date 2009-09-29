#include <windows.h>
#include <tchar.h>
#include <commdlg.h>

#include <boost/filesystem.hpp>

#include <boost/gil/gil_all.hpp>
#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>

#include <boost/tuple/tuple.hpp>

#include <numeric>
#include <string>

#include "../common.hpp"

namespace {
  boost::gil::rgb8_image_t input_image;
  boost::gil::gray8_image_t gray_image;
  boost::gil::rgb8_image_t work_image;
  boost::gil::rgb8_image_t output_image;
}

void
magenta()
{
  // generate gray image
  gray_image = boost::gil::gray8_image_t(input_image.dimensions());
  boost::gil::copy_and_convert_pixels(boost::gil::const_view(input_image), boost::gil::view(gray_image));

  // setup working image
  work_image = boost::gil::rgb8_image_t(input_image.dimensions());
  boost::gil::copy_and_convert_pixels(boost::gil::const_view(gray_image), boost::gil::view(work_image));

  // generate locators
  boost::gil::gray8c_view_t::xy_locator gray_loc = boost::gil::const_view(gray_image).xy_at(0, 0);
  boost::gil::rgb8_view_t::xy_locator work_loc = boost::gil::view(work_image).xy_at(0, 0);

  // obtaining corner intensity
  double minimum_intensity = 240;
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      minimum_intensity = std::min<double>(minimum_intensity,
                          std::min<double>(gray_loc(j, i),
                          std::min<double>(gray_loc(input_image.width() - j - 1, i),
                          std::min<double>(gray_loc(j, input_image.height() - i - 1),
                                           gray_loc(input_image.width() - j - 1, input_image.height() - i - 1)))));
    }
  }

  for (int y = 0; y < input_image.height(); ++y) {
    for (int x = 0; x < input_image.width(); ++x) {
      if (gray_loc(x, y)[0] >= minimum_intensity) {
        work_loc(x, y)[0] = 255;
        work_loc(x, y)[1] = 0;
        work_loc(x, y)[2] = 255;
      }
    }
  }
  
  output_image = work_image;
}

int WINAPI
WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int show_mode)
{
  std::string input_filename = select_input_file(hinst);
  read_image(input_filename, input_image);
  magenta();

  std::string output_filename = boost::filesystem::basename(input_filename) + std::string("-magenta.png");
  boost::gil::png_write_view(output_filename, boost::gil::const_view(output_image));

  return 0;
}
