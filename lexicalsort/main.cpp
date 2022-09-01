#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <vector>
#define JTK_FILE_UTILS_IMPLEMENTATION
#include "jtk/jtk/file_utils.h"
#define JTK_PIPE_IMPLEMENTATION
#include "jtk/jtk/pipe.h"

int main(int argc, char** argv)
  {
  std::string input = jtk::read_std_input(50);
  std::vector<std::string> lines;
  while (!input.empty())
    {
    auto pos = input.find_first_of('\n');
    std::string line = pos == std::string::npos ? input : input.substr(0, pos);
    input.erase(0, pos == std::string::npos ? pos : pos + 1);
    lines.push_back(line);
    }
  std::sort(lines.begin(), lines.end());
  for (const auto& line : lines)
    {
    std::cout << line << "\n";
    }
  return 0;
  }
