#ifndef FILENAME_PAIR_HPP
#define FILENAME_PAIR_HPP 1

#include <utility>

class filename_pair : private std::pair<std::string, std::string>
{
private:
  typedef std::pair<std::string, std::string> super_class;

public:
  filename_pair(std::string const& origin,
                std::string const& destination)
    : super_class(origin, destination)
  {}

  std::string const& origin() const { return super_class::first; }
  std::string const& destination() const { return super_class::second; }

  bool origin_exists() const { return boost::filesystem::exists(origin()); }
  bool destination_exists() const { return boost::filesystem::exists(destination()); }
};

#endif
