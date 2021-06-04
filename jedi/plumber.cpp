#include "plumber.h"

#include "utils.h"

#include <cassert>
#include <fstream>
#include <json.hpp>

#include <jtk/file_utils.h>

namespace {
void read_syntax_from_json(std::map<std::string, std::string>& m, const std::string& filename)
{
  nlohmann::json j;
  
  std::ifstream i(filename);
  if (i.is_open())
  {
    try
    {
      i >> j;
      
      for (auto ext_it = j.begin(); ext_it != j.end(); ++ext_it)
      {
        auto element = *ext_it;
        std::string exe;
        if (element.is_string())
          exe = element.get<std::string>();
        auto extensions = break_string(ext_it.key());
        for (const auto& we : extensions)
        {
          std::string e = jtk::convert_wstring_to_string(we);
          m[e] = exe;
        }
      }
    }
    catch (nlohmann::detail::exception e)
    {
    }
    i.close();
  }
}
} // namesapce

plumber::plumber()
{
  read_syntax_from_json(extension_to_executable, get_file_in_executable_path("plumber.json"));
}

plumber::~plumber()
{
}

bool plumber::extension_has_executable(const std::string& ext) const {
  return extension_to_executable.find(ext) != extension_to_executable.end();
}

std::string plumber::get_executable(const std::string& ext) const {
  assert(extension_has_executable(ext));
  return extension_to_executable.find(ext)->second;
}
