#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#define JTK_FILE_UTILS_IMPLEMENTATION
#include "jtk/jtk/file_utils.h"
#define JTK_PIPE_IMPLEMENTATION
#include "jtk/jtk/pipe.h"

#include "json.hpp"

int main(int argc, char** argv)
  {
  if (argc > 1)
    {
    std::ifstream f(argv[1]);
    if (f.is_open())
      {
      try
        {
        nlohmann::json j;
        j << f;
        f.close();
        std::cout << j.dump(2);
        }
      catch (nlohmann::json::exception e)
        {
        std::cout << e.what();
        }
      }
    else
      std::cout << argv[1] << " cannot be opened.\n";
    }
  else
    {
#if 0
    std::string input = jtk::read_std_input(50);
    std::stringstream str;
    str << input;
    nlohmann::json j;
    j << str;
    std::cout << j.dump(2);
#else 
    std::cout << "Usage: jsonfmt <json file>\n";
#endif
      }
  return 0;
    }