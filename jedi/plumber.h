#pragma once

#include <string>
#include <map>
#include <vector>

class plumber
  {
  public:
    plumber();
    ~plumber();

    //returns an empty string if no executable is found
    std::string get_executable(const std::string& filename) const;

  private:
    std::map<std::wstring, std::string> extension_to_executable;
    std::map<std::wstring, std::string> prefix_to_executable;    
  };
