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
  boost::gil::rgb8_image_t output_image;
}

void
coarse_graining()
{
  int output_width  = (input_image.width()  + 7) / 8;
  int output_height = (input_image.height() + 7) / 8;
  output_image = boost::gil::rgb8_image_t(output_width, output_height);

  boost::gil::rgb8c_view_t::xy_locator in_loc = boost::gil::const_view(input_image).xy_at(0, 0);
  boost::gil::rgb8_view_t::xy_locator out_loc = boost::gil::view(output_image).xy_at(0, 0);

  for (int y = 0; y < output_height; ++y) {
    for (int x = 0; x < output_width; ++x) {
      double r = 0, g = 0, b = 0;
      int n = 0;
      for (int i = 0; i < 8 && 8*y + i < input_image.height(); ++i) {
        for (int j = 0; j < 8 && 8*x + j < input_image.width(); ++j, ++n) {
          r += in_loc(8*x + j, 8*y + i)[2];
          g += in_loc(8*x + j, 8*y + i)[1];
          b += in_loc(8*x + j, 8*y + i)[0];
        }
      }
      g /= n;
      if (g <= 24) {
        g = 0;
        r = b = 255;
      }
      else {
        r /= n;
        b /= n;
      }
      out_loc(x, y)[0] = b;
      out_loc(x, y)[1] = g;
      out_loc(x, y)[2] = r;
    }
  }
}

int WINAPI
WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int show_mode)
{
  std::string input_filename = select_input_file(hinst);
  read_image(input_filename, input_image);
  coarse_graining();

  std::string output_filename = boost::filesystem::basename(input_filename) + std::string("-coarse.png");
  boost::gil::png_write_view(output_filename, boost::gil::const_view(output_image));

  return 0;
}
