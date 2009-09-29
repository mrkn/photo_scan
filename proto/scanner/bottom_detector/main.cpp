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
detect_bottom_pixels()
{
  output_image = input_image;
  boost::gil::rgb8c_view_t::xy_locator in_loc = boost::gil::const_view(input_image).xy_at(0, 0);
  boost::gil::rgb8_view_t::xy_locator out_loc = boost::gil::view(output_image).xy_at(0, 0);

  for (int x = 0; x < output_image.width(); ++x) {
    int y = output_image.height() - 1;
    for (int i = y + 1; i > 0; --i, --y) {
      double v = 0.0;
      if (240 <= in_loc(x, y)[2] && 240 <= in_loc(x, y)[0]) {
        v = in_loc(x, y)[1];
      }
      else {
        boost::gil::gray8_pixel_t gr_pix;
        boost::gil::color_convert(in_loc(x, y), gr_pix);
        v = gr_pix;
      }
      if (v > 24) {
        out_loc(x, y)[0] = 0;
        out_loc(x, y)[1] = out_loc(x, y)[2] = 255;
        break;
      }
    }
  }

  for (int y = 0; y < output_image.height(); ++y) {
    int x = 0;
    for (; x < output_image.width(); ++x) {
      double v = 0.0;
      if (240 <= in_loc(x, y)[2] && 240 <= in_loc(x, y)[0]) {
        v = in_loc(x, y)[1];
      }
      else {
        boost::gil::gray8_pixel_t gr_pix;
        boost::gil::color_convert(in_loc(x, y), gr_pix);
        v = gr_pix;
      }
      if (v > 24) {
        out_loc(x, y)[0] = 0;
        out_loc(x, y)[1] = out_loc(x, y)[2] = 255;
        break;
      }
    }
  }
}

int WINAPI
WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int show_mode)
{
  std::string input_filename = select_input_file(hinst);
  read_image(input_filename, input_image);
  detect_bottom_pixels();

  std::string output_filename = boost::filesystem::basename(input_filename) + std::string("-bottom.png");
  boost::gil::png_write_view(output_filename, boost::gil::const_view(output_image));

  return 0;
}
