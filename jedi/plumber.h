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
    std::string get_executable_from_extension(const std::string& filename) const;
    std::string get_executable_from_regex(const std::string& expression) const;

  private:
    std::map<std::wstring, std::string> extension_to_executable;
    std::map<std::string, std::string> regex_to_executable;    
  };
