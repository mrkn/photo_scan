#pragma warning(disable: 4996)

#include <windows.h>
#include <tchar.h>
#include <commdlg.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <boost/gil/gil_all.hpp>
#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>

#include <boost/lexical_cast.hpp>

#include <boost/tuple/tuple.hpp>

#include <numeric>
#include <string>

#include "../common.hpp"

namespace {
  boost::gil::rgb8_pixel_t const black_pixel(0, 0, 0);
  boost::gil::rgb8_pixel_t const red_pixel(255, 0, 0);
  boost::gil::rgb8_pixel_t const green_pixel(0, 255, 0);
  boost::gil::rgb8_pixel_t const blue_pixel(0, 0, 255);
  boost::gil::rgb8_pixel_t const magenta_pixel(255, 0, 255);
}

void
magenta(boost::gil::rgb8_image_t const& source, boost::gil::rgb8_image_t& destination)
{
  // generate gray image
  boost::gil::gray8_image_t gray_image = boost::gil::gray8_image_t(source.dimensions());
  boost::gil::copy_and_convert_pixels(boost::gil::const_view(source), boost::gil::view(gray_image));

  // setup working image
  destination = boost::gil::rgb8_image_t(source.dimensions());
  boost::gil::copy_and_convert_pixels(boost::gil::const_view(gray_image), boost::gil::view(destination));

  // generate locators
  boost::gil::gray8c_view_t::xy_locator gray_loc = boost::gil::const_view(gray_image).xy_at(0, 0);
  boost::gil::rgb8_view_t::xy_locator dst_loc = boost::gil::view(destination).xy_at(0, 0);

  // obtaining corner intensity
  double minimum_intensity = 240;
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      minimum_intensity = std::min<double>(minimum_intensity,
                          std::min<double>(gray_loc(j, i),
                          std::min<double>(gray_loc(source.width() - j - 1, i),
                          std::min<double>(gray_loc(j, source.height() - i - 1),
                                           gray_loc(source.width() - j - 1, source.height() - i - 1)))));
    }
  }

  for (int y = 0; y < source.height(); ++y) {
    for (int x = 0; x < source.width(); ++x) {
      if (gray_loc(x, y)[0] >= minimum_intensity) {
        dst_loc(x, y) = magenta_pixel;
      }
    }
  }
}

void
coarse_graining(boost::gil::rgb8_image_t const& source, boost::gil::rgb8_image_t& destination)
{
  int dst_width  = (source.width()  + 7) / 8;
  int dst_height = (source.height() + 7) / 8;
  destination = boost::gil::rgb8_image_t(dst_width, dst_height);

  boost::gil::rgb8c_view_t::xy_locator in_loc = boost::gil::const_view(source).xy_at(0, 0);
  boost::gil::rgb8_view_t::xy_locator dst_loc = boost::gil::view(destination).xy_at(0, 0);

  for (int y = 0; y < dst_height; ++y) {
    for (int x = 0; x < dst_width; ++x) {
      double r = 0, g = 0, b = 0;
      int n = 0;
      for (int i = 0; i < 8 && 8*y + i < source.height(); ++i) {
        for (int j = 0; j < 8 && 8*x + j < source.width(); ++j, ++n) {
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
      dst_loc(x, y) = boost::gil::rgb8_pixel_t(r, g, b);
    }
  }
}

inline bool
is_border_pixel(boost::gil::rgb8_pixel_t const& rgb)
{
  double v = 0.0;
  if (240 <= rgb[2] && 240 <= rgb[0]) {
    v = rgb[1];
  }
  else {
    boost::gil::gray8_pixel_t gr_pix;
    boost::gil::color_convert(rgb, gr_pix);
    v = gr_pix;
  }
  return v > 24;
}

void
detect_bottom_pixels(boost::gil::rgb8_image_t& image)
{
  boost::gil::rgb8_view_t::xy_locator dst_loc = boost::gil::view(image).xy_at(0, 0);

  int const w = image.width();
  int const h = image.height();
  for (int x = 0; x < w; ++x) {
    int y = image.height() - 1;
    for (int i = y + 1; i > 0; --i, --y) {
      if (is_border_pixel(dst_loc(x, y))) {
        dst_loc(x, y) = blue_pixel;
        break;
      }
    }
  }

  for (int y = 0; y < h; ++y) {
    int x = 0;
    for (; x < image.width(); ++x) {
      if (is_border_pixel(dst_loc(x, y))) {
        dst_loc(x, y) = blue_pixel;
        break;
      }
    }
  }
}

inline bool
is_border_mark(boost::gil::rgb8_pixel_t const& rgb)
{
  return blue_pixel == rgb;
}

template<class Output>
void
collect_slope_points_left(boost::gil::rgb8_image_t const& source, int x, int y, Output out)
{
  boost::gil::rgb8c_view_t::xy_locator src_loc = boost::gil::const_view(source).xy_at(0, 0);
  int const w = source.width();
  int const h = source.height();

  typedef typename Output::container_type::value_type point_type;
  *out++ = point_type(x, y);
  for (int j = x--; j > 0; --j, --x) {
    int yy = source.height() - 1;
    for (int i = yy + 1; i > 0; --i, --yy) {
      if (is_border_mark(src_loc(x, yy)))
        break;
    }
    if (y - 1 <= yy && yy <= y + 1) {
      *out++ = point_type(x, yy);
      if (yy == y - 1) y = yy;
    }
  }
}

template<class Output>
void
collect_slope_points_right(boost::gil::rgb8_image_t const& source, int x, int y, Output out)
{
  boost::gil::rgb8c_view_t::xy_locator src_loc = boost::gil::const_view(source).xy_at(0, 0);
  int const w = source.width();
  int const h = source.height();

  typedef typename Output::container_type::value_type point_type;
  *out++ = point_type(x, y);
  for (; x < source.width(); ++x) {
    int yy = source.height() - 1;
    for (int i = yy + 1; i > 0; --i, --yy) {
      if (is_border_mark(src_loc(x, yy)))
        break;
    }
    if (y - 1 <= yy && yy <= y + 1) {
      *out++ = point_type(x, yy);
      if (yy == y - 1) y = yy;
    }
  }
}

template<class Input>
std::pair<double, double>
linear_interpolate(Input first, Input last)
{
  double x_mean = 0, x2_mean = 0, xy_mean = 0, y_mean = 0;
  int n = 0;
  while (first != last) {
    double x = first->x;
    double y = first->y;

    x_mean += x;
    x2_mean += x*x;
    xy_mean += x*y;
    y_mean += y;
    ++n;
    ++first;
  }

  x_mean /= n;
  x2_mean /= n;
  xy_mean /= n;
  y_mean /= n;

  double a = (y_mean*x_mean - xy_mean) / (x_mean*x_mean - x2_mean);
  double b = y_mean - a*x_mean;
  return std::make_pair(a, b);
}

boost::tuple<double, int, int>
detect_slope(boost::gil::rgb8_image_t const& source)
{
  boost::gil::rgb8c_view_t::xy_locator src_loc = boost::gil::const_view(source).xy_at(0, 0);
  int const w = source.width();
  int const h = source.height();

  int x = -1;
  int y = source.height() - 1;
  for (int i = y + 1; x < 0 && i > 0; --i, --y) {
    for (int xx = 0; xx < source.width(); ++xx) {
      if (is_border_mark(src_loc(xx, y)) &&
          ((xx   > 0 && y   > 0 && is_border_mark(src_loc(xx-1, y-1))) ||
           (            y   > 0 && is_border_mark(src_loc(xx,   y-1))) ||
           (xx+1 < w && y   > 0 && is_border_mark(src_loc(xx+1, y-1))))) {
        std::string msg = (boost::format("(%d, %d)") % xx % y).str();
        ::MessageBox(NULL, multi_to_wide(msg).c_str(), L"slope-bottom was detected", MB_OK);
        while (is_border_mark(src_loc(xx+1, y))) ++xx;
        x = xx;
        break;
      }
    }
  }

  std::vector<boost::gil::point2<int> > points;
  if (x <= source.width() / 2) {
    collect_slope_points_right(source, x, y, std::back_inserter(points));
  }
  else {
    collect_slope_points_left(source, x, y, std::back_inserter(points));
  }

  double slope, intercept;
  boost::tie(slope, intercept) = linear_interpolate(points.begin(), points.end());

  return boost::make_tuple(slope, x, y);
}

namespace {
  boost::gil::rgb8_image_t input_image;
  boost::gil::rgb8_image_t work_image;
  boost::gil::rgb8_image_t magenta_image;
  boost::gil::rgb8_image_t coarse_image;
}

int WINAPI
WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int show_mode)
{
  std::string input_filename = select_input_file(hinst);
  read_image(input_filename, input_image);

  magenta(input_image, magenta_image);
  boost::gil::png_write_view(boost::filesystem::basename(input_filename) + std::string("-magenta.png"),
                             boost::gil::const_view(magenta_image));

  coarse_graining(magenta_image, coarse_image);
  boost::gil::png_write_view(boost::filesystem::basename(input_filename) + std::string("-magenta-coarse.png"),
                             boost::gil::const_view(coarse_image));

  detect_bottom_pixels(coarse_image);
  boost::gil::png_write_view(boost::filesystem::basename(input_filename) + std::string("-magenta-coarse-bottom.png"),
                             boost::gil::const_view(coarse_image));

  double slope;
  int x, y;
  boost::tie(slope, x, y) = detect_slope(coarse_image);
  std::string slope_str = boost::lexical_cast<std::string>(slope);
  ::MessageBox(NULL, multi_to_wide(slope_str).c_str(), L"slope", MB_OK);
  boost::gil::rgb8_view_t::xy_locator loc = boost::gil::view(coarse_image).xy_at(0, 0);
  if (slope > 0.0) {
    int xx = x;
    for (int i = xx + 1; i > 0; --i, --xx) {
      int yy = y + static_cast<int>((xx - x)*slope);
      if (0 <= yy && yy < coarse_image.height())
        loc(xx, yy) = black_pixel;
    }
  }
  else {
    for (int xx = x; xx < coarse_image.width(); ++xx) {
      loc(xx, y + (xx - x)*slope) = boost::gil::rgb8_pixel_t(0, 0, 0);
      int yy = y + static_cast<int>((xx - x)*slope);
      if (0 <= yy && yy < coarse_image.height())
        loc(xx, yy) = black_pixel;
    }
  }
  boost::gil::png_write_view(boost::filesystem::basename(input_filename) + std::string("-magenta-coarse-bottom-x.png"),
                             boost::gil::const_view(coarse_image));

  return 0;
}
