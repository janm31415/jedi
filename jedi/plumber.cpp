#include "plumber.h"

#include "utils.h"

#include <cassert>
#include <fstream>
#include <json.hpp>

#include <jtk/file_utils.h>

namespace {
void read_syntax_from_json(std::map<std::wstring, std::string>& extension_to_executable, std::map<std::wstring, std::string>& prefix_to_executable, const std::string& filename)
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
        if (element.is_object()) {
          for (auto it = element.begin(); it != element.end(); ++it) {
            if (it.key() == "extensions") {
              if (it.value().is_string()) {
                auto exts = break_string(it.value().get<std::string>());
                for (auto e : exts)
                  extension_to_executable[e] = ext_it.key();
              }
            }
            if (it.key() == "prefixes") {
              if (it.value().is_string()) {
                auto prefs = break_string(it.value().get<std::string>());
                for (auto p : prefs)
                  prefix_to_executable[p] = ext_it.key();
              }
            }
          }
        }
      }
    }
    catch (nlohmann::detail::exception e)
    {
    }
    i.close();
  }
}

std::wstring to_lower(const std::wstring& s) {
  std::wstring out = s;
  std::transform(s.begin(), s.end(), out.begin(), [](wchar_t ch){ return std::tolower(ch);});
  return out;
}
} // namesapce

plumber::plumber()
{
  read_syntax_from_json(extension_to_executable, prefix_to_executable, get_file_in_executable_path("plumber.json"));
}

plumber::~plumber()
{
}

std::string plumber::get_executable(const std::string& filename) const {
  std::string exe;
  std::wstring wfilename = to_lower(jtk::convert_string_to_wstring(filename));
  std::string f = jtk::convert_wstring_to_string(wfilename);
  auto ext = jtk::convert_string_to_wstring(jtk::get_extension(f));
  auto it = extension_to_executable.find(ext);
  if (it != extension_to_executable.end())
    return it->second;
  for (const auto& pr : prefix_to_executable) {
    std::wstring pref = wfilename.substr(0, pr.first.length());
    if (pref == pr.first)
      return pr.second;
  }
  return exe;
}
