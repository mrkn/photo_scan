#define BOOST_TEST_MODULE const_string test
#include <boost/test/unit_test.hpp>

#include <boost/filesystem.hpp>

#include <filename_pair.hpp>

namespace FS = boost::filesystem;

inline void
touch_file(std::string const& filename)
{
  std::ofstream os(filename.c_str());
  os.close();
}

BOOST_AUTO_TEST_CASE(filename_pair_creation_test)
{
  filename_pair fnp("a", "b");
  BOOST_CHECK_EQUAL("a", fnp.origin());
  BOOST_CHECK_EQUAL("b", fnp.destination());
}

BOOST_AUTO_TEST_CASE(filename_pair_existence_test)
{
  filename_pair fnp("a", "b");

  if (FS::exists(fnp.origin()))
    FS::remove_all(fnp.origin());
  if (FS::exists(fnp.destination()))
    FS::remove_all(fnp.destination());

  BOOST_CHECK_EQUAL(false, fnp.origin_exists());
  touch_file(fnp.origin());
  BOOST_CHECK_EQUAL(true, fnp.origin_exists());

  BOOST_CHECK_EQUAL(false, fnp.destination_exists());
  touch_file(fnp.destination());
  BOOST_CHECK_EQUAL(true, fnp.destination_exists());

  FS::remove_all(fnp.origin());
  FS::remove_all(fnp.destination());
}

