#pragma once

#include <string>
#include <map>
#include <vector>

class plumber
  {
  public:
    plumber();
    ~plumber();

    bool extension_has_executable(const std::string& ext) const;
    std::string get_executable(const std::string& ext) const;

  private:
    std::map<std::string, std::string> extension_to_executable;
  };
