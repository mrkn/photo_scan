#include <windows.h>
#include <tchar.h>
#include <commdlg.h>

#include <boost/gil/gil_all.hpp>
#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>

#include <boost/tuple/tuple.hpp>

#include <numeric>
#include <string>

namespace {
int x(RECT const& rect) { return rect.left; }
int y(RECT const& rect) { return rect.top; }
int width(RECT const& rect) { return rect.right - rect.left; }
int height(RECT const& rect) { return rect.bottom - rect.top; }
}

std::string wide_to_multi(std::wstring const& wstr, int code = CP_ACP)
{
  // acquire the size of multi-byte string.
  int sz = ::WideCharToMultiByte(code, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
  if (sz == 0)
    return ""; // failed

  // converting to the multi-byte code.
  std::string str;
  str.resize(sz);
  ::WideCharToMultiByte(code, 0, wstr.c_str(), -1, &str[0], sz, NULL, NULL);
  return str;
}

std::string
select_input_file(HINSTANCE hinst)
{
  OPENFILENAME ofn;
  TCHAR filename[_MAX_PATH] = {'\0', };

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = NULL;
  ofn.hInstance = hinst;
  ofn.lpstrFilter = _T("Image file\0*.jpg;*.jpeg;*.png;*.tif;*.tiff\0");
  ofn.nFilterIndex = 1;
  ofn.lpstrFile = filename;
  ofn.nMaxFile = sizeof(filename);
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrTitle = NULL;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
  ofn.nFileOffset = 0;
  ofn.nFileExtension = 0;
  ofn.lpstrDefExt = NULL;

  BOOL rv = GetOpenFileName(&ofn);
  if (0 == rv) {
    DWORD error = CommDlgExtendedError();
    switch (error) {
#define CASE_(error_code) case error_code: MessageBox(NULL, _T(#error_code L" has occured"), _T("GetOpenFileName"), MB_OK); break
      CASE_(CDERR_DIALOGFAILURE);
      CASE_(CDERR_FINDRESFAILURE);
      CASE_(CDERR_NOHINSTANCE);
      CASE_(CDERR_INITIALIZATION);
      CASE_(CDERR_NOHOOK);
      CASE_(CDERR_LOCKRESFAILURE);
      CASE_(CDERR_NOTEMPLATE);
      CASE_(CDERR_LOADRESFAILURE);
      CASE_(CDERR_STRUCTSIZE);
      CASE_(CDERR_LOADSTRFAILURE);
      CASE_(FNERR_BUFFERTOOSMALL);
      CASE_(CDERR_MEMALLOCFAILURE);
      CASE_(FNERR_INVALIDFILENAME);
      CASE_(CDERR_MEMLOCKFAILURE);
      CASE_(FNERR_SUBCLASSFAILURE);
#undef CASE_
      default:
        MessageBox(NULL, _T("Unknown error"), _T("GetOpenFileName"), MB_OK);
    }
    return "";
  }
  return wide_to_multi(std::wstring(filename));
}

bool
register_window_class(HINSTANCE hinst, LPCTSTR class_name, WNDPROC wndproc)
{
  WNDCLASSEX wcex;
  wcex.cbSize = sizeof(wcex);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = wndproc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hinst;
  wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(255,255,255));
  wcex.lpszMenuName = NULL;
  wcex.lpszClassName = class_name;
  wcex.hIconSm = wcex.hIcon;

  ATOM atom = RegisterClassEx(&wcex);
  return 0 != atom;
}

HWND
create_main_window(HINSTANCE hinst, LPCTSTR class_name, LPCTSTR window_name, int width_, int height_)
{
  RECT rect;
  ::SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);

  if (width(rect) < width_ || height(rect) < height_) {
    width_ /= 2;
    height_ /= 2;
  }

  HWND hwnd = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW,
      class_name,
      window_name,
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      width_,
      height_,
      NULL,
      NULL,
      hinst,
      NULL);
  return hwnd;
}

LRESULT CALLBACK
window_proc(HWND hwnd, UINT msgid, WPARAM wp, LPARAM lp)
{
  void setup(HWND);
  void paint(HWND);

  switch (msgid) {
    case WM_CREATE:
      setup(hwnd);
      return 0;

    case WM_PAINT:
      paint(hwnd);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, msgid, wp, lp);
}

int
message_loop(HWND hwnd)
{
  void idle(HWND);
  MSG msg;

  for (;;) {
    if (0 != PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
      while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
      if (WM_QUIT == msg.message)
        return msg.wParam;
    }
    else {
      idle(hwnd);
    }
  }
}

namespace {
  HDC offscreen_dc = NULL;
  HBITMAP offscreen_bmp = NULL;
  boost::gil::rgb8_pixel_t* offscreen_pixbuf = NULL;

  boost::gil::rgb8_image_t input_image;
  boost::gil::gray8_image_t gray_image;
  boost::gil::rgb8_image_t work_image;
  boost::gil::rgb8_image_t output_image;

  // positions of scan lines
  boost::gil::point2<int> left_scanline;
  boost::gil::point2<int> right_scanline;
  boost::gil::point2<int> top_scanline;
  boost::gil::point2<int> bottom_scanline;

  // distances between image end-points on each scan line
  int left_scan_distance = 0;
  int right_scan_distance = 0;
  int top_scan_distance = 0;
  int bottom_scan_distance = 0;

  // detected flags for each side of image bounding-box
  bool left_detected = false;
  bool right_detected = false;
  bool top_detected = false;
  bool bottom_detected = false;
}

void
init_scanlines()
{
  left_scanline = boost::gil::point2<int>(input_image.width() / 2, input_image.height() / 2);
  right_scanline = top_scanline = bottom_scanline = left_scanline;
}

void
setup(HWND hwnd)
{
  HDC hdc = ::GetDC(hwnd);
  offscreen_dc = ::CreateCompatibleDC(hdc);
  ::ReleaseDC(hwnd, hdc);

  RECT rect;
  ::GetClientRect(hwnd, &rect);

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
  double minimum_intensity = 255;
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

  // setup offscreen buffer
  BITMAPINFO bmi;
  ::ZeroMemory(&bmi, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width(rect);
  bmi.bmiHeader.biHeight = -height(rect);
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 24;
  bmi.bmiHeader.biCompression = BI_RGB;

  offscreen_bmp = ::CreateDIBSection(offscreen_dc, &bmi, DIB_RGB_COLORS, (void**)&offscreen_pixbuf, 0, 0);
  ::SelectObject(offscreen_dc, offscreen_bmp);

  boost::gil::rgb8_view_t offscreen_view = boost::gil::interleaved_view(width(rect), height(rect), offscreen_pixbuf, 3*width(rect));
  boost::gil::resize_view(boost::gil::const_view(work_image), offscreen_view, boost::gil::bilinear_sampler());

  init_scanlines();
}

void
paint(HWND hwnd)
{
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);

  ::BitBlt(hdc, x(ps.rcPaint), y(ps.rcPaint), width(ps.rcPaint), height(ps.rcPaint),
           offscreen_dc, x(ps.rcPaint), y(ps.rcPaint), SRCCOPY);

  EndPaint(hwnd, &ps);
}

std::pair<boost::gil::point2<int>, boost::gil::point2<int> >
detect_vertical_endpoints(boost::gil::point2<int> origin)
{
  return std::make_pair(origin, origin);
}

std::pair<boost::gil::point2<int>, boost::gil::point2<int> >
detect_horizontal_endpoints(boost::gil::point2<int> origin)
{

  return std::make_pair(origin, origin);
}

void
idle(HWND hwnd)
{
  if (!left_detected) {
    boost::gil::point2<int> top, bottom;
    boost::tie(top, bottom) = detect_vertical_endpoints(left_scanline);
    if (top == bottom) {
      left_detected = true;
    }
    else {
      --left_scanline.x;
      left_scanline.y = top.y + (bottom.y - top.y) / 2;
    }
  }
  if (!right_detected) {
    boost::gil::point2<int> top, bottom;
    boost::tie(top, bottom) = detect_vertical_endpoints(right_scanline);
    if (top == bottom) {
      right_detected = true;
    }
    else {
      ++right_scanline.x;
      right_scanline.y = top.y + (bottom.y - top.y) / 2;
    }
  }
  if (!top_detected) {
    boost::gil::point2<int> left, right;
    boost::tie(left, right) = detect_horizontal_endpoints(top_scanline);
    if (left == right) {
      top_detected = true;
    }
    else {
      --top_scanline.y;
      top_scanline.x = left.y + (right.y - left.y) / 2;
    }
  }
  if (!bottom_detected) {
    boost::gil::point2<int> left, right;
    boost::tie(left, right) = detect_horizontal_endpoints(bottom_scanline);
    if (left == right) {
      bottom_detected = true;
    }
    else {
      ++bottom_scanline.y;
      bottom_scanline.x = left.y + (right.y - left.y) / 2;
    }
  }
}

int WINAPI
WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int show_mode)
{
  std::string input_filename = select_input_file(hinst);
  boost::gil::jpeg_read_image(input_filename, input_image);

  if (!register_window_class(hinst, _T("scanner_proto"), window_proc))
    return 0;
  HWND main_window = create_main_window(hinst, _T("scanner_proto"), _T("Scanner Proto"),
                                        input_image.width(), input_image.height());
  ShowWindow(main_window, show_mode);
  UpdateWindow(main_window);
  return message_loop(main_window);
}
