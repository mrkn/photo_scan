#ifndef SCANNER_COMMON_HPP
#define SCANNER_COMMON_HPP 1

#include <boost/filesystem.hpp>

#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>
#include <boost/gil/extension/io/png_dynamic_io.hpp>

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

std::wstring multi_to_wide(std::string const& str, int code = CP_ACP)
{
  int sz = ::MultiByteToWideChar(code, 0, str.c_str(), -1, NULL, 0);
  if (sz == 0)
    return L"";

  std::wstring wstr;
  wstr.resize(sz);
  ::MultiByteToWideChar(code, 0, str.c_str(), -1, &wstr[0], sz);
  return wstr;
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

template<class Image>
inline void
read_image(std::string const& filename, Image& image)
{
  if (boost::filesystem::extension(filename) == std::string(".png"))
    boost::gil::png_read_image(filename, image);
  else if (boost::filesystem::extension(filename) == std::string(".jpg") ||
           boost::filesystem::extension(filename) == std::string(".jpeg"))
    boost::gil::jpeg_read_image(filename, image);
}

#endif // ifndef SCANNER_COMMON_HPP
